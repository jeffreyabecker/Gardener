#!/usr/bin/env python3
"""Compile and run the Berry data image on the host.

    python tools/berry/check.py              # compile, load and drive
    python tools/berry/check.py --compile    # compile and load only
    python tools/berry/check.py --no-cache   # rebuild everything
    python tools/berry/check.py --art default
    python tools/berry/check.py --art interval --seconds 30
    python tools/berry/check.py --scene browser
    python tools/berry/check.py --tests      # run the test suites
    python tools/berry/check.py --test interval
    python tools/berry/check.py --memory     # what the image holds at boot, file by file

The check is the firmware, not a model of it: the interpreter's host side, the panel's registry,
overlay, settings and control law are compiled here against the vendored VM, with a bitmap for a
glass, a chamber for a rig and an in-memory data image. A page therefore runs on the code the
device runs, and the driver walks the image's own routines.

--art runs one routine and saves what it drew, using the Gfx library the device compiles: an
animated GIF of every beat. --scene does the same for one of the device's own scenes: home or
browser.
--tests saves a GIF per case that ran.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parent
ROOT = TOOL.parents[1]
BERRY = ROOT / "libraries" / "berry"
GARDENER = ROOT / "libraries" / "Gardener"
GFX = ROOT / "libraries" / "Gfx"
DATA = ROOT / "firmware" / "data"
HOST = TOOL / "host"
BUILD = TOOL / "build"
EXE = BUILD / ("berrycheck.exe" if os.name == "nt" else "berrycheck")

# What the check compiles is discovered, not listed, so a source that moves and a file a library
# gains or loses is not an edit here. Gfx is the whole library, and the check is that code. Gardener
# is the part of it a panel build compiles on a host — the sources that name no board header, the
# interpreter's host side under routines/ among them — which is the selection tools/unit/check.py
# makes in the other direction.
PLATFORM_INCLUDES = ("<FS.h>", "<SD.h>", "<Wire.h>", "<SPI.h>", "<NimBLE", "freertos/",
                     "<TFT_eSPI", "<WiFi", "<Preferences.h>")


def board_free(source):
    text = source.read_text(encoding="utf-8", errors="replace")
    return not any(name in text for name in PLATFORM_INCLUDES)


GFX_SOURCES = sorted((GFX / "src").rglob("*.cpp"))

FIRMWARE_SOURCES = [path for path in sorted((GARDENER / "src").rglob("*.cpp")) if board_free(path)]

# The parts the panel's pieces are made of here: a chamber and its three parts, a counted piezo, a
# full battery, the data image in memory and a log buffer. The glass is the shared recording
# display.
HOST_SOURCES = [
    HOST / "HarnessModule.cpp",
    HOST / "HarnessSession.cpp",
    HOST / "MemoryFiles.cpp",
]

MAIN_SOURCES = [TOOL / "main.cpp", TOOL / "entry.cpp", *HOST_SOURCES]

# What a host tool shares with the others: the Arduino API, the clock, the recording display, the
# gesture strip and the frame writer.
SHARED = ROOT / "tools" / "host"
SHARED_SOURCES = [
    SHARED / "arduino" / "Clock.cpp",
    SHARED / "capture" / "CaptureDisplay.cpp",
    SHARED / "capture" / "GestureStrip.cpp",
    SHARED / "capture" / "GifWriter.cpp",
]

# The API's own sources, and the two a board's C library would provide: a device's core library links
# them, so a host tool that compiles the API's headers has to compile these as well.
API = SHARED / "arduino-core-api"
API_SOURCES = [API / "api" / name for name in ("String.cpp", "Print.cpp", "Stream.cpp", "Common.cpp")]
API_SOURCES += [API / "extras" / name for name in ("itoa.cpp", "dtostrf.cpp")]


def compiler(kind):
    preferred = os.environ.get("CXX" if kind == "c++" else "CC")
    names = ("g++", "clang++") if kind == "c++" else ("cc", "gcc", "clang")
    for name in (preferred, *names):
        if name and shutil.which(name):
            return name
    sys.exit(f"no {kind} compiler found; set {'CXX' if kind == 'c++' else 'CC'}")


def run(command):
    if args.verbose:
        print(" ".join(str(part) for part in command), flush=True)
    completed = subprocess.run(command)
    if completed.returncode != 0:
        sys.exit(f"{Path(str(command[0])).name} exited with {completed.returncode}")


def slug(path):
    return "_".join(Path(path).resolve().parts[-4:])


def build(cc, cxx):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    includes = []
    for path in (HOST, BERRY / "src", BERRY / "generate", GFX / "src",
                 GARDENER / "src", SHARED / "arduino", SHARED / "capture", API):
        includes.extend(["-I", str(path)])
    # The API's folder holds String.h, which a case-insensitive filesystem would otherwise hand to
    # #include <string.h> in place of the C library's.
    includes.extend(["-idirafter", str(SHARED / "arduino-core-api" / "api")])

    built = []

    # A header edit changes every object that saw it, and an object built against the old one is a
    # crash rather than an error: one that embeds a class's layout by value corrupts the heap when
    # the class grows. The cache is keyed on source files, so the newest header is the other half.
    header_mtime = 0.0
    if not args.no_cache:
        for directory in (HOST, BERRY / "src", BERRY / "generate",
                          GFX / "src", GARDENER / "src"):
            for header in directory.rglob("*.h"):
                header_mtime = max(header_mtime, header.stat().st_mtime)

    def compile(source, std, compiler_path):
        obj = objects / (slug(source) + ".o")
        if (args.no_cache or not obj.exists()
                or obj.stat().st_mtime < Path(source).stat().st_mtime
                or obj.stat().st_mtime < header_mtime):
            run([compiler_path, std, "-O0", *includes, "-c", str(source), "-o", str(obj)])
        built.append(obj)

    for source in sorted((BERRY / "src").glob("*.c")):
        compile(source, "-std=gnu11", cc)
    for source in (*MAIN_SOURCES, *SHARED_SOURCES, *API_SOURCES, *FIRMWARE_SOURCES,
                   *GFX_SOURCES):
        compile(source, "-std=c++17", cxx)

    # Static, because the first libstdc++ on PATH is usually a different MinGW's and the exe then
    # refuses to start with a missing entry point rather than a diagnostic. The objects go in a
    # response file: the whole of three libraries is more than one command line takes.
    response = BUILD / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in built), encoding="utf-8")
    run([cxx, "-static", f"@{response}", "-o", str(EXE), "-lm"])


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compile", action="store_true", help="stop after compiling and loading")
parser.add_argument("--no-cache", action="store_true", help="rebuild every object")
parser.add_argument("--verbose", action="store_true", help="show compiler commands and device log")
parser.add_argument(
    "--memory",
    action="store_true",
    help="measure the image's own boot — the heap at each file — with no driver or suites loaded",
)
parser.add_argument("--data", default=str(DATA), help="the data image to check")
parser.add_argument("--art", metavar="NAME", help="draw that routine and save it as a GIF animation")
parser.add_argument(
    "--scene",
    metavar="NAME",
    help="draw that device scene — home or browser — and save it as a GIF animation",
)
parser.add_argument(
    "--seconds",
    type=int,
    default=0,
    help="with --art or --scene, run this much virtual time before saving",
)
parser.add_argument(
    "--out",
    help="where the GIF animation goes (default: build/<name>.gif)",
)
parser.add_argument(
    "--ceiling-kpa",
    type=float,
    help="the deepest depth the model chamber reaches, in kPa (default 70)",
)
parser.add_argument("--tests", action="store_true", help="run the test suites in tools/berry/tests")
parser.add_argument(
    "--test",
    metavar="NAME",
    help="run only the tests whose full name contains NAME (implies --tests)",
)
args = parser.parse_args()

build(compiler("c"), compiler("c++"))

command = [
    str(EXE),
    "--data",
    str(Path(args.data).resolve()),
    "--harness",
    str(TOOL / "harness.be"),
    "--driver",
    str(TOOL / "driver.be"),
]
if args.compile:
    command.append("--compile-only")
if args.verbose:
    command.append("--verbose")
if args.memory:
    command.append("--memory")
if args.ceiling_kpa is not None:
    command.extend(["--ceiling-kpa", str(args.ceiling_kpa)])
if args.art:
    out = Path(args.out) if args.out else BUILD / f"{args.art}.gif"
    command.extend(
        [
            "--art",
            args.art,
            "--seconds",
            str(args.seconds),
            "--out",
            str(out.resolve()),
        ]
    )
if args.scene:
    out = Path(args.out) if args.out else BUILD / f"{args.scene}.gif"
    command.extend(
        ["--scene", args.scene, "--seconds", str(args.seconds), "--out", str(out.resolve())]
    )
if args.tests or args.test:
    tests_dir = TOOL / "tests"
    command.extend(["--suite", str(tests_dir / "suite.be")])
    for file in sorted(tests_dir.glob("*.test.be")):
        command.extend(["--test-file", str(file)])
    if args.test:
        command.extend(["--test", args.test])
    # The stem the runner names a per-case GIF from; a non-GIF --out is meaningless here.
    stem = Path(args.out) if args.out else BUILD / "runs.gif"
    command.extend(["--out", str(stem.resolve())])
if args.verbose:
    print(" ".join(command), flush=True)
sys.exit(subprocess.run(command).returncode)
