#!/usr/bin/env python3
"""Compile and run the host unit tests.

    python tools/unit/check.py                       # build and run every case
    python tools/unit/check.py --list-test-cases     # what doctest knows about
    python tools/unit/check.py --test-case="a name"  # one case, while a change is being made
    python tools/unit/check.py --no-cache            # rebuild every object
    python tools/unit/check.py --verbose             # print each compiler command

The suite is the firmware's own C++ compiled for the host: libraries/Gfx and libraries/Gardener,
their cases under this folder, the fakes the cases drive, the capture writer the tools share, and
doctest. It needs a C++ compiler and Python and nothing else -- no interpreter, no data image and no
board. A source that names one of those things is not one of its inputs, and neither is anything
under libraries/berry, libraries/BerryHost, tools/berry or firmware/data.

Every argument the script does not recognise is passed to the test binary, so doctest's own filter,
listing and reporting are the suite's.
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
HOST = TOOLS / "host"
API = HOST / "arduino-core-api"
DOCTEST = TOOLS / "doctest"
GFX = ROOT / "libraries" / "Gfx"
GARDENER = ROOT / "libraries" / "Gardener"
BUILD = TOOL / "build"
EXE = BUILD / ("unitcheck.exe" if os.name == "nt" else "unitcheck")

# A source naming any of these needs the interpreter, a filesystem, a radio or an RTOS, and so is not
# part of what the suite compiles.
PLATFORM_INCLUDES = ("<berry.h>", "<FS.h>", "<Wire.h>", "<NimBLE", "freertos/", "<SD.h>")

# The two sources that reach the interpreter through Routines without naming it themselves. Both are
# deferred: the panel's composition and the routine launcher belong to a suite that loads a VM.
SUITE_EXCLUDES = ("panel/Panel.cpp", "routines/Routines.cpp")

# The API's own sources, and the two a board's C library would provide.
API_SOURCES = ("String.cpp", "Print.cpp", "Stream.cpp", "Common.cpp")
API_EXTRAS = ("itoa.cpp", "dtostrf.cpp")

# A source that needs a build symbol of its own. Device.cpp is the machine-alone build's `device`
# domain: its panel branch builds createDeviceDomain over Panel, which names the interpreter and is
# not part of the suite, so it compiles as the machine-alone build does.
SOURCE_DEFINES = {"serial/domains/Device.cpp": ("-DGARDENER_PANEL=0",)}


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


def interpreter_free(source):
    if any(source.as_posix().endswith(name) for name in SUITE_EXCLUDES):
        return False
    text = source.read_text(encoding="utf-8", errors="replace")
    return not any(name in text for name in PLATFORM_INCLUDES)


def sources():
    found = [
        *(GFX / "src" / "gfx").glob("*.cpp"),
        *(GFX / "src" / "ui").glob("*.cpp"),
        *(HOST / "arduino").glob("*.cpp"),
        *(HOST / "capture").glob("*.cpp"),
        *((API / "api") / name for name in API_SOURCES),
        *((API / "extras") / name for name in API_EXTRAS),
        *(TOOL.glob("*.cpp")),
        *(TOOL / "fakes").glob("*.cpp"),
        *(TOOL / "cases").glob("*.cpp"),
    ]
    found.extend(path for path in (GARDENER / "src").rglob("*.cpp") if interpreter_free(path))
    return found


def include_dirs():
    return [TOOL, HOST / "arduino", HOST / "capture", API, DOCTEST, GFX / "src", GARDENER / "src"]


def defines_for(source):
    path = source.as_posix()
    for name, flags in SOURCE_DEFINES.items():
        if path.endswith(name):
            return flags
    return ()


def build(cxx):
    objects = BUILD / "obj"
    objects.mkdir(parents=True, exist_ok=True)

    includes = []
    for path in include_dirs():
        includes.extend(["-I", str(path)])
    # The API's folder holds String.h, which a case-insensitive filesystem would otherwise hand to
    # #include <string.h> in place of the C library's. After the system headers, only the API's own
    # quote includes reach it.
    includes.extend(["-idirafter", str(API / "api")])

    header_mtime = 0.0
    if not args.no_cache:
        for directory in include_dirs():
            for header in directory.rglob("*.h"):
                header_mtime = max(header_mtime, header.stat().st_mtime)
            for header in directory.rglob("*.hpp"):
                header_mtime = max(header_mtime, header.stat().st_mtime)

    built = []
    for source in sorted(set(sources())):
        obj = objects / (slug(source) + ".o")
        if (args.no_cache or not obj.exists()
                or obj.stat().st_mtime < Path(source).stat().st_mtime
                or obj.stat().st_mtime < header_mtime):
            run([cxx, "-std=c++17", "-O0", *defines_for(source), *includes, "-c", str(source),
                 "-o", str(obj)])
        built.append(obj)

    # Static, because the first libstdc++ on PATH is usually a different MinGW's and the exe then
    # refuses to start with a missing entry point rather than a diagnostic. The objects go in a
    # response file: a few dozen full paths on one command line is more than the linker takes.
    response = BUILD / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in built), encoding="utf-8")
    run([cxx, "-static", f"@{response}", "-o", str(EXE), "-lm"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-cache", action="store_true", help="rebuild every object")
    parser.add_argument("--verbose", action="store_true", help="show the compiler commands")
    args, test_args = parser.parse_known_args()

    build(compiler())
    sys.exit(subprocess.run([str(EXE), *test_args]).returncode)
