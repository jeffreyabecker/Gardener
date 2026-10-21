#!/usr/bin/env python3
"""Compile and run the Berry data image on the host.

    python .github/skills/berry/check.py              # compile, load and drive
    python .github/skills/berry/check.py --compile    # compile only
    python .github/skills/berry/check.py --no-cache   # rebuild the VM objects

The VM is the vendored one from libraries/berry, built with the same berry_conf.h
the firmware uses, so 32-bit ints, single-precision reals and the 2000-slot stack
are the ones a routine will meet on the device.
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


def build(cc):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    include = ["-I", str(BERRY / "src"), "-I", str(BERRY / "generate")]
    libraries = []
    for source in sorted((BERRY / "src").glob("*.c")):
        obj = objects / (source.stem + ".o")
        if args.no_cache or not obj.exists() or obj.stat().st_mtime < source.stat().st_mtime:
            run([cc, "-std=gnu11", "-O0", *include, "-c", str(source), "-o", str(obj)])
        libraries.append(obj)

    main_obj = objects / "berrycheck_main.o"
    main_source = SKILL / "main.c"
    if args.no_cache or not main_obj.exists() or main_obj.stat().st_mtime < main_source.stat().st_mtime:
        run([cc, "-std=gnu11", "-O0", *include, "-c", str(main_source), "-o", str(main_obj)])
    run([cc, str(main_obj), *(str(obj) for obj in libraries), "-o", str(EXE), "-lm"])


def files(data):
    modules = sorted((data / "modules").glob("*.be"))
    routines = sorted((data / "routines").glob("*.be"))
    return [SKILL / "harness.be", *modules, SKILL / "driver.be", *routines]


def inis(data):
    found = [("device", data / "settings.ini"), ("jinglelibrary", data / "modules" / "jingles.txt")]
    for path in sorted((data / "routines").glob("*.ini")):
        found.append((path.stem, path))
    return [(name, path) for name, path in found if path.exists()]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compile", action="store_true", help="stop after compiling and loading")
parser.add_argument("--no-cache", action="store_true", help="rebuild the VM objects")
parser.add_argument("--verbose", action="store_true", help="show the compiler commands")
parser.add_argument("--data", default=str(DATA), help="the data image to check")
args = parser.parse_args()

cc = compiler()
build(cc)

data = Path(args.data).resolve()
command = [str(EXE)]
if args.compile:
    command.append("--compile-only")
for name, path in inis(data):
    command.extend(["--ini", f"{name}={path}"])
command.extend(str(path) for path in files(data))
if args.verbose:
    print(" ".join(command), flush=True)
sys.exit(subprocess.run(command).returncode)
