# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it,
then vents it — driven over one serial line.

The serial line is the only way in. A controller sets a depth and the machine pulls to it; a vent
releases the chamber; a reset clears a fault. What the device owns is what has to happen when
nobody is watching: holding the target, and venting the chamber through one path on stop, abort or
fault. Pressures are kPa below ambient, on the wire and in the firmware.

The device is the machine — pump, sensor and vent valve. One sketch,
[firmware/gardener](./firmware/gardener), composes the libraries for it: what it names is what the
device has, over the serial link it builds.

## Wire

One domain, `vacuum`, with four verbs on a line ending in LF:

| command | answer |
| --- | --- |
| `vacuum get` | `ok pressure_kpa=<x> target=<y> state=<word> duty=<z> fault=<code\|none>` |
| `vacuum set <kPa\|->` | `ok target=<kPa> state=PULL`, or `ok target=0.0 state=VENT` for the ambient target |
| `vacuum vent` | `ok state=VENT` |
| `vacuum reset` | `ok state=IDLE` — clears a latched fault |

A set is a target and a run in one command, and the run is granted the machine's own ceiling
(two hours). Zero is the ambient target, which releases the chamber, so `set 0` and `set -` vent.
A target is refused with `err out_of_range` past the fitted floor, and with `err not_ready` when
the machine has no reading or a latched fault. The boot and run lines the firmware logs share the
same stream, framed as `[I]`, `[W]` and `[E]`, so a controller reads the `ok` and `err` lines.

## Build

Requires `arduino-cli` with the ESP32 core, and Python 3 for the wrapper.

```powershell
arduino-cli core install esp32:esp32
python scripts/arduino.py --task build
python scripts/arduino.py --task upload --port COM5
python scripts/arduino.py --task monitor --port COM5
```

The board options are pinned in
[firmware/gardener/sketch.yaml](./firmware/gardener/sketch.yaml) — including `CDCOnBoot=cdc`,
without which `Serial` is UART0 and a USB cable sees nothing. The build uses
`--libraries libraries`, so [libraries/Gardener](./libraries/Gardener) — the machine, the control
law and the serial link — is the firmware rather than a sketchbook copy. Beside it a build finds
[libraries/Runtime](./libraries/Runtime), the board it runs on: the pump, sensor and valve drivers,
the parts builder and the two tasks that drive them.

A build keeps its object files in `firmware/gardener/build/compile-<hash>`, a path per board and
define set, so the next one compiles only what changed since, header dependents included.

The rig's numbers are written to a generated `firmware/gardener/rig_options.h` rather than passed
as compiler flags, so retuning a rig recompiles the files that read them and nothing else. The
header `core/knob_defaults.h` beside `core/Knob.h` is generated the same way, by `tools/knob.py`
from the `GARDENER_KNOB_VALID` calls, and both build paths write it before they compile.

## Unit tests

The tree's own C++ runs on the host, with no board and no data image:

```powershell
python tools/unit/check.py                        # build and run every case
python tools/unit/check.py --list-test-cases      # what the suite holds
python tools/unit/check.py --test-case="a name"   # one case, while a change is being made
```

The suite compiles [libraries/Gardener](./libraries/Gardener) beside its own cases under
[doctest](./tools/doctest), with [tools/host](./tools/host) supplying the Arduino API it is built
against and a clock a case advances. It needs a C++ compiler (`g++` or `clang++`) and Python, and
nothing else.

## Formatting

The C++ is formatted with `clang-format` from [.clang-format](./.clang-format) at the root, which
holds the style [.github/skills/cpp](./.github/skills/cpp/SKILL.md) describes: two spaces, a brace on
the statement's own line, a hundred columns, and comments left as they were written.

```powershell
clang-format -i (git ls-files '*.h' '*.cpp' '*.ino')   # rewrite what is not formatted
clang-format --dry-run --Werror libraries/Gardener/src/machine/Machine.h
```

What keeps another style carries a `.clang-format` of its own switching formatting off — doctest,
and the host's Arduino core and stand-ins — so the sweep leaves them alone. An editor reads the
same files, so a save lands in the shape the sweep would give it.

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
