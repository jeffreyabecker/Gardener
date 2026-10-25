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

The check is the firmware, not a model of it: the panel's own bindings, registry, overlay, settings
and control law are compiled here against the vendored VM, with a bitmap for a glass, a chamber for
a rig and an in-memory data image. A page therefore runs on the code the device runs, and the
driver walks the image's own routines.

--art runs one routine and saves what it drew, using the Adafruit GFX library the device compiles
for the fonts: an animated GIF of every beat by default, or a one-bit BMP when --out names a .bmp
file. --scene does the same for one of the device's own scenes: readout or browser.
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
DATA = ROOT / "firmware" / "gardener" / "data"
HOST = TOOL / "host"
BUILD = TOOL / "build"
EXE = BUILD / ("berrycheck.exe" if os.name == "nt" else "berrycheck")

# The firmware's own sources, compiled on the host. What is not here is what needs a board: the
# composition roots, the tasks, the links, the device drivers and the vendor display.
FIRMWARE_SOURCES = [
    GARDENER / "src" / "BuzzerPlayer.cpp",
    GARDENER / "src" / "panel" / "Overlay.cpp",
    GARDENER / "src" / "panel" / "Fonts.cpp",
    GARDENER / "src" / "routines" / "Bindings.cpp",
    GARDENER / "src" / "routines" / "Routines.cpp",
    GARDENER / "src" / "routines" / "GestureQueue.cpp",
    GARDENER / "src" / "routines" / "bindings" / "Config.cpp",
    GARDENER / "src" / "routines" / "bindings" / "Device.cpp",
    GARDENER / "src" / "routines" / "bindings" / "Screen.cpp",
    GARDENER / "src" / "routines" / "bindings" / "Store.cpp",
    GARDENER / "src" / "routines" / "bindings" / "Vacuum.cpp",
    GARDENER / "src" / "MachineCommand.cpp",
    GARDENER / "src" / "storage" / "Config.cpp",
    GARDENER / "src" / "storage" / "Ini.cpp",
    GARDENER / "src" / "storage" / "Jingles.cpp",
    GARDENER / "src" / "storage" / "Values.cpp",
    GARDENER / "src" / "VacuumSystem.cpp",
]

# The parts the panel's pieces are made of here: a bitmap for the glass, a chamber and its three
# parts, a counted piezo, a full battery, the data image in memory and a log buffer.
HOST_SOURCES = [
    HOST / "BitmapPanel.cpp",
    HOST / "Clock.cpp",
    HOST / "GestureCallout.cpp",
    HOST / "GifWriter.cpp",
    HOST / "HarnessModule.cpp",
    HOST / "HarnessSession.cpp",
    HOST / "MemoryFiles.cpp",
]

MAIN_SOURCES = [TOOL / "main.cpp", *HOST_SOURCES]


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


def arduino_libraries():
    """The sketchbook's libraries folder: what arduino-cli reports, or the usual places."""
    if shutil.which("arduino-cli"):
        completed = subprocess.run(
            ["arduino-cli", "config", "get", "directories.user"],
            capture_output=True,
            text=True,
        )
        user = completed.stdout.strip()
        if completed.returncode == 0 and user:
            return [Path(user) / "libraries"]
    home = Path.home()
    return [home / "Documents" / "Arduino" / "libraries", home / "Arduino" / "libraries"]


def gfx_library():
    """The Adafruit GFX library the device compiles: the fonts and the classic 5x7 come from it."""
    requested = args.gfx or os.environ.get("ADAFRUIT_GFX")
    candidates = [Path(requested)] if requested else []
    candidates += [libraries / "Adafruit_GFX_Library" for libraries in arduino_libraries()]
    for path in candidates:
        if (path / "Fonts" / "FreeSans18pt7b.h").exists() and (path / "glcdfont.c").exists():
            return path
    sys.exit(
        "the panel renderer needs the Adafruit GFX library's fonts; install it with\n"
        '  arduino-cli lib install "Adafruit GFX Library"\n'
        "or point --gfx (or ADAFRUIT_GFX) at the library folder"
    )


def slug(path):
    return "_".join(Path(path).resolve().parts[-4:])


def build(cc, cxx, gfx):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    includes = []
    for path in (HOST, BERRY / "src", BERRY / "generate", TOOL / "gfx", gfx, GARDENER / "src"):
        includes.extend(["-I", str(path)])

    built = []

    def compile(source, std, compiler_path):
        obj = objects / (slug(source) + ".o")
        if (args.no_cache or not obj.exists()
                or obj.stat().st_mtime < Path(source).stat().st_mtime):
            run([compiler_path, std, "-O0", *includes, "-c", str(source), "-o", str(obj)])
        built.append(obj)

    for source in sorted((BERRY / "src").glob("*.c")):
        compile(source, "-std=gnu11", cc)
    for source in (*MAIN_SOURCES, *FIRMWARE_SOURCES):
        compile(source, "-std=c++17", cxx)

    # Static, because the first libstdc++ on PATH is usually a different MinGW's and the exe then
    # refuses to start with a missing entry point rather than a diagnostic.
    run([cxx, "-static", *(str(obj) for obj in built), "-o", str(EXE), "-lm"])


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compile", action="store_true", help="stop after compiling and loading")
parser.add_argument("--no-cache", action="store_true", help="rebuild every object")
parser.add_argument("--verbose", action="store_true", help="show compiler commands and device log")
parser.add_argument("--data", default=str(DATA), help="the data image to check")
parser.add_argument("--gfx", help="the Adafruit GFX library folder; default is the installed one")
parser.add_argument("--art", metavar="NAME", help="draw that routine and save the frame as a BMP")
parser.add_argument(
    "--scene",
    metavar="NAME",
    help="draw that device scene — readout or browser — and save the frame as a BMP",
)
parser.add_argument(
    "--component",
    metavar="NAME",
    help="draw that component — from either framework — and save the frame as a BMP",
)
parser.add_argument(
    "--seconds",
    type=int,
    default=0,
    help="with --art or --scene, run this much virtual time before saving",
)
parser.add_argument(
    "--out",
    help="where the frame or animation goes (default: build/<name>.gif)",
)
parser.add_argument(
    "--sets",
    type=int,
    default=12,
    help="with --art, the set count to activate with (default 12; 0 keeps the file's)",
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

# The capture folders the harness writes into. Generated frames live here rather than in build/,
# so a component's frame is easy to find and the build output stays machine-only.
for sub in ("components",):
    (TOOL / "artifacts" / sub).mkdir(parents=True, exist_ok=True)

build(compiler("c"), compiler("c++"), gfx_library())

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
            "--sets",
            str(args.sets),
            "--out",
            str(out.resolve()),
        ]
    )
if args.scene:
    out = Path(args.out) if args.out else BUILD / f"{args.scene}.gif"
    command.extend(
        ["--scene", args.scene, "--seconds", str(args.seconds), "--out", str(out.resolve())]
    )
if args.component:
    out = Path(args.out) if args.out else TOOL / "artifacts" / "components" / f"{args.component}.bmp"
    command.extend(["--component", args.component, "--out", str(out.resolve())])
if args.tests or args.test:
    tests_dir = TOOL / "tests"
    command.extend(["--suite", str(tests_dir / "suite.be")])
    for file in sorted(tests_dir.glob("*.test.be")):
        command.extend(["--test-file", str(file)])
    if args.test:
        command.extend(["--test", args.test])
    # The stem the runner names a per-case GIF from; a .bmp name is meaningless here.
    stem = Path(args.out) if args.out else BUILD / "runs.gif"
    command.extend(["--out", str(stem.resolve())])
if args.verbose:
    print(" ".join(command), flush=True)
sys.exit(subprocess.run(command).returncode)
