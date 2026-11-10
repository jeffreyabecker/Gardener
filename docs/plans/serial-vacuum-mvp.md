# A serial vacuum, nothing else

The MVP is the machine and one wire in front of it: a controller opens the serial link, sets a
pressure, and vents. Everything that is not that goes — the Berry image and the machines it
registers, the panel and every part of it, the settings and the filesystem they live on, and any
notion of a second link or a second task. There is no Bluetooth in the tree to remove: the only
interface is, and stays, the USB CDC serial, so this is a deletion, not a port, and the board stays
the ESP32-S3 it is built for.

The product is [Machine.h](../../libraries/Gardener/src/machine/Machine.h) and
[VacuumSystem.h](../../libraries/Gardener/src/machine/VacuumSystem.h) as they stand — the pressure
loop, the deadline, the fault latch and the fail-safe valve — behind a trimmed serial surface. The
machine's control law is not changed; what is removed is everything that reached it through the
panel or the VM.

## What is there now, and what goes

The tree is laid out in layers, and the MVP keeps the three that are the device and deletes the
rest.

**Stays.**

- `libraries/Gardener/src/machine/` — `Machine`, `VacuumSystem`, `MachineCommand`, `CommandQueue`.
  This is the whole product and is untouched except for the two dead ends named below.
- `libraries/Gardener/src/serial/SerialLink.h/.cpp` and `SerialVacuum.cpp` — the link and the one
  grammar that remains.
- `libraries/Gardener/src/beats/MachineTick.h/.cpp` — the control beat that drains the queue,
  services the chamber and logs the run.
- `libraries/Gardener/src/core/` — the device drivers and the small utilities: `Pump`,
  `PressureSensor`, `VentValve`, `RunState`, `Logger`, `SerialLogger`, `Knob`, `knob_defaults`,
  `Units`, `Math`, `firmwareversion`.
- `libraries/Runtime` — `MachineRoot` (`buildMachineParts`), `MachineTasks` (the control and
  interface tasks), `RtosCommandQueue`, and the three hal parts the machine is wired through:
  `PwmPump`, `XgzpSensor`, `SolenoidValve`.
- `scripts/arduino.py` — build, upload, monitor and clean, plus the knob and rig-header generation.
- `tools/knob.py` and `tools/unit/check.py` — the knob defaults and the host check, both narrowed.

**Goes.**

- `libraries/berry` — the vendored VM and the archive step that precompiled it.
- `libraries/Gfx` — the display library, fonts, blend and the capture picture.
- `libraries/Gardener/src/bindings/` — the native modules the VM reached.
- `libraries/Gardener/src/routines/` — `BerryInterpreter`, `Routines`, `Interpreter`, the loader.
- `libraries/Gardener/src/panel/` — the page stack, the browser, the settings scenes, the gesture
  queue and the buzzer player.
- `libraries/Gardener/src/beats/ButtonsTick`, `RoutinesTick`, `PartsTick` — the panel's beats.
- `libraries/Gardener/src/storage/` — `Config`, `DeviceSettings`, `RoutineSettingStore`,
  `SettingKind`, `JingleLibrary`: the settings and the filesystem they rode on.
- `libraries/Gardener/src/core/Battery`, `Button`, `ButtonPanel`, `Buzzer`, `Gestures`,
  `SystemState` — the panel's parts and the published state nothing reads once the panel is gone.
- `libraries/Gardener/src/serial/SerialDevice`, `SerialBuzzer`, `SerialFiles`, `SerialGesture`,
  `BoardFacts` — the four grammars that go with their subjects.
- `libraries/Runtime/PanelTasks`, `RtosGestureQueue`, `RtosInternTable`, `storage/FileLibrary`,
  `machine/SafeReboot`, and the hal impls `AdcBattery`, `Buttons`, `PwmBuzzer`, `Sh1106Device`.
- `firmware/data` — the image: the `.be` routines, the system pages, the `.ini` documents.
- `firmware/gardener/assets` — the bitmaps.
- `firmware/gardener/PanelRoot.h/.cpp` — the panel composition root.
- `firmware/test_buttons` and `firmware/test_pressure` — the rig test sketches, with the parts they
  exercised.
- `tools/berry`, `tools/walkthrough`, `tools/panel`, `tools/fonts` — with their layers.

**No Bluetooth.** Nothing under `firmware/`, `libraries/` or `scripts/` names Bluetooth, NimBLE or
a BLE peripheral; the only transport is the chip's USB-Serial-JTAG CDC. The plan therefore removes
nothing for it and states, as the entry point, that the MVP has no radio and no way to add one
without a new plan.

**Documents.** The plans and references whose subject is deleted go with it: `headless-machines.md`
and `redo-berry-drawing.md` and `text-entry-scene.md` under `docs/plans/`, the
`docs/backlogs/text-entry-scene.md` that breaks the last one down, and the panel and display
references under `docs/reference/`. The parts guide and the hardware ideas stay, because the
machine's parts survive.

## The shape of the change

### The composition root

`firmware/gardener/gardener.ino` becomes the whole composition, in place of `PanelRoot`. It names
the three machine pins and nothing else — no I2C, no piezo, no gauge, no key register — builds the
parts with `buildMachineParts`, the machine over them, the `RtosCommandQueue`, the serial link, and
spawns the two tasks that remain, control and interface. `SerialParts` shrinks to the machine alone,
so the link no longer holds a battery, a settings document, a buzzer, a file store or a gesture
queue. The board's pins constant loses everything but `MachinePins`.

### The wire

`SerialLink` serves one grammar, `vacuum`, with four verbs:

| command | answer |
| --- | --- |
| `vacuum get` | `ok pressure_kpa=<x> target=<y> state=<word> duty=<z> fault=<code\|none>` |
| `vacuum set <kPa\|->` | `ok target=<kPa> state=PULL`; `err out_of_range field=target value=<v> min=0.0 max=<floor>`; `err not_ready <code>` |
| `vacuum vent` | `ok state=VENT` |
| `vacuum reset` | `ok state=IDLE` — clears a latched fault |

`set` is the existing `target` and `start` in one command: it stores the target, runs it, and grants
the machine's ceiling as the hold. `vent` is the existing `stop`, the one release path. `reset` is
the existing `reset` and stays because the fault latch is sticky and nothing else clears it: without
it a device that faulted once is dead until it is power-cycled. `get` is the existing `get` trimmed
to what the verbs need watched — the reading, the target, the state, the duty and the fault; the
document's `hold` and `max_run` die with `DeviceSettings`, so the run is now always bounded by the
machine's own ceiling, `ControlMaxRunMs`, and the `floor`, `sensor` and `up` fields go with the host
that no longer needs them. A target is still a depth below ambient in kPa, 0 or `-` is still the
ambient target, and the floor and the fail-safe valve behaviour are unchanged.

The four deleted grammars stop existing, not just stop being served: the `device`, `buzzer`, `fs`
and `gesture` tokens answer the link's unknown-domain error the way any other unknown token does,
and their files are deleted. The log lines the `SerialLogger` writes continue to share the stream,
so a controller that reads only `ok`/`err` lines already ignores them.

### The machine

The machine itself is untouched: the PID, the deadline, the fault latch, the vent floor and the
release criterion all stay. Two dead ends go with the panel: the `Reboot` op and
`libraries/Runtime/src/machine/SafeReboot.cpp`, since no surface issues a reboot now, and the
`Capability` byte and `setCapabilities`, since nothing sets or reads a capability bit. `MachineTick`
stops publishing `SystemState`, and the interface task stops writing the connection flag into it,
because the panel was the only reader.

### The build

`scripts/arduino.py` loses the Berry archive handling (`archive.py`, `archived_libraries`,
`pack_archives`, `drop_stale_archives`) and the `--task data` and `--task archive` tasks, which
packed the image and rebuilt the archive. `--task build`, `upload`, `monitor` and `clean` stay, and
so do `write_knob_defaults` and `write_rig_header`: the machine's knobs and the rig's numbers are
still how a different pump or chamber is fitted.

### The host check

`tools/unit/check.py` compiles `libraries/Gfx` and `libraries/Gardener` today. With Gfx deleted it
compiles Gardener alone, and its case list narrows to the machine and serial cases: the Gfx cases,
the capture display under `tools/host/capture` and the panel cases under `tools/unit/cases/` go with
their layers, and the machine and the two remaining serial verbs keep their cases. Nothing under
`bindings/` is compiled today, and nothing changes there except that the folder no longer exists.

## The alternatives, and why not

**Keep `target`, `start` and `stop` as three verbs.** That is the wire as it exists, and a
controller that already speaks it keeps working. It is set aside because the ask is exactly two
actions: a set that aims and runs, and a vent. `set` collapses the two, and a separate `start`
promises a run-a-target-again step the MVP does not need; it can come back with the first routine.

**Hold a set indefinitely until told to vent.** The machine's safety model is that every run is
bounded, and a controller that walks away must not hold the pump. The plan keeps the bounded run and
the ceiling, so `set` vents on its own after `ControlMaxRunMs`; unbounded hold is a new deadline
model and a new plan, not a rename.

**Keep `device get` as the identity verb.** It reports the board and the capability byte, which a
minimal host has no use for, and it drags `BoardFacts` and the caps along. The plan drops it; the
firmware version is still in the log at boot if a host needs it.

**A second sketch beside the old one.** Keeping the panel sketch while a headless one is cut would
preserve the old device, but the ask is to rip the panel and the VM out, and the two compositions
would share a link and a machine that this plan is simplifying. One sketch is the product.

**Delete only the panel, keep the filesystem.** The filesystem exists for the image and the
settings, and both go; a store with nothing to read is a mount failure and a `fs` grammar with
nothing to serve. The machine needs no persisted value.

## The entry points it has to hold on

- **The wire.** `vacuum get`, `vacuum set` and `vacuum vent`, with the same `ok`/`err` framing, the
  same kPa-below-ambient units, the same `0`/`-` ambient target and the same `err out_of_range`
  bounds as today. A controller that spoke the old `vacuum` verbs is one rename away.
- **The fail-safe valve.** De-energised vents, on the fitted normally-open solenoid, and nothing in
  the strip may change which coil level means open.
- **The deadline.** Every run ends at `ControlMaxRunMs` or a fault, with the chamber released, even
  when a controller stops talking.
- **The sensor.** The release criterion is the vent floor the sensor reads, not zero; the machine's
  `VentFloorKpa` and `VentTimeoutMs` are what make `vent` finish.

## How it is known to have worked

- `python tools/unit/check.py` compiles the remaining C++ and runs the machine and serial cases:
  a `set` queues a target and a start, a `vent` queues a stop, an out-of-range target is refused,
  and the vacuum system still regulates, faults and releases as its cases describe.
- `python scripts/arduino.py --task build` compiles with no Gfx, no Berry and no data image.
- Uploaded to the board, `vacuum get` reports the ambient reading and `state=IDLE`; `vacuum set 30`
  pulls to 30 kPa below ambient and holds; `vacuum set -` and `vacuum vent` release the chamber;
  unplugging the pump with the valve energised vents it, and a set left to expire releases on its
  own.
- The deleted folders are gone, not just unlinked: a grep for `berry`, `Scene`, `ButtonPanel`,
  `Routines` and the display drivers finds nothing left in the include graph.

## What could go wrong

- **The log and the wire share the stream.** `SerialLogger` writes boot and run lines between the
  replies, as it does today; a host that parses everything instead of skipping to `ok`/`err` sees
  noise. This is the existing contract, kept.
- **`set` is a run, not a state.** A controller that treats a set as permanent finds the chamber
  vented at the ceiling. The `get` reply's `state` is the truth, and the deadline is a feature.
- **The ambient target is not "off".** `0` and `-` are the released setpoint, the same as today, and
  `vent` is the release; a host that sends `0` expecting the pump to stop gets a vent, which is the
  same thing by the machine's own definition.
- **Deleting a layer deletes its knobs.** `GARDENER_KNOB_VALID` calls live in the files they name;
  deleting the panel removes its knobs from `knob_defaults.h` on the next `--task build`, and the
  machine's knobs are the ones that must survive. `tools/knob.py` runs before the compile, so the
  header and the tree cannot drift.
- **The host check narrows by deletion.** The Gfx and panel cases are removed with their sources;
  if a case file is left that includes a deleted header, `check.py` fails to compile, which is the
  check that the include graph is really clean.
- **The rig test sketches name deleted parts.** `test_buttons` and `test_pressure` are deleted
  rather than left half-working, so a half-deleted core cannot compile one of them into a pass.
