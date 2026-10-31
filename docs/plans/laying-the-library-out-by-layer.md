# Laying the Gardener library out by layer

## What is wrong now

`libraries/Gardener/src` holds 102 sources across six folders and four inside them. The folders
already describe a stack — `core` underneath `machine`, both underneath `panel`, everything
underneath `routines` — and the namespaces mostly do not say it. Forty-five sources name a layer
(`panel::page`, `panel::parts`, `berry`) and fifty-six are flat `gardener`; the umbrella names none.
Three types in `panel/page/` — `BerryScene`, `Registration` and `RoutineRegistry` — declare
themselves in plain `gardener` while every neighbour in the folder is `panel::page`, so a reader
cannot tell whether the folder or the namespace is the statement.

The gaps are not only cosmetic, because nothing states the stack, and two edges in the tree run
against it:

- `panel/page/BerryScene.cpp` includes `routines/bindings/BindingContext.h` and
  `routines/bindings/DrawTree.h`. It is the only place a page reaches above the Berry boundary, and
  it is the same file that `routines/BerryInterpreter.cpp` includes to build its root page — the two
  ends of one arrow through the middle of the stack.
- `storage/RoutineSettingStore.h` includes `panel/page/SettingsRows.h` and types its whole interface
  in terms of `panel::page::SettingsRow` and `panel::page::SettingValues`, while
  `panel/page/Registration.cpp` and `panel/page/RoutineRegistry.cpp` include the store. The folder
  that writes documents and the folder that draws pages depend on each other.

The folder names also collide with Runtime's. Both libraries put their `src` on the include path and
both have a `machine/`, a `serial/` and a `storage/`; `#include "machine/SafeReboot.h"` and
`#include "machine/Machine.h"` are one line apart in the same files with nothing but the filename
saying which library the header came from.

## The shape of the change

One rule, and everything else follows from it: **a folder is a namespace and a namespace is a
folder, one segment each, and a file may include only from its own layer or a lower one.**

| # | folder | namespace | holds |
| --- | --- | --- | --- |
| 1 | `core/` | `gardener::core` | the parts, the log, the store, the published state |
| 2 | `machine/` | `gardener::machine` | the chamber, its commands, the control tick |
| 2 | `storage/` | `gardener::storage` | a document's text, the device's document, a routine's set |
| 3 | `panel/` | `gardener::panel` | the panel itself |
| 3 | `panel/parts/` | `gardener::panel::parts` | the piezo and the gesture classifier |
| 3 | `panel/page/` | `gardener::panel::page` | every scene, the stack, the registry |
| 4 | `bindings/` | `gardener::bindings` | everything that knows `bvm` and C++ both |
| 5 | `routines/` | `gardener::routines` | the interpreter and the loader |
| 6 | `beats/` | `gardener::beats` | the four things a task steps |
| 6 | `serial/` | `gardener::serial` | the wire and its domains |

`serial::domains` is the one nested pair (`serial/domains/`), because a domain is a file of the wire
and not a layer of its own.

### The map

```
libraries/Gardener/src/
  Gardener.h                             gardener              the umbrella: includes, nothing else

  core/                                  gardener::core
    Logger.h  SerialLogger.h                                 what a part reports through
    Library.h                                                what a document is read from
    Math.h  Units.h  RunState.h  SystemState.h               numbers, names, the published state
    Battery.h  Button.h  ButtonPanel.h  Buzzer.h
    Pump.h  PressureSensor.h  VentValve.h                    the parts, as interfaces
    firmwareversion.h

  machine/                               gardener::machine
    Machine.{h,cpp}  VacuumSystem.{h,cpp}                    the chamber and the run
    MachineCommand.{h,cpp}                                   the record, the queue and the outbox
    CommandQueue.h

  storage/                               gardener::storage
    Config.{h,cpp}                                           the .ini reader and writer
    DeviceSettings.{h,cpp}
    SettingKind.h                                            a setting's key, kind, default and text
    RoutineSettingStore.{h,cpp}

  panel/                                 gardener::panel
    GestureQueue.{h,cpp}
    parts/                               gardener::panel::parts
      BuzzerPlayer.{h,cpp}
      GestureClassifier.{h,cpp}
    page/                                gardener::panel::page
      Scene.{h,cpp}  Ux.{h,cpp}  Mark.h
      MarkScene.{h,cpp}  BoolScene.{h,cpp}  ChoiceScene.{h,cpp}  DurationScene.{h,cpp}
      EditorScene.{h,cpp}  NumberScene.{h,cpp}  PressureScene.{h,cpp}
      SettingsScene.{h,cpp}  SettingsRows.{h,cpp}  PressureReadout.{h,cpp}
      Overlay.{h,cpp}  Registration.{h,cpp}  RoutineRegistry.{h,cpp}

  bindings/                              gardener::bindings
    Module.h  Modules.h                                      the tables and the helpers
    BindingContext.{h,cpp}
    BerryScene.{h,cpp}                                       a page whose body is Berry
    DrawTree.{h,cpp}                                         a draw record walked into paints
    Screen.cpp  Device.cpp  Vacuum.cpp  Settings.cpp  Ux.cpp  Routines.cpp

  routines/                              gardener::routines
    Interpreter.h  BerryInterpreter.{h,cpp}  Routines.{h,cpp}

  beats/                                 gardener::beats
    MachineTick.{h,cpp}  ButtonsTick.{h,cpp}  PartsTick.{h,cpp}  RoutinesTick.{h,cpp}

  serial/                                gardener::serial
    Domain.h  Domains.h  PanelDomains.h  BoardFacts.h
    SerialLink.{h,cpp}
    domains/                             gardener::serial::domains
      Device.cpp  Vacuum.cpp  Buzzer.cpp
```

### What moves

| from | to | why |
| --- | --- | --- |
| `panel/page/BerryScene.{h,cpp}` | `bindings/` | the only page naming `bvm` |
| `routines/ButtonsTick.{h,cpp}` | `beats/` | clocked by the panel task, above the panel |
| `routines/PartsTick.{h,cpp}` | `beats/` | same |
| `routines/RoutinesTick.{h,cpp}` | `beats/` | it takes `Routines&`, so it cannot sit in `panel/` |
| `machine/ControlTick.{h,cpp}` | `beats/MachineTick.{h,cpp}` | it is the machine task's beat, as the other three are the panel task's |
| `firmwareversion.h` | `core/` | the boot line and `device get` both read it |
| `routines/bindings/*` | `bindings/` | the folder and its namespace become the same word |

`serial/BoardFacts.h` stays where it is: it describes the board for one reply, and only the device
domain and the sketch read it. `Gardener.h` is the one file that names several layers; it includes
and declares nothing, and it keeps its place at the root of the source tree.

### The one cycle, and how it is cut

`RoutineSettingStore` needs five things from a row — its key, its kind, its default, the text a
document keeps and a value read back out of that text. A row needs five more that only drawing can
answer — its label, the text the list prints, the scene that edits it, the check it carries, and the
acceptance a value from outside goes through. Only the first five are the document's.

So the row splits into a part and its page half, the way a device already splits into
`core/Pump.h` and `hal/impl/PwmPump`:

- `storage/SettingKind.h` — `gardener::storage::SettingKind`, with `key()`, `defaultValue()`,
  `declaration()`, `stored()` and `parse()`, and `SettingDeclaration`, `SettingValues` and
  `ChoiceItem` beside it, which is the vocabulary a document speaks.
- `panel/page/SettingsRows.h` — `gardener::panel::page::SettingsRow : public storage::SettingKind`,
  adding `label()`, `text()`, `scene()`, `accept()` and `check()`.

`RoutineSettingStore::load` and `save` then take `const std::vector<const storage::SettingKind*>&`,
and `storage` stops naming `panel` at all. Two places in the library hand the store a list —
`Registration::loadSettings` and `Registration::saveSettings`, which are the store's only callers —
and each builds that vector in one line from the rows it already holds. The registry case builds one
for its rig.

## The alternatives, and why not

**Folders only, one flat `gardener::`.** What the tree has now. The folder is then the only
statement of the layer, and a review cannot see a violation: both edges named above were written by
someone reading a folder that said one thing and a type that said nothing.

**Namespaces only, with the files left where they are.** An include line is what a reader follows,
and `#include "panel/page/BerryScene.h"` would still say "page" while the type said
`gardener::bindings`. Two statements of one layering drift, and only one of them can be checked by a
grep.

**`bindings/` kept under `routines/`.** `gardener::routines::bindings` is four segments for the
layer `routines` depends on, and `BerryScene` under `routines/` reads as part of the loader. The
include path would also keep spelling the upward arrow this plan deletes.

**The three ticks under `panel/beat/`.** `RoutinesTick` takes `Routines&`, so grouping them by the
panel they serve would leave the panel both below and above the Berry layer at the same time.

**`SettingsRow` moved whole into `storage/`.** It stores a `GestureQueue&` and a `Ux&` and returns a
`std::unique_ptr<EditorScene>`, so the move pulls the whole page layer into storage — the same
cycle, one folder lower and harder to see.

**The store left where it is, and its edge to `panel/page` accepted as an exception.** One exception
is all it takes for `storage` to stop meaning "below `panel`", and then the rule is a preference
again.

**Gfx folded into the same effort.** `libraries/Gfx` repeats the pattern — `gardener::gfx` flat over
`layout/`, `shapes/` and the rest — and the rule applies there for the same reason. It is left out
because it touches every scene in the panel and every file in Gfx, and a change that has to be
reviewed on its own is its own plan.

## The entry points it has to hold on

- **Runtime's 33 sources** declare their types inside `namespace gardener` and name `Machine`,
  `SystemState`, `Routines`, `CommandOutbox`, `Logger`, `Library` and the parts unqualified. Each
  file gains the `using` declarations for the names it actually names, and the parts it implements
  spell `core::Pump` and `core::Buzzer` in their base clauses.
- **The sketch**, `firmware/gardener/gardener.ino` with `PanelRoot.{h,cpp}`, names nearly every
  layer and is the only place that constructs the graph, so it is where a missing include shows up
  first.
- **The unit suite** — the 18 cases and their two fakes under `tools/unit`, 22 files of which name a
  Gardener type — includes the headers by folder path and names the types, so it moves with them.
- **The host harness** — `tools/host` and `tools/berry`, seven files of which name a Gardener type,
  and the two check scripts, whose `board_free()` and `HOST_MODULES` lists name folders as strings.
- **The image** — nothing under `firmware/data` names a C++ namespace, and the six modules register
  under the names a page imports (`screen`, `device`, `vacuum`, `settings`, `ux`, `routines`), which
  is a table name and not a namespace. The `.be` files and the `cfg/*.ini` documents are untouched.
- **The wire** — the domain factories and their reply text are unchanged; only the qualified names
  of the types they take move.

## How it is known to have worked

- `python tools/unit/check.py` — 239 cases and 980 assertions, green after every item and not only
  at the end.
- `python tools/berry/check.py` — 9 of 9 files loaded and 3 of 3 routines driven, and `--memory`
  printing the same `vm 15.0 KB, framework 25.7 KB, image 54.0 KB` it prints today, which is what
  says the layer names cost nothing at run time.
- `python scripts/arduino.py --task build` — the sketch is the only thing that can be wrong about
  the Runtime seam, and it is the one check that reads it.
- Three greps that must come back empty, which is the rule made checkable: no `#include "routines/`
  under `src/panel`, no `#include "panel/` under `src/storage`, and no header whose namespace does
  not end in the path of its folder.
- On the device, `--task upload`: the panel draws its root page, `[E]` stays quiet, and `device get`
  answers the version.

Nothing here changes behaviour. The measurement is that it does not: the same checks, the same
memory report, the same drawing.
