#!/usr/bin/env python3
"""Wrapper around arduino-cli for the Gardener firmware.

--task build    compile the sketch (uses the project local libraries folder)
--task upload   compile and upload, auto-detecting the serial port
--task monitor  open the serial monitor at 115200 baud
--task clean    remove the build output

`--define NAME=VALUE` sets one of the rig's own numbers, and repeats. `PUMP_FLOW_LPM`,
`CHAMBER_VOLUME_L`, `CONTROL_FLOOR_KPA`, `PUMP_DUTY_MAX_PERCENT`, `CONTROL_DEADBAND_KPA` and
`VALVE_NORMALLY_OPEN` default to the reference rig in `libraries/Gardener/src/machine/Machine.h`,
and a build for a different pump, chamber or valve overrides them here: they are written to the
sketch's `rig_options.h`, which every `GARDENER_KNOB_VALID` in the tree reads through
`core/Knob.h`. The name here is the knob's token without its `GARDENER_` prefix.

    python scripts/arduino.py --task upload --port COM5
    python scripts/arduino.py --task build --define PUMP_FLOW_LPM=2.0f --define CHAMBER_VOLUME_L=0.25f
"""

import argparse
import functools
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT_LIBS = ROOT / "libraries"

# The sketch: the device's own composition root, which compiles the libraries under `libraries/`.
SKETCH = ROOT / "firmware" / "gardener"

# The build output. arduino-cli's build path is a subdirectory of it, one per configuration.
BUILD_DIR = SKETCH / "build"

# USBMode=hwcdc puts USB on the chip's USB-Serial-JTAG CDC and CDCOnBoot=cdc maps Serial to it,
# which is what puts the interface on the USB-C socket while leaving the ROM downloader on the same
# port. Without CDCOnBoot Serial is UART0 on GPIO43/44 and the interface is invisible to a USB cable.
DEFAULT_FQBN = ("esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,"
                "FlashSize=4M,PartitionScheme=default")

# Where `--define` writes the rig's numbers. It sits in the sketch because that folder is on the
# include path of every translation unit, and it is generated rather than committed because it is
# one rig's numbers and not the reference one's.
RIG_HEADER = SKETCH / "rig_options.h"

FQBN = DEFAULT_FQBN
PORT = ""
VERBOSE = False
# The rig's own build symbols, as `NAME=VALUE` from `--define`. Empty leaves Machine.h's defaults.
DEFINES = []
# The board's build properties, asked of arduino-cli once per run: the core whose version sets the
# build path a configuration compiles into.
_CONFIGURATION = None


def run(command):
    try:
        completed = subprocess.run(command)
    except FileNotFoundError:
        sys.exit(f"{Path(command[0]).name} not found; install it or put it on PATH")
    if completed.returncode != 0:
        sys.exit(f"{Path(command[0]).name} exited with {completed.returncode}")


def arduino_cli(*arguments):
    if VERBOSE:
        print("arduino-cli " + " ".join(arguments), flush=True)
    run(["arduino-cli", *arguments])


def device_port():
    if PORT:
        return PORT
    try:
        completed = subprocess.run(["arduino-cli", "board", "list", "--format", "json"],
                                   capture_output=True, text=True)
    except FileNotFoundError:
        return ""
    if completed.returncode != 0 or not completed.stdout:
        return ""
    try:
        ports = json.loads(completed.stdout).get("detected_ports") or []
    except json.JSONDecodeError:
        return ""
    for entry in ports:
        for board in entry.get("matching_boards") or []:
            if board.get("fqbn", "").startswith("esp32"):
                return entry["port"]["address"]
    if ports:
        return ports[0]["port"]["address"]
    return ""


@functools.lru_cache(maxsize=None)
def build_properties(fqbn):
    """The board's own build properties, from the same platform tables the compile reads them from.
    Nothing here is inferred from the FQBN string, which says the board rather than its MCU."""
    completed = subprocess.run(["arduino-cli", "board", "details", "-b", fqbn, "--format", "json"],
                               capture_output=True, text=True)
    try:
        properties = json.loads(completed.stdout)["build_properties"]
    except (json.JSONDecodeError, KeyError):
        sys.exit(f"arduino-cli could not describe the board {fqbn}")
    return dict(entry.split("=", 1) for entry in properties)


def configuration():
    global _CONFIGURATION
    if _CONFIGURATION is None:
        properties = build_properties(FQBN)
        _CONFIGURATION = {"core": Path(properties["runtime.platform.path"]).name}
    return _CONFIGURATION


def write_knob_defaults():
    """Write core/knob_defaults.h from the GARDENER_KNOB_VALID calls before anything is compiled."""
    run([sys.executable, str(ROOT / "tools" / "knob.py")])


def write_rig_header():
    """Write `--define`'s numbers where core/Knob.h reads them, instead of into the build options.

    A rig number passed as `build.defines` is part of arduino-cli's options, so every retune made
    arduino-cli throw the whole build path away — every object in it, for a number the library
    cannot see. The sketch folder is already on the include path of every translation unit, so a
    header there reaches the knobs' includers and nothing else.

    The file is only written when its contents change: rewriting it every run would recompile those
    includers every run.
    """
    text = "#pragma once\n" + "".join(_define(symbol) for symbol in DEFINES)
    if not RIG_HEADER.exists() or RIG_HEADER.read_text(encoding="utf-8") != text:
        RIG_HEADER.write_text(text, encoding="utf-8")


def _define(symbol):
    name, _, value = symbol.partition("=")
    token = name if name.startswith("GARDENER_") else "GARDENER_" + name
    return f"#define {token} {value}\n" if value else f"#define {token}\n"


def build_flags():
    """What a compile is made with, as far as the wrapper can change it: the board and the core. A
    rig number is deliberately not in here — that is the point of the header."""
    return "\n".join([FQBN, configuration()["core"]])


def compile_dir():
    """One build path per configuration.

    arduino-cli deletes its build path when the build options differ, and those carry the FQBN and
    the defines, so a second board would otherwise cost the first one's objects and a full build. A
    path apiece keeps both, and `--task clean` is what removes them.
    """
    key = hashlib.sha1(build_flags().encode()).hexdigest()[:8]
    return BUILD_DIR / f"compile-{key}"


def compile_sketch(*extra):
    write_knob_defaults()
    write_rig_header()
    arguments = ["compile", "--fqbn", FQBN, "--libraries", str(PROJECT_LIBS), "--build-path",
                 str(compile_dir())]
    arguments.extend(extra)
    arguments.append(str(SKETCH))
    # arduino-cli prints nothing while it works, so --verbose is what makes a long build show which
    # file it is on. The switch rides along with the script's own echoing.
    if VERBOSE:
        arguments.append("--verbose")
    arduino_cli(*arguments)


def clean():
    """Remove the build output: the object files and the data image beside them."""
    if not BUILD_DIR.is_dir():
        return
    for entry in BUILD_DIR.iterdir():
        if entry.is_dir():
            shutil.rmtree(entry)
        else:
            entry.unlink()
    print(f"{SKETCH.name} build output removed")


def build():
    compile_sketch()


def upload():
    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")
    print(f"uploading {SKETCH.name} to {device}")
    compile_sketch("--upload", "-p", device)


def monitor():
    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")
    print(f"monitoring {SKETCH.name} on {device} at 115200 baud, Ctrl+C to stop")
    try:
        arduino_cli("monitor", "-p", device, "--config", "baudrate=115200")
    except KeyboardInterrupt:
        pass


def main():
    global FQBN, PORT, VERBOSE, DEFINES

    parser = argparse.ArgumentParser(description="Build, upload and monitor the Gardener firmware.")
    parser.add_argument("--task", choices=["build", "upload", "monitor", "clean"],
                        default="build", help="what to do (default: build)")
    parser.add_argument("--port", default="", help="the serial port, auto-detected when omitted")
    parser.add_argument("--fqbn", default=DEFAULT_FQBN, help="the board FQBN to compile for")
    parser.add_argument("--define", action="append", default=[], metavar="NAME=VALUE",
                        help="a build symbol for the rig's own numbers, e.g. PUMP_FLOW_LPM=2.0f; "
                             "repeatable")
    parser.add_argument("--verbose", action="store_true", help="echo each arduino-cli command")
    args = parser.parse_args()

    FQBN = args.fqbn
    PORT = args.port
    VERBOSE = args.verbose
    DEFINES = args.define

    if args.task == "clean":
        clean()
    elif args.task == "build":
        build()
    elif args.task == "upload":
        upload()
    elif args.task == "monitor":
        monitor()


if __name__ == "__main__":
    main()
