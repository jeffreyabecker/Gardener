#!/usr/bin/env python3
"""Wrapper around arduino-cli for the Gardener firmware.

--task build    compile the sketch
--task upload   compile, upload, then write the page image
--task data     build the LittleFS image from the sketch's `data` folder and write it
--task monitor  open the serial monitor at 115200 baud
--task clean    remove the build output

The page is not in the firmware. Everything under `firmware/gardener/data/www` becomes the board's
LittleFS partition, and a GET is the matching path inside that tree, so a stylesheet or a second
page is a file in the folder rather than a rebuild. `--task data` builds that image with
`mklittlefs` and writes it with `esptool` at the offset the board's own partition table gives it;
the write is skipped while the folder is unchanged, because the partition also holds the network
the board stored and rewriting it would clear that. A page edit does rewrite it, and the board
comes back as its setup access point.

A profile in `sketch.yaml` records the build's options under `defines:`, one `NAME: value` per
line, so a pump's measured floor or a normally-open valve sits in the file that describes the build
rather than in a command someone has to remember. A name is a knob's token without its `GARDENER_`
prefix, and a knob the profile leaves out keeps the default beside its `GARDENER_KNOB_VALID`. The
options are written to the sketch's `options.h`, which every `GARDENER_KNOB_VALID` in the tree
reads through `core/Knob.h`. `--define NAME=VALUE` overrides one for a single run, and repeats.

    python scripts/arduino.py --task upload --port COM5
    python scripts/arduino.py --task build --define CONTROL_FLOOR_KPA=55.0f
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

# The sketch: the device's own composition root, which compiles the libraries under `libraries/`.
SKETCH = ROOT / "firmware" / "gardener"

# The build output. arduino-cli's build path is a subdirectory of it, one per configuration.
BUILD_DIR = SKETCH / "build"

# The page tree, and the image built from it. It is one partition with the round trip's other half:
# the network the board stores is a file at the filesystem's root, so an image write clears it.
DATA = SKETCH / "data"
IMAGE = BUILD_DIR / "littlefs.bin"
IMAGE_STAMP = BUILD_DIR / "littlefs.stamp"

# The geometry mklittlefs is given, in the units a NOR flash partition has: 4 KB blocks and the
# 256-byte pages the tool expects to write.
FLASH_BLOCK = 4096
FLASH_PAGE = 256

# The sketch profile the wrapper compiles unless one is named, from sketch.yaml: it pins the board,
# the platform and the tree's own library, so a build is the tree's choice rather than whatever the
# machine's library manager was left holding. The `ble` profile adds the Bluetooth stack.
PROFILE = "no-ble"

# Where the build's options are written. The header sits in the sketch because that folder is on the
# include path of every translation unit, and it is generated rather than committed because it is
# one build's options and not the defaults the library ships.
OPTIONS_HEADER = SKETCH / "options.h"

# The sketch project file, read for the profile's board and its own `defines:` — the second block
# being one arduino-cli ignores, which is why the options a build is made with are recorded here
# rather than on a command line.
SKETCH_YAML = SKETCH / "sketch.yaml"

PORT = ""
VERBOSE = False
# `--define`'s symbols, as `NAME=VALUE`. The selected profile's `defines:` are read alongside them
# and a symbol here wins, so an override lasts the run rather than the build.
DEFINES = []


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
        for detected in entry.get("matching_boards") or []:
            if detected.get("fqbn", "").startswith("esp32"):
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


def write_knob_defaults():
    """Write core/knob_defaults.h from the GARDENER_KNOB_VALID calls before anything is compiled."""
    run([sys.executable, str(ROOT / "tools" / "knob.py")])


def sketch_project(profile):
    """What sketch.yaml says about the named profile: the board it builds for, and its options.

    The board comes from the profile, since that is the one the compile resolves; a project with no
    such profile offers its `default_fqbn` instead. arduino-cli's model of this file has no field
    for a preprocessor symbol and no command prints a profile back, so both are read here: two
    spaces per level place `profiles:`, the profile's name, then `fqbn:` and its `defines:` beneath.
    """
    if not SKETCH_YAML.exists():
        sys.exit(f"{SKETCH_YAML} is missing; it is where the board and the build's options are")
    default, fqbn, symbols = "", "", []
    section, current, recorded = "", "", False
    for line in SKETCH_YAML.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip())
        key, _, value = line.strip().partition(":")
        value = value.strip().strip("\"'")
        if indent == 0:
            section, current, recorded = key, "", False
            if key == "default_fqbn":
                default = value
        elif section == "profiles" and indent == 2:
            current, recorded = key, False
        elif current == profile and indent == 4:
            recorded = key == "defines"
            if key == "fqbn":
                fqbn = value
            elif recorded and value:
                sys.exit(f"{SKETCH_YAML.name}: the {profile} profile's defines are one "
                         f"NAME: value per line, not {value}")
        elif recorded and indent >= 6:
            symbols.append(f"{key}={value}" if value else key)
    if not (fqbn or default):
        sys.exit(f"{SKETCH_YAML.name} gives no FQBN for the {profile} profile")
    return fqbn or default, symbols


def board():
    """The FQBN the selected profile builds for: the board the compile resolves, so the properties
    the data image is written with cannot describe a different one."""
    return sketch_project(PROFILE)[0]


def token(name):
    """The preprocessor token behind a knob's name: the profile's block and `--define` both drop
    the `GARDENER_` prefix the tree's knobs carry."""
    return name if name.startswith("GARDENER_") else "GARDENER_" + name


def build_options():
    """The options a build is made with, one per token: the profile's, then `--define`'s on top.

    A token appears once, because the preprocessor warns about a second #define of the same name.
    """
    numbers = {}
    for symbol in sketch_project(PROFILE)[1] + DEFINES:
        name, _, value = symbol.partition("=")
        numbers[token(name)] = value
    return numbers


def write_options():
    """Write the build's options where core/Knob.h reads them, instead of into arduino-cli's.

    An option passed as `build.defines` is part of arduino-cli's options, so every change made
    arduino-cli throw the whole build path away — every object in it, for a value the library
    cannot see. The sketch folder is already on the include path of every translation unit, so a
    header there reaches the knobs' includers and nothing else.

    The file is only written when its contents change: rewriting it every run would recompile those
    includers every run.
    """
    text = "#pragma once\n" + "".join(
        f"#define {name} {value}\n" if value else f"#define {name}\n"
        for name, value in build_options().items())
    if not OPTIONS_HEADER.exists() or OPTIONS_HEADER.read_text(encoding="utf-8") != text:
        OPTIONS_HEADER.write_text(text, encoding="utf-8")


def compile_dir():
    """One build path per configuration, named by the profile, the board and the core.

    arduino-cli deletes its build path when the build options differ, and those carry the board and
    the profile's libraries, so a second board would otherwise cost the first one's objects and a
    full build. A path apiece keeps both, and `--task clean` is what removes them. An option is
    deliberately not in the name — that is the point of the header.
    """
    fqbn = board()
    core = Path(build_properties(fqbn)["runtime.platform.path"]).name
    key = hashlib.sha1("\n".join([PROFILE, fqbn, core]).encode()).hexdigest()[:8]
    return BUILD_DIR / f"compile-{key}"

def compile_sketch(*extra):
    write_knob_defaults()
    write_options()
    arguments = ["compile", "--profile", PROFILE, "--build-path", str(compile_dir())]
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


def tool(directory, *names):
    """A core tool's executable. The core ships `mklittlefs.exe` and `esptool.exe` on Windows and
    the bare names elsewhere, so the caller passes both and the first that exists wins."""
    for name in names:
        path = Path(directory) / name
        if path.exists():
            return str(path)
    sys.exit(f"none of {' '.join(names)} is in {directory}")


def partition(properties):
    """The spiffs row of the board's own partition table: the offset the image is written at and the
    size the tool is given, so neither is a number this script invented."""
    table = (Path(properties["runtime.platform.path"]) / "tools" / "partitions"
             / f"{properties['build.partitions']}.csv")
    for line in table.read_text(encoding="utf-8").splitlines():
        fields = [field.strip() for field in line.split(",")]
        if len(fields) >= 5 and fields[1] == "data" and fields[2] == "spiffs":
            return int(fields[3], 0), int(fields[4], 0)
    sys.exit(f"{table.name} has no spiffs partition to put the page in")


def image_stamp():
    """What an image is made of, as one hash: the tree's files and the port it was written to. A
    second board is a different port and so gets its own write."""
    digest = hashlib.sha1()
    for path in sorted(DATA.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(DATA)).encode())
            digest.update(path.read_bytes())
    digest.update((PORT or "").encode())
    return digest.hexdigest()


def write_image():
    """Build the page image and write it, unless the tree is what the last write left on the board.

    Writing the partition clears the network the board stored beside the page, so a firmware-only
    upload must not do it; a page edit must, and the board then comes up as its access point.
    """
    if not DATA.is_dir():
        sys.exit(f"{DATA} is missing; the page tree is what the image is built from")

    stamp = image_stamp()
    if IMAGE.exists() and IMAGE_STAMP.exists() and \
            IMAGE_STAMP.read_text(encoding="utf-8").strip() == stamp:
        print("page tree unchanged, image not written")
        return

    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")

    properties = build_properties(board())
    offset, size = partition(properties)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    run([tool(properties["runtime.tools.mklittlefs.path"], "mklittlefs.exe", "mklittlefs"),
         "-c", str(DATA), "-b", str(FLASH_BLOCK), "-p", str(FLASH_PAGE), "-s", hex(size),
         str(IMAGE)])

    print(f"writing the page tree to {device} at {hex(offset)}")
    run([tool(properties["runtime.tools.esptool_py.path"], "esptool.exe", "esptool"),
         "--chip", properties["build.mcu"], "--port", device, "--baud", "921600",
         "--before", "default_reset", "--after", "hard_reset", "write_flash", "-z",
         "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep",
         hex(offset), str(IMAGE)])
    IMAGE_STAMP.write_text(stamp + "\n", encoding="utf-8")


def upload():
    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")
    print(f"uploading {SKETCH.name} to {device}")
    compile_sketch("--upload", "-p", device)
    write_image()


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
    global PROFILE, PORT, VERBOSE, DEFINES

    parser = argparse.ArgumentParser(description="Build, upload and monitor the Gardener firmware.")
    parser.add_argument("--task", choices=["build", "upload", "data", "monitor", "clean"],
                        default="build", help="what to do (default: build)")
    parser.add_argument("--profile", default=PROFILE,
                        help=f"the sketch profile to compile (default: {PROFILE})")
    parser.add_argument("--port", default="", help="the serial port, auto-detected when omitted")
    parser.add_argument("--define", action="append", default=[], metavar="NAME=VALUE",
                        help="one of the build's options for this run, overriding the profile's "
                             "defines, e.g. CONTROL_FLOOR_KPA=55.0f; repeatable")
    parser.add_argument("--verbose", action="store_true", help="echo each arduino-cli command")
    args = parser.parse_args()

    PROFILE = args.profile
    PORT = args.port
    VERBOSE = args.verbose
    DEFINES = args.define

    if args.task == "clean":
        clean()
    elif args.task == "build":
        compile_sketch()
    elif args.task == "upload":
        upload()
    elif args.task == "data":
        write_image()
    elif args.task == "monitor":
        monitor()


if __name__ == "__main__":
    main()
