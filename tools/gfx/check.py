#!/usr/bin/env python3
"""Compile and run the Gfx component checks on the host.

    python tools/gfx/check.py              # build and run every case
    python tools/gfx/check.py --no-cache   # rebuild every object
    python tools/gfx/check.py --verbose    # print each compiler command

The components in libraries/Gfx/src/ui/ are plain C++: they draw through a Canvas over a
DisplayDevice and never touch the interpreter. This check is therefore the firmware's own drawing
code compiled against a stub display, with no Berry, no VM and no board: a case asserts the numbers
a component answers and the pixels it leaves in the frame, and the exit code is 0 only when every
case passes.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parent
ROOT = TOOL.parents[1]
GFX = ROOT / "libraries" / "Gfx"
BUILD = TOOL / "build"
EXE = BUILD / ("gfxcheck.exe" if os.name == "nt" else "gfxcheck")
HOST = TOOL / "host"


def compiler():
    preferred = os.environ.get("CXX")
    for name in (preferred, "g++", "clang++"):
        if name and shutil.which(name):
            return name
    sys.exit("no c++ compiler found; set CXX")


def run(command):
    if args.verbose:
        print(" ".join(str(part) for part in command), flush=True)
    completed = subprocess.run(command)
    if completed.returncode != 0:
        sys.exit(f"{Path(str(command[0])).name} exited with {completed.returncode}")


def slug(path):
    return "_".join(Path(path).resolve().parts[-3:])


def build(cxx):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    includes = []
    for path in (HOST, TOOL, GFX / "src"):
        includes.extend(["-I", str(path)])

    # A header edit changes every object that saw it, and an object built against the old one is a
    # crash rather than an error: one that embeds a class's layout by value corrupts the heap when
    # the class grows.
    header_mtime = 0.0
    if not args.no_cache:
        for directory in (HOST, TOOL, GFX / "src"):
            for header in directory.rglob("*.h"):
                header_mtime = max(header_mtime, header.stat().st_mtime)

    sources = [
        *(GFX / "src" / "gfx").glob("*.cpp"),
        *(GFX / "src" / "ui").glob("*.cpp"),
        TOOL / "main.cpp",
    ]

    built = []
    for source in sorted(sources):
        obj = objects / (slug(source) + ".o")
        if (args.no_cache or not obj.exists()
                or obj.stat().st_mtime < Path(source).stat().st_mtime
                or obj.stat().st_mtime < header_mtime):
            run([cxx, "-std=c++17", "-O0", *includes, "-c", str(source), "-o", str(obj)])
        built.append(obj)

    # Static, because the first libstdc++ on PATH is usually a different MinGW's and the exe then
    # refuses to start with a missing entry point rather than a diagnostic.
    run([cxx, "-static", *(str(obj) for obj in built), "-o", str(EXE), "-lm"])


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--no-cache", action="store_true", help="rebuild every object")
parser.add_argument("--verbose", action="store_true", help="show the compiler commands")
args = parser.parse_args()

build(compiler())
sys.exit(subprocess.run([str(EXE)]).returncode)
