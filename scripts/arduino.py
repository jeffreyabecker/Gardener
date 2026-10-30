#!/usr/bin/env python3
"""Wrapper around arduino-cli for the Gardener firmware.

--task build    compile the sketch (uses the project local libraries folder)
--task upload   compile and upload, auto-detecting the serial port
--task data     pack the device's data image and write it to the LittleFS partition
--task monitor  open the serial monitor at 115200 baud
--task libs     install the third party libraries the firmware needs, and stage them for the build
--task archive  compile with the prebuilt archives set aside, which packs them again
--task clean    remove the build output

`--define NAME=VALUE` sets one of the rig's own numbers, and repeats. `PUMP_FLOW_LPM`,
`CHAMBER_VOLUME_L`, `CONTROL_FLOOR_KPA`, `PUMP_DUTY_MAX_PERCENT`, `CONTROL_DEADBAND_KPA` and
`VALVE_NORMALLY_OPEN` default to the reference rig in `libraries/Gardener/src/machine/Machine.h`,
and a build for a different pump, chamber or valve overrides them here: they are written to the
sketch's `rig_options.h`, which only Machine.h's includers read.

    python scripts/arduino.py --task upload --port COM5
    python scripts/arduino.py --task build --define PUMP_FLOW_LPM=2.0f --define CHAMBER_VOLUME_L=0.25f
    python scripts/arduino.py --task data --port COM5
"""

import argparse
import functools
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import archive

ROOT = Path(__file__).resolve().parent.parent
PROJECT_LIBS = ROOT / "libraries"

# The sketch: the device's own composition root, which compiles the libraries under `libraries/`.
SKETCH = ROOT / "firmware" / "gardener"

# The image itself: the files the device holds on LittleFS, beside the sketch rather than inside it,
# because it is the device's contents and not a part of what compiles.
DATA_DIR = ROOT / "firmware" / "data"
# The build output. arduino-cli's build path is a subdirectory of it, one per configuration, and the
# staged third-party libraries another; the data image task keeps littlefs.bin beside them.
BUILD_DIR = SKETCH / "build"

# USBMode=hwcdc puts USB on the chip's USB-Serial-JTAG CDC and CDCOnBoot=cdc maps Serial to it,
# which is what puts the interface on the USB-C socket while leaving the ROM downloader on the same
# port. Without CDCOnBoot Serial is UART0 on GPIO43/44 and the interface is invisible to a USB cable.
#
# FlashSize and PartitionScheme are pinned because --task data writes the LittleFS partition at the
# offset the table names: a different scheme would put the image somewhere else.
DEFAULT_FQBN = ("esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,"
                "FlashSize=4M,PartitionScheme=default")

# The libraries the firmware compiles but never changes: NimBLE is 189 of the build's translation
# units and Berry 41, and they are most of what a full build spends its minutes on. Each is packed
# into an archive once and linked from there afterwards; `precompiled=full` in a
# `library.properties` is what makes arduino-cli look for that archive in `src/{build.mcu}/` and
# skip the sources when it finds one.
#
# Berry is vendored, so its properties and its archive folder are the repository's. NimBLE is
# installed into the sketchbook by `--task libs` and staged from there, because the archive has to
# sit inside a `library.properties` the build answers to rather than inside the user's library.
NIMBLE = "NimBLE-Arduino"
NIMBLE_VERSION = "2.5.1"
STAGED_LIBS = BUILD_DIR / "third_party"

THIRD_PARTY_LIBS = [
    # The BLE transport's stack. The pin is what makes the NimBLE compile options below meaningful.
    f"{NIMBLE}@{NIMBLE_VERSION}",
]

# NimBLE builds only what the contract can use: a peripheral with one connection, no scanning and no
# client role. These reach every compiled file, including the library's own sources, which is where
# nimconfig.h would read them if the library name were the variable.
BLE_DEFINES = " ".join([
    "-DCONFIG_BT_NIMBLE_ROLE_CENTRAL=0",
    "-DCONFIG_BT_NIMBLE_ROLE_OBSERVER=0",
    "-DCONFIG_BT_NIMBLE_ROLE_BROADCASTER=1",
    "-DCONFIG_BT_NIMBLE_ROLE_PERIPHERAL=1",
    "-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=1",
])

# Where `--define` writes the rig's numbers. It sits in the sketch because that folder is on the
# include path of every translation unit, and it is generated rather than committed because it is
# one rig's numbers and not the reference one's.
RIG_HEADER = SKETCH / "rig_options.h"

FQBN = DEFAULT_FQBN
PORT = ""
VERBOSE = False
# The rig's own build symbols, as `NAME=VALUE` from `--define`. Empty leaves Machine.h's defaults.
DEFINES = []
# The board's build properties, asked of arduino-cli once per run: the MCU the archives are named
# for, the compiler and archiver that make them, the core whose version sets most of the flags.
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


def arduino_data():
    configured = os.environ.get("ARDUINO_DIRECTORIES_DATA")
    if configured:
        return Path(configured)
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        sys.exit("LOCALAPPDATA is not set; set ARDUINO_DIRECTORIES_DATA to the Arduino data folder")
    return Path(local_app_data) / "Arduino15"


def newest(paths):
    """The newest copy is the one arduino-cli builds with, and the version is in the path."""
    return max(paths, key=str, default=None)


def find_tool(esp32_tools, folder, name):
    return newest((esp32_tools / folder).rglob(name))


def spiffs_partition(table):
    """The offset and size come from the table the build pins, so the image lands on the partition
    the firmware mounts rather than on a number copied into this script."""
    for line in table.read_text(encoding="utf-8").splitlines():
        if re.match(r"^\s*spiffs\s*,", line):
            cells = [cell.strip() for cell in line.split(",")]
            return f"0x{int(cells[3], 16):x}", int(cells[4], 16)
    sys.exit(f"no spiffs partition in {table}")


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
        compiler = Path(properties["compiler.path"])
        toolchain = subprocess.run([str(compiler / properties["compiler.c.cmd"]), "-dumpversion"],
                                   capture_output=True, text=True).stdout.strip()
        _CONFIGURATION = {
            "mcu": properties["build.mcu"],
            # gcc-ar rather than ar: the toolchain's own wrapper is what writes an index the
            # linker can read.
            "ar": str(compiler / properties["compiler.ar.cmd"]),
            "toolchain": toolchain or properties["compiler.path"],
            "core": Path(properties["runtime.platform.path"]).name,
        }
    return _CONFIGURATION


def write_rig_header():
    """Write `--define`'s numbers where Machine.h reads them, instead of into the build options.

    A rig number passed as `build.defines` is part of arduino-cli's options, so every retune made
    arduino-cli throw the whole build path away — NimBLE's 189 objects and Berry's 41 among them,
    for a number neither library can see. The sketch folder is already on the include path of every
    translation unit, so a header there reaches Machine.h's includers and nothing else.

    The file is only written when its contents change: rewriting it every run would recompile those
    includers every run.
    """
    text = "#pragma once\n" + "".join(_define(symbol) for symbol in DEFINES)
    if not RIG_HEADER.exists() or RIG_HEADER.read_text(encoding="utf-8") != text:
        RIG_HEADER.write_text(text, encoding="utf-8")


def _define(symbol):
    name, _, value = symbol.partition("=")
    return f"#define {name} {value}\n" if value else f"#define {name}\n"


def archive_flags():
    """What a compile of the archived libraries is made with, as far as the wrapper can change it:
    the board, the defines every unit sees, and the core. A rig number is deliberately not in here —
    that is the point of the header, and why retuning a rig keeps the archives."""
    return "\n".join([FQBN, BLE_DEFINES, configuration()["core"]])


def compile_dir():
    """One build path per configuration.

    arduino-cli deletes its build path when the build options differ, and those carry the FQBN and
    the defines, so a second board or panel would otherwise cost the first one's objects and a full
    build. A path apiece keeps both, and `--task clean` is what removes them.
    """
    key = hashlib.sha1(archive_flags().encode()).hexdigest()[:8]
    return BUILD_DIR / f"compile-{key}"


def archived_libraries():
    """The libraries that get an archive, as (name, folder): the archive is `lib{name}.a` inside the
    folder's `src/{mcu}/`."""
    libraries = [("berry", PROJECT_LIBS / "berry"), (NIMBLE, STAGED_LIBS / NIMBLE)]
    return [(name, library) for name, library in libraries if library.is_dir()]


def drop_stale_archives():
    build = configuration()
    for name, library in archived_libraries():
        reason = archive.verified(library, name, build["mcu"], build["toolchain"],
                                  archive_flags())
        if reason:
            print(f"dropping the {name} archive: {reason}")


def pack_archives():
    """Pack what this build compiled and has no archive for. The objects are the build's own, made
    with the flags the archive will be linked with, so the two cannot disagree."""
    build = configuration()
    for name, library in archived_libraries():
        if archive.archive_path(library, name, build["mcu"]).exists():
            continue
        objects = compile_dir() / "libraries" / library.name
        if not objects.is_dir() or not any(objects.rglob("*.o")):
            continue
        archive.pack(library, name, build["mcu"], objects, build["ar"], build["toolchain"],
                     archive_flags())
        print(f"packed the {name} archive")


def library_folders():
    """The project's libraries first, then the staged copies. arduino-cli takes `--libraries` more
    than once, and a header found in an earlier folder's library is the one it resolves."""
    folders = [PROJECT_LIBS]
    if STAGED_LIBS.is_dir():
        folders.append(STAGED_LIBS)
    return folders


def compile_sketch(*extra):
    write_rig_header()
    drop_stale_archives()
    arguments = ["compile", "--fqbn", FQBN, "--build-property", f"build.defines={BLE_DEFINES}",
                 "--build-path", str(compile_dir())]
    for folder in library_folders():
        arguments.extend(["--libraries", str(folder)])
    arguments.extend(extra)
    arguments.append(str(SKETCH))
    # arduino-cli prints nothing while it works, so --verbose is what makes a long build show which
    # file it is on. The switch rides along with the script's own echoing.
    if VERBOSE:
        arguments.append("--verbose")
    arduino_cli(*arguments)
    pack_archives()


def user_libraries():
    """Where `arduino-cli lib install` puts a library: the sketchbook's `libraries` folder."""
    completed = subprocess.run(["arduino-cli", "config", "get", "directories.user"],
                               capture_output=True, text=True)
    if completed.returncode != 0 or not completed.stdout.strip():
        sys.exit("arduino-cli does not say where the sketchbook is; check its configuration")
    return Path(completed.stdout.strip()) / "libraries"


def stage(name):
    """Copy the installed library where the build can own it.

    The archive has to sit inside a `library.properties` that declares `precompiled=full`, and the
    installed copy is the user's to keep. `src` and the properties are all arduino-cli reads, so the
    examples and the docs stay behind; the copy is made again from scratch, so a version bump cannot
    leave the old one's sources or archive in place.
    """
    installed = user_libraries() / name
    if not (installed / "library.properties").is_file():
        sys.exit(f"{name} is not installed in {installed}; --task libs installs it first")
    staged = STAGED_LIBS / name
    shutil.rmtree(staged, ignore_errors=True)
    staged.mkdir(parents=True)
    shutil.copytree(installed / "src", staged / "src")
    properties = installed.joinpath("library.properties").read_text(encoding="utf-8")
    (staged / "library.properties").write_text(properties.rstrip("\n") + "\nprecompiled=full\n",
                                               encoding="utf-8")
    print(f"staged {name} in {staged}")


def install_libs():
    for library in THIRD_PARTY_LIBS:
        print(f"installing {library}")
        arduino_cli("lib", "install", library)
    stage(NIMBLE)


def clean():
    """Remove the build output. The staged libraries stay: they are a copy of an installed library
    rather than output, and the archive beside one is a rebuild only the sources can pay for."""
    if not BUILD_DIR.is_dir():
        return
    for entry in BUILD_DIR.iterdir():
        if entry == STAGED_LIBS:
            continue
        if entry.is_dir():
            shutil.rmtree(entry)
        else:
            entry.unlink()
    print(f"{SKETCH.name} build output removed, the staged libraries kept")


def archive_task():
    """Pack the archives again from a fresh compile: a build only packs one that is missing, so the
    ones on disk go first."""
    for name, library in archived_libraries():
        archive.drop(library, name, configuration()["mcu"])
    compile_sketch()


def build():
    compile_sketch()


def upload():
    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")
    print(f"uploading {SKETCH.name} to {device}")
    compile_sketch("--upload", "-p", device)


def upload_data():
    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")

    if not DATA_DIR.is_dir():
        DATA_DIR.mkdir(parents=True)
        print(f"created {DATA_DIR}; put the files to upload there")

    # The ESP32 core ships both tools, and the newest copy is the one arduino-cli builds with.
    data_root = arduino_data()
    esp32_tools = data_root / "packages" / "esp32" / "tools"
    mklittlefs = find_tool(esp32_tools, "mklittlefs", "mklittlefs.exe")
    esptool = find_tool(esp32_tools, "esptool_py", "esptool.exe")
    if not mklittlefs:
        sys.exit(f"mklittlefs.exe not found under {esp32_tools}")
    if not esptool:
        sys.exit(f"esptool.exe not found under {esp32_tools}")

    core_root = data_root / "packages" / "esp32" / "hardware" / "esp32"
    cores = [entry for entry in core_root.iterdir() if entry.is_dir()] if core_root.is_dir() else []
    if not cores:
        sys.exit(f"no ESP32 core under {core_root}")
    core = max(cores, key=lambda entry: entry.name)
    offset, size = spiffs_partition(core / "tools" / "partitions" / "default.csv")

    image = BUILD_DIR / "littlefs.bin"
    image.parent.mkdir(parents=True, exist_ok=True)

    print(f"packing {DATA_DIR}")
    run([str(mklittlefs), "-c", str(DATA_DIR), "-b", "4096", "-p", "256", "-s", str(size),
         str(image)])

    print(f"writing {image} to {device} at {offset}")
    run([str(esptool), "--chip", "esp32s3", "--port", device, "--baud", "921600",
         "--before", "default_reset", "--after", "hard_reset", "write_flash", offset, str(image)])


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
    parser.add_argument("--task",
                        choices=["build", "upload", "data", "monitor", "libs", "archive", "clean"],
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

    if args.task == "libs":
        install_libs()
    elif args.task == "clean":
        clean()
    elif args.task == "build":
        build()
    elif args.task == "upload":
        upload()
    elif args.task == "data":
        upload_data()
    elif args.task == "monitor":
        monitor()
    elif args.task == "archive":
        archive_task()


if __name__ == "__main__":
    main()
