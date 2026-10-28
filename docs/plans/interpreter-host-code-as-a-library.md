# Give the interpreter's host code a library of its own

The interpreter is two things in one library. [libraries/berry](../libraries/berry) is upstream's
VM, vendored at 1.1.0 with `berry_conf.h` and `be_port.c` as the only files this project owns, and
[libraries/Gardener](../libraries/Gardener) holds everything the interpreter is given: the install
(`routines/Bindings.cpp`), the four native modules under `routines/bindings/`, the page scene
(`routines/BerryScene.cpp`) and the tree that scene draws (`routines/DrawTree.cpp`). Twelve files
that exist only because there is an interpreter are therefore compiled as the firmware's own sources,
found only at a Gardener include path, and described only by the firmware's one manifest.

This plan moves those twelve files into a library beside `libraries/berry`, so that the interpreter's
host side has a name, a manifest and a folder of its own, and the firmware includes it the way it
includes Gfx. The VM stays vendored where it is. The VM owner stays in the firmware, because its beat
draws the band and owns the scene stack, and the routine registry stays too, because it is the
panel's catalogue rather than the interpreter's surface. The coupling is deliberately left as it
stands: the moved files keep including `panel/`, `machine/`, `storage/` and `core/` headers and the
firmware keeps including the library's header. Nothing a page draws, the image loads, the wire
carries or a setting stores changes.

## The twelve files are a library

Today the interpreter's host side is spread through the firmware: `routines/Bindings.h` and
`Bindings.cpp`, `routines/bindings/Module.h`, `Modules.h`, `Vacuum.cpp`, `Screen.cpp`, `Device.cpp`
and `Config.cpp`, `routines/BerryScene.h` and `BerryScene.cpp`, `routines/DrawTree.h` and
`DrawTree.cpp`. Every one is under `libraries/Gardener/src`, so a build compiles it as the firmware's
source, a reader finds it by walking the firmware, and no manifest says it is the interpreter's.

All twelve live under the `src` of one library under `libraries/`, beside `libraries/berry` and
`libraries/Gfx`, and none of the twelve is left under `libraries/Gardener/src`. What it is called is
the implementer's choice; what matters is that it is a folder of its own in that one place, with a
`library.properties` whose `sentence` and `paragraph` say it is the interpreter's host side — the
install and the modules a page is written against — and what it compiles against, and whose
`includes=` names the one header the firmware includes by. `scripts/arduino.py` needs no change: it
passes `--libraries libraries`, so a library placed there is discovered and compiled.

Checked: `python scripts/arduino.py --task build` compiles and links, and `--verbose` names the new
library's sources in the compile list; `git ls-files libraries/Gardener/src/routines` no longer lists
any of the twelve.

## The firmware reaches the library through one header

Today `Routines.cpp` includes `routines/Bindings.h` for the install and `routines/BerryScene.h` for
the page scene, and those are the firmware's only two doors into the interpreter's host code.

After the move the firmware includes the new library's one public header, which exposes the install
and the page scene, and names no path inside the library's subfolders. `Routines.cpp` still includes
`<berry.h>`, because the loader drives the VM's stack directly; what the library owns is the install,
the modules and the page, not every `be_` call. The library's public headers keep forward-declaring
`bvm`, `Routines`, `PanelContext` and `Logger` as they do today, so including one does not drag a
Gardener header into a translation unit that only wants the install.

Checked: a search of `libraries/Gardener/src` for `bindings/`, `BerryScene`, `DrawTree` and
`Bindings.h` finds only the one include of the new header, and the build still passes.

## A headless build still compiles the library to nothing

Arduino compiles every source under the `src` of a library it discovers, and the discovery that finds
a library reads an `#include` without evaluating the `#if` around it. The headless sketch includes no
Berry header, but the firmware's guarded include is still an include, so the library is discovered in
the headless build too and its sources are compiled there as they are in the full one. That is why
the seven `.cpp` files of the twelve begin with `#include "Build.h"` and `#if GARDENER_PANEL` today,
and the five headers carry no guard.

The seven sources keep those two lines exactly as they are, and the five headers stay unguarded: a
build symbol empties those files, not a library boundary. A headless image therefore compiles the
same empty objects it compiles for the panel's other sources and links them without an interpreter
symbol, and if a future `arduino-cli` stopped discovering a library through a guard, the library
would simply not be compiled and the image would be the same.

Checked: `python scripts/arduino.py --task build --sketch headless` compiles and links, the two lines
are still the first two of all seven sources, and `firmware/headless/headless.ino`'s `#error` on
`GARDENER_PANEL` still stands.

## The host check and its harness name the new paths

`tools/berry/check.py` is the check that compiles and drives the image, and it names the moved files
by path: `FIRMWARE_SOURCES` holds seven of them — `routines/Bindings.cpp`, `BerryScene.cpp`,
`DrawTree.cpp` and the four under `bindings/` — among its twenty-one entries, and `GARDENER/src` is
its only Gardener include root. Two harness files name paths that will be gone too:
`tools/berry/host/HarnessModule.cpp` includes `routines/bindings/Module.h` and
`tools/berry/host/HarnessSession.cpp` includes `routines/Bindings.h`.

The source list names the new library's files, the include roots gain the library's `src` beside
`GARDENER/src`, and the two harness includes name the public header. Nothing else about the check
changes: it still compiles the VM's own sources, the firmware's, Gfx's and the harness into one
executable and drives the same image.

Checked: `python tools/berry/check.py --compile` exits zero and reports the same routine count as
before the move, and `--memory` lists the same image files as before it.

## The prose that names the files follows them

`.github/skills/berry/SKILL.md` names `routines/bindings/` in its description, in its list of files
worth opening and in its account of what a binding is, and `.github/skills/firmware/SKILL.md` names
it once as the place a binding lives. The project README describes `libraries/Gardener` as the
machine, the panel and everything either of them runs.

Each names the new library and the folder the twelve files moved to, and the README's description of
what a build composes mentions the library beside Gfx and the vendored VM. The two manifests follow
as well: Gardener's `paragraph` no longer claims the interpreter its pages run on as its own file,
and the new library's says what it takes from the firmware.

Checked: a search of `.github/` and `README.md` for `routines/bindings` returns nothing, and the
README's library paragraph names every library a build discovers.

## Consequences

- A full build gains a library: `--verbose` lists it, and the two manifests split what used to be one
  description. The new one's `paragraph` says it is the interpreter's host side and names `berry` and
  `Gfx` among what it compiles against, stating in prose that it also compiles against
  `libraries/Gardener`, because a `depends` on `Gardener` would close a cycle with the `depends` that
  names this library. Gardener's adds the new library to `depends` and stops claiming the
  interpreter's host code as its own, keeping the machine and the panel.
- The dependency direction is unchanged and now visible: the move relocates the coupling rather than
  reducing it. The library includes `panel/PanelContext.h`, `panel/PressureReadout.h`,
  `panel/Units.h`, `panel/BuzzerPlayer.h`, `machine/Machine.h`, `machine/MachineCommand.h`,
  `machine/VacuumSystem.h`, `storage/Config.h`, `storage/DeviceConfig.h`, `storage/Jingles.h` and
  `core/Logger.h`, and the firmware includes the library's header. `arduino-cli` resolves the pair
  because both are local libraries on `--libraries` and every discovered `src` is on the include path.
- `tools/berry/check.py`'s `FIRMWARE_SOURCES` falls from twenty-one entries to fourteen, and its
  include roots gain the new library's `src`; its docstring's account of what it compiles follows.
- The parts of the interpreter that stay in the firmware read differently in a folder that no longer
  holds the modules: `Routines` is the VM's owner and the beat, `RoutineRegistry`, `Registration` and
  `RoutineSettingStore` are the routine catalogue, and the gesture queues are the panel's.
- The `.be` image, the wire domains, the settings documents and the two sketches' behaviour are
  untouched, so no spec changes.

## Deferred

- The library still compiles only inside this tree. It names the firmware's headers for the machine's
  bounds and run state, the two documents, the jingle library and the buzzer, which is more than
  `PanelContext` covers. What it would take to declare that surface in the library and have the
  firmware implement it is the parking-lot entry on the Berry host library's dependency.
- The VM owner moving with the modules: `Routines` draws the band and owns the scene stack, so a beat
  in the library would carry `panel/Band.cpp`, `panel/MarkScene.cpp`, `hal/Battery.h` and the storage
  library with it. Nothing requires it until a second host wants a beat of its own.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles and links the full device, and `--verbose` names
   the new library's sources.
2. `python scripts/arduino.py --task build --sketch headless` compiles and links the machine-alone
   image, with the interpreter's host sources emptied by the panel guard.
3. `python tools/berry/check.py --compile` exits zero, so nothing was left naming a path that moved.
4. `git ls-files libraries/Gardener/src/routines` lists none of the twelve, and a search of
   `libraries/`, `tools/` and `.github/` for `routines/bindings` returns nothing.
