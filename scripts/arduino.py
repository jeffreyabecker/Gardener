#!/usr/bin/env python3
"""Wrapper around arduino-cli for the Gardener firmware.

--task build    compile the sketch (uses the project local libraries folder)
--task upload   compile and upload, auto-detecting the serial port
--task data     pack firmware/gardener/data and write it to the LittleFS partition
--task monitor  open the serial monitor at 115200 baud
--task libs     install the third party libraries the firmware needs
--task clean    remove the build output

    python scripts/arduino.py --task upload --port COM5
    python scripts/arduino.py --task data --port COM5
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKETCH = ROOT / "firmware" / "gardener"
PROJECT_LIBS = ROOT / "libraries"
DATA_DIR = SKETCH / "data"
BUILD_DIR = SKETCH / "build"

# USBMode=hwcdc puts USB on the chip's USB-Serial-JTAG CDC and CDCOnBoot=cdc maps Serial to it,
# which is what puts the interface on the USB-C socket while leaving the ROM downloader on the same
# port. Without CDCOnBoot Serial is UART0 on GPIO43/44 and the interface is invisible to a USB cable.
#
# FlashSize and PartitionScheme are pinned because --task data writes the LittleFS partition at the
# offset the table names: a different scheme would put the image somewhere else.
DEFAULT_FQBN = ("esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,"
                "FlashSize=4M,PartitionScheme=default")

THIRD_PARTY_LIBS = [
    "Adafruit SH110X",
    "Adafruit BusIO",
    "Adafruit GFX Library",
    # The BLE transport's stack. docs/plans/ble-control.md was written against this version, and
    # the pin is what makes the NimBLE compile options below meaningful.
    "NimBLE-Arduino@2.5.1",
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

FQBN = DEFAULT_FQBN
PORT = ""
VERBOSE = False


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


def compile_sketch(*extra):
    arduino_cli("compile", "--fqbn", FQBN, "--build-property", f"build.defines={BLE_DEFINES}",
                "--libraries", str(PROJECT_LIBS), *extra, str(SKETCH))


def install_libs():
    for library in THIRD_PARTY_LIBS:
        print(f"installing {library}")
        arduino_cli("lib", "install", library)


def clean():
    shutil.rmtree(BUILD_DIR, ignore_errors=True)
    print("build output removed")


def build():
    compile_sketch()


def upload():
    device = device_port()
    if not device:
        sys.exit("no serial port found, pass --port COMx")
    print(f"uploading to {device}")
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
    print(f"monitoring {device} at 115200 baud, Ctrl+C to stop")
    try:
        arduino_cli("monitor", "-p", device, "--config", "baudrate=115200")
    except KeyboardInterrupt:
        pass


def main():
    global FQBN, PORT, VERBOSE

    parser = argparse.ArgumentParser(description="Build, upload and monitor the Gardener firmware.")
    parser.add_argument("--task", choices=["build", "upload", "data", "monitor", "libs", "clean"],
                        default="build", help="what to do (default: build)")
    parser.add_argument("--port", default="", help="the serial port, auto-detected when omitted")
    parser.add_argument("--fqbn", default=DEFAULT_FQBN, help="the board FQBN to compile for")
    parser.add_argument("--verbose", action="store_true", help="echo each arduino-cli command")
    args = parser.parse_args()

    FQBN = args.fqbn
    PORT = args.port
    VERBOSE = args.verbose

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


if __name__ == "__main__":
    main()
