# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it
for a timer, then vents it — driven over a serial line or the panel on the device.

There is no ownership handshake: the serial line and the panel can both command the machine, and
the last write wins. What the device owns is what has to happen when nobody is
watching: pulling to the target, and venting the chamber through one path on stop, abort, fault or
the panel's reserved key hold. Pressures are kPa, on the wire and in the firmware.

The device is the machine — pump, sensor and vent valve — with the OLED panel, the keys, the buzzer
and the battery gauge arranged around it. One sketch, [firmware/gardener](./firmware/gardener),
composes the libraries for it: what it names is what the device has, over the serial link it builds.
The Bluetooth transport is out of the build for now — its sources are commented out, so the serial
line is the only way a controller reaches the device.

## Build

Requires `arduino-cli` with the ESP32 core, and Python 3 for the wrapper.

```powershell
arduino-cli core install esp32:esp32
python scripts/arduino.py --task libs
python scripts/arduino.py --task build
python scripts/arduino.py --task upload --port COM5
python scripts/arduino.py --task monitor --port COM5
```

`--task data` writes [firmware/data/](./firmware/data) to the device's filesystem. VS Code has the
same commands as tasks.

The board options are pinned in
[firmware/gardener/sketch.yaml](./firmware/gardener/sketch.yaml) — including `CDCOnBoot=cdc`,
without which `Serial` is UART0 and a USB cable sees nothing. The build uses
`--libraries libraries`, so [libraries/Gardener](./libraries/Gardener) — the machine, the panel and
the interpreter's host side, the install and the native modules a page is written against — is the
firmware rather than a sketchbook copy. Beside it a build finds
[libraries/Runtime](./libraries/Runtime), the board it runs on — the part drivers, the file store,
the two composition roots and the tasks that drive them;
[libraries/Gfx](./libraries/Gfx), the canvas those pages draw on; and
[libraries/berry](./libraries/berry), the vendored VM itself.

A build keeps its object files in `firmware/gardener/build/compile-<hash>`, a path per board and
define set, so the next one compiles only what changed since, header dependents included. The two
libraries that never change are not compiled at all after the first build:
[libraries/berry](./libraries/berry) and the staged NimBLE are packed into an archive beside their
sources — `src/esp32s3/libberry.a` — which `arduino-cli` links instead, and which is two thirds of
what a full build used to be. The archives are not committed: a fresh clone, a new toolchain or a
deleted one compiles the sources and packs again. `--task archive` repacks them on demand, and
`--task clean` keeps them.

The rig's numbers are written to a generated `firmware/gardener/rig_options.h` rather than passed
as compiler flags, so retuning a rig recompiles the files that read them and nothing else.
`--task libs` installs the pinned libraries and stages the copies the build compiles against;
`--task clean` removes the object files, and the data image beside them, by hand.

## Unit tests

The tree's own C++ runs on the host, with no board, no interpreter and no data image:

```powershell
python tools/unit/check.py                        # build and run every case
python tools/unit/check.py --list-test-cases      # what the suite holds
python tools/unit/check.py --test-case="a name"   # one case, while a change is being made
```

The suite compiles [libraries/Gfx](./libraries/Gfx) and [libraries/Gardener](./libraries/Gardener)
beside its own cases under [doctest](./tools/doctest), with [tools/host](./tools/host) supplying the
Arduino API it is built against, a clock a case advances and the display a case hands a page — a
recording display that keeps the beats it drove and writes them as a GIF. It needs a C++ compiler
(`g++` or `clang++`) and Python, and nothing else.

## Formatting

The C++ is formatted with `clang-format` from [.clang-format](./.clang-format) at the root, which
holds the style [.github/skills/cpp](./.github/skills/cpp/SKILL.md) describes: two spaces, a brace on
the statement's own line, a hundred columns, and comments left as they were written.

```powershell
clang-format -i (git ls-files '*.h' '*.cpp' '*.ino')   # rewrite what is not formatted
clang-format --dry-run --Werror libraries/Gfx/src/Canvas.h
```

What keeps another style carries a `.clang-format` of its own switching formatting off — the
vendored VM under [libraries/berry](./libraries/berry), doctest, and the host's Arduino core and
stand-ins — so the sweep leaves them alone. An editor reads the same files, so a save lands in the
shape the sweep would give it.

## Walkthroughs

A GIF of the app can be scripted as a command file and run on the host, with no board and no
interpreter on the device:

```powershell
python tools/walkthrough/walk.py tools/walkthrough/sample.walk
python tools/walkthrough/walk.py my.walk --out build/my.gif --delay-cs 20 --scale 3
```

`sample.walk` is the tour — a run on the home page, the browser over it and a routine launched with a
set — and the folder holds a walk per subject beside it: `home.walk` for the target stepped, started,
stopped and resumed; `settings.walk` for the device's own list edited and saved through every row;
`settings-cancel.walk` for the same list throwing each change away; and `units.walk` for the display
unit changed from kPa to inHg and the home page read back in it.

[docs/guides/walkthroughs.md](./docs/guides/walkthroughs.md) is the command reference: every verb,
the gesture spelling, the marks and the note, the picture's layout, and the tool's options.

## Documentation

- [docs/README.md](./docs/README.md) — the index: specs, guides and backlogs
- [docs/guides/poc-parts.md](./docs/guides/poc-parts.md) — the parts, the wiring and a build
- [docs/archive/specs-and-plans.zip](./docs/archive/specs-and-plans.zip) — every spec and plan as it
  stood before the panel port, kept for reference and read by nothing

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids or vapours can
hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember that the
firmware's limits are software limits only.

The one hardware guarantee is the valve. It is normally open, so losing power releases the chamber
whether or not the firmware noticed; a normally closed part trades that fail-safe for holding the
vacuum through a power loss.
