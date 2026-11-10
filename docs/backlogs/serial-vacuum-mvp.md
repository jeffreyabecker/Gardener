# Serial vacuum MVP

The steps of [the serial vacuum plan](../plans/serial-vacuum-mvp.md), in the order they land. Each
item is one branch, one commit and one diff, and leaves the tree building with the suite green.
Branches are `backlog/serial-vacuum-mvp/main`, `backlog/serial-vacuum-mvp/epic-<n>/main` and
`backlog/serial-vacuum-mvp/epic-<n>/task-<m>`, merged back with `--no-ff` as each item lands.

The order is the deletions': the wire shrinks first, because it is the only surface that stays and
the rest is judged against it; the composition root then stops naming the panel, so the panel can
be deleted; and the VM, the display library and the harnesses go only once nothing includes them.
Every item is checked with whatever can see it — `python tools/unit/check.py` for the C++, and
`python scripts/arduino.py --task build` where the item touches what the suite does not compile.

## Epic 1 — the wire

- [x] **Trim `vacuum get` to the five fields it reports.** `get` answers
      `pressure_kpa`, `target`, `state`, `duty` and `fault`; the `hold`, `max_run`, `floor`,
      `sensor` and `up` fields go, and with `max_run` the document read behind it, so `get` is
      handed the vacuum system rather than the machine and the settings. `tools/unit/cases/
      vacuum_commands.cpp` drops the two cases that assert the removed fields and keeps the rest.
- [x] **Collapse `target` and `start` into `vacuum set`.** One verb: refuse a token that is not a
      number or past `ControlFloorKpa` with `err out_of_range`, refuse with `startRefusal()`'s
      code as `err not_ready`, and otherwise queue the target and a start whose hold is
      `ControlMaxRunMs` in seconds — no document is read. `vacuum target` and `vacuum start` stop
      being verbs, and the hold argument and its four refusal cases go with them.
- [x] **Rename `vacuum stop` to `vacuum vent`, and keep `vacuum reset`.** `vent` is the one
      release path and `reset` clears the fault latch, which nothing else does. The suite's `stop`
      case becomes `vent`, and `reset` keeps its case.
- [x] **Delete the four grammars and shrink `SerialParts`.** `SerialDevice`, `SerialBuzzer`,
      `SerialFiles`, `SerialGesture` and `BoardFacts` are deleted, `SerialParts` holds the machine
      and nothing else, and `Gardener.h` and the sketch stop naming the board's facts. The
      `device`, `buzzer`, `fs` and `gesture` tokens answer the link's unknown-domain error, and
      `device_commands.cpp`, `fs_commands.cpp` and `gesture_commands.cpp` go — with the harnesses
      that built links to those grammars, `tools/berry`, `tools/walkthrough` and
      `tools/host/session`, which cannot compile once the parts they drive are gone.

## Epic 2 — the composition and the panel

- [x] **Compose the device in the sketch and delete the panel.** `gardener.ino` builds the logger,
      the queue, the state, the parts, the machine and the link itself, and spawns the control and
      interface tasks; `PanelRoot.h/.cpp` are deleted and the pins block is `MachinePins` alone.
      The panel goes with it, because the C++ include graph will not have it any other way:
      `panel/`, `bindings/`, `routines/`, `storage/`, the `ButtonsTick`, `RoutinesTick` and
      `PartsTick` beats, the panel's `core/` parts, `Runtime`'s `PanelTasks`, `RtosGestureQueue`,
      `RtosInternTable`, `FileLibrary` and the four hal impls, `Runtime.h` trimmed to what is left.
      `tools/host/capture`, `tools/unit/suite.*` and the cases that drove the panel, the store or
      the VM go with them, and `check.py` stops compiling the capture display.
- [x] **Remove the published state and the reboot from the machine.** `core/SystemState.h` and
      `MachineTick`'s publish go, the interface task stops writing the connection flag,
      `MachineCommand::Reboot`, `takeReboot`, `SafeReboot` and `Machine.h`'s `Capability` byte go,
      and `machine_tick.cpp` loses the three cases that assert them. The machine's control law is
      untouched.

## Epic 3 — the tree that isn't the firmware

- [x] **Delete the VM and its skill.** `libraries/berry` goes, and with it the `berry` skill under
      `.github/skills/berry/` and the line in `.github/copilot-instructions.md` that points at its
      host check: nothing includes `<berry.h>` and nothing runs the image, because the routines went
      with the panel.
- [x] **Delete the display library and the cases that drew with it.** `libraries/Gfx`, `tools/panel`
      and `tools/fonts` go, and `tools/unit/check.py` compiles `libraries/Gardener` alone: no Gfx
      source, no Gfx include and no drawing case.
- [x] **Delete the data image, its assets and the rig sketches.** `firmware/data`,
      `firmware/gardener/assets`, `scripts/asset.py`, `firmware/test_buttons` and
      `firmware/test_pressure` go, and nothing reads a `.be`, a `.ini` or a `.bmp`.

## Epic 4 — the build

- [x] **Trim `scripts/arduino.py` to the four tasks.** The Berry archive handling (`archive.py`,
      `archived_libraries`, `pack_archives`, `drop_stale_archives`) and the `data` and `archive`
      tasks go, with `DATA_DIR`; `build`, `upload`, `monitor` and `clean` stay, and so do
      `write_knob_defaults` and `write_rig_header`, because the machine's knobs are still how a
      rig is fitted. The script's docstring and `--task` choices say what is left.

## Epic 5 — the documents

- [x] **Rewrite the README for the serial-only device.** The device described is the machine, the
      vent valve and one serial line: the three verbs, the kPa-below-ambient units, the bounded
      run and the fail-safe valve, with the panel, the image, the VM and the data task gone from
      the build and the documentation index.
- [ ] **Delete the documents whose subject is gone.** The `headless-machines`, `redo-berry-drawing`
      and `text-entry-scene` plans, the `text-entry-scene` backlog, the walkthroughs guide and the
      three panel and display references are deleted, and `docs/README.md` loses the sections that
      indexed them. This backlog and its plan are not among them; they go with the last item.

When the last item lands, delete this file and the plan it breaks down.
