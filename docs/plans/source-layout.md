# Source layout

Status: proposal. Nothing has moved; this records the shape `libraries/VacuumCore/src` should take
and the rule that keeps it there. It leaves when the move is in the tree.

`core/` and `hal/` are still what they were: four foundations and one abstract class per part.
`app/` is not. It has drifted back to 17 units at one level — the loop, the machine, the screens,
the filesystem, the settings, the logger and the reboot path all sitting beside each other — with
three subfolders beside them. A reader opening the folder sees a list of nouns, not a structure.

## 1. The proposed tree

```
libraries/VacuumCore/src/
├── VacuumCore.h
├── core/                 Config.h  Math.h  Parse.h  Units.h            (unchanged)
├── hal/                  the eight part interfaces
│   └── impl/             the seven fitted parts                        (unchanged)
└── app/
    ├── App.{h,cpp}               the loop
    ├── AppContext.h              the graph as interfaces
    ├── Build.{h,cpp}             the composition root
    ├── VacuumSystem.{h,cpp}      the machine: chamber and run
    ├── Ownership.h               who may command it
    ├── BuzzerPlayer.{h,cpp}      the piezo's patterns
    ├── SafeReboot.{h,cpp}        release, then restart
    ├── StreamLogger.h            diagnostics on Serial
    ├── panel/       UI.h  ConnectedUI  DisconnectedUI  Overlay  Fonts  Asset
    ├── storage/     Storage  Ini  Settings
    ├── routines/    Routines  Bindings
    ├── serial/      SerialLink  Domain.h  Gate.h  domains/{Buzzer,Control,Device,Fs,Vacuum}
    └── ble/         BleLink
```

`app/` root goes from 17 flat units to 8. Nothing in `core/` or `hal/` moves, and the dependency
rule in the [project README](../../README.md) — `core/` sees `core/`, `hal/` sees `core/`, `app/` sees
everything — is unchanged.

## 2. What each folder is

| Folder | What it is | Why it is one |
| --- | --- | --- |
| `panel/` | everything that draws on the glass | all six compile against `hal/Display`; the machine reads none of them |
| `storage/` | what the device remembers | `Storage` is the mount and the handles, `Ini` is the text placed in it, `Settings` is the one file written that way |
| `routines/` | the Berry runtime and the modules a routine sees | `Routines` is a VM and a press queue, `Bindings` is its seven native modules; neither is a screen, and `Bindings.cpp` is 604 lines that do not belong beside the loop |
| `serial/` | the wire | already right; only `Domains/` becomes `domains/` so the tree does not mix cases |
| `ble/` | the radio | its own contract, not a second encoding of the serial one |

The eight units left at the root are the instrument itself, and each is one file's worth of
concept: the loop, the graph, the composition root, the machine, the ownership record, the sound,
the reboot path and the log. None of them has a sibling, and a folder holding one file would be a
folder that exists to look organised.

Two names are deliberately not used:

* `store/` for the storage folder — the Berry `store.*` module is a different thing (a routine's own
  values) and lives in `routines/`.
* `disconnected-ui/` — the old name says where the code is used, not what it is. The VM runs behind
  the idle page today; it is still a VM. `routines/Routines.cpp` also matches
  [specs/routines.md](../specs/routines.md) and the serial `routines` surface.

## 3. Where the next file goes

1. A pin, a bus or a PWM channel → `hal/`, and `hal/impl/` if it names a vendor header.
2. A constant every layer may name → `core/Config.h`, still the only thing there that is.
3. It touches the glass → `panel/`. It reads or writes a file → `storage/`. It enters the VM →
   `routines/`. It decodes a transport → `serial/` or `ble/`.
4. Otherwise it is the instrument → `app/` root, in its own file.

A folder must hold at least two units that change together. If it would hold one, it is a file.
That is the whole rule, and it is the one that failed here: `disconnected-ui/` had two files and
earned its place, but nothing said where the ninth flat file went, so everything went flat.

This rejects `ui/`, `common/`, `utils/`, `services/` and `helpers/` by name. `ui/` in particular is
a drawer, not a boundary: every file in this tree has a user interface of some kind.

## 4. What applying it costs

| Work | Scope |
| --- | --- |
| Physical moves | 26 files, `git mv` so history follows; `Domains/` → `domains/` is a case-only rename |
| Include lines | 41 in 23 files, all `#include "app/..."` with a path that changes |
| Links | 8 in [README.md](../../README.md) (the architecture section), 1 in [panel-vm-task.md](./free-rtos/panel-vm-task.md), 1 prose mention in [RLEBitmap/README.md](../../libraries/RLEBitmap/README.md) |
| Verification | `powershell -File scripts/arduino.ps1 -Task build` — nothing else checks the tree, so the include rewrite is the risk |

The moves within `app/` do not touch a single constructor, body or field: this is a directory
change plus the include strings. Nothing outside `libraries/VacuumCore/src` includes these paths
except the links above.

## 5. Alternatives considered

* **Leave the screens flat and only create `storage/` and `routines/`.** Cheaper, but leaves
  `ConnectedUI`, `DisconnectedUI`, `Overlay`, `UI`, `Fonts` and `Asset` interleaved with `App` and
  `Build`, which is the lumping this is meant to stop.
* **`machine/` for `VacuumSystem`, `Ownership`, `BuzzerPlayer`, `SafeReboot`.** Four files, four
  unrelated jobs, one drawer. The vacuum system is the machine; none of the others are.
* **Split `panel/` into `panel/` and `screens/`.** Six small units, all drawing to the same
  interface. The second folder would exist to hold two files.
* **Fold the drivers back into the classes that use them.** That is the opposite move, and the
  README explains why the boundary is there.
