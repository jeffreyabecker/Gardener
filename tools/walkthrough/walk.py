#!/usr/bin/env python3
"""Compile and run a scripted GIF walkthrough of the panel.

    python tools/walkthrough/walk.py tools/walkthrough/sample.walk
    python tools/walkthrough/walk.py my.walk --out build/my.gif --delay-cs 20 --scale 3
    python tools/walkthrough/walk.py my.walk --no-cache --verbose

The tool is the firmware, not a model of it: the interpreter's host side and the panel's registry,
overlay, settings and control law are compiled here against the vendored VM, with a bitmap for a
glass and a chamber for a rig, over the same session the Berry check drives. What it runs is the
command file — a load, a send, a wait, a set-rate and an emit, one per line — and what it writes is
the animated GIF of the frames the beats drew.

Nothing in the arguments, the output or the exit code is about the image's files or the routines a
check drives: no harness, no driver and no suites. The build lands in tools/walkthrough/build/,
which is ignored, so a run after the first compiles only what changed.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parent
TOOLS = TOOL.parent
ROOT = TOOLS.parent
BERRY = ROOT / "libraries" / "berry"
GARDENER = ROOT / "libraries" / "Gardener"
GFX = ROOT / "libraries" / "Gfx"
DATA = ROOT / "firmware" / "data"
SHARED = TOOLS / "host"
SESSION = SHARED / "session"
API = SHARED / "arduino-core-api"
BUILD = TOOL / "build"
EXE = BUILD / ("walkthrough.exe" if os.name == "nt" else "walkthrough")

# What the tool compiles is discovered, not listed. Gfx is the whole library and Gardener is the part
# of it a panel build compiles on a host — the sources that name no board header, the interpreter's
# host side under routines/ among them. The session, the capture and the writer are the host's own
# library, shared with the Berry check rather than copied.
PLATFORM_INCLUDES = ("<FS.h>", "<SD.h>", "<Wire.h>", "<SPI.h>", "<NimBLE", "freertos/",
                     "<TFT_eSPI", "<WiFi", "<Preferences.h>")


def board_free(source):
    text = source.read_text(encoding="utf-8", errors="replace")
    return not any(name in text for name in PLATFORM_INCLUDES)


GFX_SOURCES = sorted((GFX / "src").rglob("*.cpp"))

FIRMWARE_SOURCES = [path for path in sorted((GARDENER / "src").rglob("*.cpp")) if board_free(path)]

SESSION_SOURCES = [SESSION / "HarnessSession.cpp", SESSION / "MemoryFiles.cpp"]

MAIN_SOURCES = [TOOL / "Script.cpp", TOOL / "main.cpp", TOOL / "entry.cpp", *SESSION_SOURCES]

SHARED_SOURCES = [
    SHARED / "arduino" / "Clock.cpp",
    SHARED / "capture" / "CaptureDisplay.cpp",
    SHARED / "capture" / "GestureStrip.cpp",
    SHARED / "capture" / "GifWriter.cpp",
]

# The API's own sources, and the two a board's C library would provide: a device's core library links
# them, so a host tool that compiles the API's headers has to compile these as well.
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


# Every folder an include reaches, so the header watch below reads the same set the compiler does: a
# header that is not watched is an object that keeps a class's old layout.
INCLUDES = (SESSION, TOOL, BERRY / "src", BERRY / "generate", GFX / "src", GARDENER / "src",
            SHARED / "arduino", SHARED / "capture", API)


def build(cc, cxx):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    includes = []
    for path in INCLUDES:
        includes.extend(["-I", str(path)])
    # The API's folder holds String.h, which a case-insensitive filesystem would otherwise hand to
    # #include <string.h> in place of the C library's.
    includes.extend(["-idirafter", str(API / "api")])

    built = []

    # A header edit changes every object that saw it, and an object built against the old one is a
    # crash rather than an error: the cache is keyed on source files, so the newest header is the
    # other half.
    header_mtime = 0.0
    if not args.no_cache:
        for directory in INCLUDES:
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


parser = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("script", help="the command file to run, one command per line")
parser.add_argument("--data", default=str(DATA), help="the data image to walk (default: firmware/data)")
parser.add_argument("--out", help="where the GIF goes (default: build/<script>.gif)")
parser.add_argument("--delay-cs", type=int,
                    help="how long each frame plays, in centiseconds (default 25, one beat)")
parser.add_argument("--scale", type=int, help="how much the picture is enlarged")
parser.add_argument("--ceiling-kpa", type=float,
                    help="the deepest depth the model chamber reaches, in kPa (default 70)")
parser.add_argument("--no-cache", action="store_true", help="rebuild every object")
parser.add_argument("--verbose", action="store_true", help="show compiler commands and the device log")
args = parser.parse_args()

build(compiler("c"), compiler("c++"))

command = [
    str(EXE),
    "--data",
    str(Path(args.data).resolve()),
    "--script",
    str(Path(args.script).resolve()),
]
out = Path(args.out) if args.out else BUILD / (Path(args.script).stem + ".gif")
command.extend(["--out", str(out.resolve())])
if args.delay_cs is not None:
    command.extend(["--delay-cs", str(args.delay_cs)])
if args.scale is not None:
    command.extend(["--scale", str(args.scale)])
if args.ceiling_kpa is not None:
    command.extend(["--ceiling-kpa", str(args.ceiling_kpa)])
if args.verbose:
    command.append("--verbose")
if args.verbose:
    print(" ".join(command), flush=True)
sys.exit(subprocess.run(command).returncode)
