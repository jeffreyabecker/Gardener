#!/usr/bin/env python3
"""Compile and run the Berry data image on the host.

    python .github/skills/berry/check.py              # compile, load and drive
    python .github/skills/berry/check.py --compile    # compile only
    python .github/skills/berry/check.py --no-cache   # rebuild the VM objects
    python .github/skills/berry/check.py --art default
    python .github/skills/berry/check.py --art interval --seconds 30

The VM is the vendored one from libraries/berry, built with the same berry_conf.h
the firmware uses, so 32-bit ints, single-precision reals and the 2000-slot stack
are the ones a routine will meet on the device.

--art runs one routine and saves the frame it drew as a one-bit BMP, using the
Adafruit GFX library the device compiles for the fonts, so the picture is the
panel's, not an approximation.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parent
ROOT = SKILL.parents[2]
BERRY = ROOT / "libraries" / "berry"
DATA = ROOT / "firmware" / "gardener" / "data"
BUILD = SKILL / "build"
EXE = BUILD / ("berrycheck.exe" if os.name == "nt" else "berrycheck")


def compiler():
    for name in (os.environ.get("CC"), "cc", "gcc", "clang"):
        if name and shutil.which(name):
            return name
    sys.exit("no C compiler found; set CC")


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
        "the frame renderer needs the Adafruit GFX library's fonts; install it with\n"
        '  arduino-cli lib install "Adafruit GFX Library"\n'
        "or point --gfx (or ADAFRUIT_GFX) at the library folder"
    )


def build(cc, gfx):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    include = [
        "-I", str(BERRY / "src"),
        "-I", str(BERRY / "generate"),
        "-I", str(SKILL / "gfx"),
        "-I", str(gfx),
    ]
    libraries = []
    for source in sorted((BERRY / "src").glob("*.c")):
        obj = objects / (source.stem + ".o")
        if args.no_cache or not obj.exists() or obj.stat().st_mtime < source.stat().st_mtime:
            run([cc, "-std=gnu11", "-O0", *include, "-c", str(source), "-o", str(obj)])
        libraries.append(obj)

    # main.c and frame.c: the host entry point, and the panel the harness draws on.
    for source in (SKILL / "main.c", SKILL / "frame.c"):
        obj = objects / ("berrycheck_" + source.stem + ".o")
        if args.no_cache or not obj.exists() or obj.stat().st_mtime < source.stat().st_mtime:
            run([cc, "-std=gnu11", "-O0", *include, "-c", str(source), "-o", str(obj)])
        libraries.append(obj)
    run([cc, *(str(obj) for obj in libraries), "-o", str(EXE), "-lm"])


def files(data):
    modules = sorted((data / "modules").glob("*.be"))
    routines = sorted((data / "routines").glob("*.be"))
    return [SKILL / "harness.be", *modules, SKILL / "driver.be", *routines]


def seeds(data):
    found = [("device", data / "settings.cfg"), ("jinglelibrary", data / "modules" / "jingles.txt")]
    for path in sorted((data / "routines").glob("*.cfg")):
        found.append((path.stem, path))
    return [(name, path) for name, path in found if path.exists()]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compile", action="store_true", help="stop after compiling and loading")
parser.add_argument("--no-cache", action="store_true", help="rebuild the VM objects")
parser.add_argument("--verbose", action="store_true", help="show the compiler commands")
parser.add_argument("--data", default=str(DATA), help="the data image to check")
parser.add_argument("--gfx", help="the Adafruit GFX library folder; default is the installed one")
parser.add_argument("--art", metavar="NAME", help="draw that routine and save the frame as a BMP")
parser.add_argument(
    "--seconds",
    type=int,
    default=0,
    help="with --art, press OK and run this much virtual time before saving",
)
parser.add_argument("--out", help="with --art, where the BMP goes (default: build/<name>.bmp)")
args = parser.parse_args()

cc = compiler()
build(cc, gfx_library())

data = Path(args.data).resolve()
command = [str(EXE)]
if args.compile:
    command.append("--compile-only")
if args.art:
    out = Path(args.out) if args.out else BUILD / f"{args.art}.bmp"
    command.extend(["--art", args.art, "--seconds", str(args.seconds), "--out", str(out.resolve())])
for name, path in seeds(data):
    command.extend(["--values", f"{name}={path}"])
command.extend(str(path) for path in files(data))
if args.verbose:
    print(" ".join(command), flush=True)
sys.exit(subprocess.run(command).returncode)
