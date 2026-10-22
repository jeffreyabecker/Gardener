# Backlog — the machine without the panel

The machine and the panel split into two libraries with a one-way dependency: `VacuumCore` keeps the
vacuum system and its parts, the ownership stack, the loggers, the command queue, the control and
interface tasks, and the serial and Bluetooth links; a new `GardenerPanel` keeps the display, the
keys, the buzzer, the battery, the interpreter, the storage and settings, and the panel task. The
panel is itself a link: it reads and commands the machine only through the surface a controller
uses. Storage, settings, the buzzer and the battery are panel; the machine's knobs — radio,
diagnostics — are configuration the composition root hands it.

Both links become pluggable. The serial link already speaks through domains; the BLE link gains the
same idea, so each link's surface is whatever the build registers. The machine registers its own
domains — `control`, `vacuum`, `device`, and the core BLE characteristics — and the panel registers
its own — `fs`, `buzzer`, the BLE `unit`/`batt` fields, and the `device` `batt`/`unit`/`log`
fields. One capability record, composed at build time and served identically by both links, tells a
host which assemblies are fitted and how the device stops itself.

The settled safety rule is ownership, everywhere: the machine holds pressure only while the link on
top has it running, and any change of the top — a claim, a release or a drop — vents the chamber. A
panel-less build's stack is `None` alone, so an unowned machine is always vented; the full device
pushes the panel above `None`, so the panel is the default controller and the device's own stop is
the panel releasing.

The epics are worked in order, and every task leaves the tree building. Check a task off as it
lands; the unticked tasks are what remains.

## Epic 1 — Pluggable links

The domain concept reaches both links. Serial is refactored to register its domains instead of
hard-wiring them; BLE gains the same abstraction and is split into a machine domain and a panel
domain. This is the foundation: the capability record and the library split both build on it, so it
comes first.

- [x] 1.1 Move the serial domain population out of `SerialLink`'s constructor, so the link starts
  empty and exposes registration.
- [x] 1.2 Register the five serial domains from `buildAppContext()`, so the full device's serial
  surface is unchanged.
- [x] 1.3 Give the BLE link a domain concept mirroring the serial `Domain`: a registered piece that
  declares its characteristics — UUID suffix and properties — answers reads, applies writes, and
  publishes on its cadence.
- [x] 1.4 Split the fixed BLE field table into two domains: the machine domain (`state`, `kpa`,
  `target`, `duty`, `fault`, `command`, `fw`, `ble_proto`, `control`, `sensor`, `floor`, `batt`)
  and the panel domain (`unit`).
- [x] 1.5 Build the GATT service and its characteristics from the registered domains, keeping every
  UUID suffix and property unchanged.
- [x] 1.6 Route reads through the owning domain's answer.
- [x] 1.7 Route writes through the owning domain's apply, keeping the pending-slot and
  last-write-wins behavior.
- [x] 1.8 Route publish and its cadence through the owning domain, keeping 5 Hz `kpa`/`duty`, 10 s
  `batt`, and on-change for the rest.
- [x] 1.9 Keep the `command` opcode executor with the machine domain, preserving its result bytes.
- [x] 1.10 Register the machine's BLE domain in `buildAppContext()`.
- [x] 1.11 Register the panel's BLE domain (`unit`) in `buildAppContext()`.
- [x] 1.12 Build and flash the full device; verify its serial surface is unchanged.
- [x] 1.13 Verify the BLE table, UUIDs, rates and opcode results are unchanged.

## Epic 2 — The capability record

One machine-side value says which assemblies are fitted and how the device stops itself, and both
links serve it.

- [x] 2.1 Add one machine-side capability value — panel, battery, safety posture — set by the
  composition root and read by both links.
- [x] 2.2 Add a read-only `caps` characteristic to the machine's BLE domain.
- [x] 2.3 Print `caps` from `device get` and drop `display-size` from the reply.
- [x] 2.4 Compose the capability in the full build's root: panel fitted, battery fitted, local stop
  present.
- [x] 2.5 Record the changed `device` surface in [serial-protocol.md](../specs/serial-protocol.md).
- [x] 2.6 Record the `caps` characteristic in [bluetooth.md](../specs/bluetooth.md).
- [x] 2.7 Verify `device get` and the BLE `caps` characteristic report the same value.

## Epic 3 — The machine's seams

The machine stops reading settings, keys and the buzzer directly, and its boot stops owning the
panel's bring-up. Each seam lands while everything still lives in one library, so the full device
behaves identically after the epic.

- [x] 3.1 Hand the machine its knobs — radio, diagnostics, battery gauge — as configuration from the
  composition root.
- [x] 3.2 Read the radio knob in `BleLink::begin()` from that configuration.
- [x] 3.3 Read the diagnostics knob from configuration when the boot picks the logger.
- [x] 3.4 Have the full build's root read `settings.ini` and pass the values, so the full device's
  behavior is unchanged.
- [x] 3.5 Give the control beat one installable local-stop slot — a verdict, stop or reset, applied
  on the same beat.
- [x] 3.6 Move the two holds — Back 3 s stop, Back+OK 5 s reset — into a safety reader the panel
  installs into the slot.
- [x] 3.7 Keep the press classifier in the control beat — the button driver is single-consumer — so
  the panel's page reader drains the presses the control beat queues, on the panel beat.
- [x] 3.8 Move the panel's I²C bring-up and the `display.begin()` abort out of the machine's boot
  into the composition that fits a panel.
- [x] 3.9 Make the machine's boot succeed with no display fitted.
- [x] 3.10 Move the buzzer tick out of the control beat to an installable tick the composition
  provides.
- [x] 3.11 Verify the full device boots and behaves identically after the seams land.

## Epic 4 — The control stack

The one-owner record with its stale flag becomes a stack of links. `None` sits at the bottom and
vents; a panel build pushes the panel above it at boot, so the panel is the default controller;
serial and Bluetooth push themselves when they claim and are removed when they release or drop.
Whoever is on top drives, and any change of the top — a claim, a release or a drop — vents the
chamber. The buttons leave the control task for the panel: the two holds become panel gestures —
Back releases control, Back+OK is the reboot the wire already serves.

- [x] 4.1 Add `Panel` to the `Link` enum, beside `None`, `Serial` and `Bluetooth`.
- [x] 4.2 Replace `Ownership`'s single owner and stale flag with a stack: `None` at the bottom,
  `acquire` pushes and always succeeds, `release` and `drop` remove, and the top is the owner. The
  `busy` reply and the stale flag go with it.
- [x] 4.3 Vent the chamber whenever the top changes — a claim, a release or a drop — so a new
  controller always starts from an open chamber.
- [x] 4.4 Push the panel at boot in a panel build, so it is the default controller beneath every
  external link.
- [x] 4.5 Report the top as the owner: `control get` and the BLE `control` characteristic answer
  serial, bluetooth, panel or none.
- [x] 4.6 Move the press classifier from the control beat to the panel beat, so the control task
  reads no keys.
- [x] 4.7 Make the two holds panel gestures — Back releases control, Back+OK issues `device reboot` —
  and remove the installable stop slot and the safety reader.
- [x] 4.8 Verify the full device holds and releases through the stack, and that a vanished serial or
  BLE owner vents the chamber.

## Epic 5 — The library split

The one `AppContext` graph becomes two: a machine context and a panel context. The split has to
come first — arduino-cli compiles each library against only its own `src/`, so the panel files
cannot move into `GardenerPanel` while the machine still includes them. Phase A splits the context
and re-routes the boot, the tasks and the wire surface, all inside `VacuumCore`; the build keeps
working because nothing has moved yet. Phase B then moves the panel files into `GardenerPanel`,
which depends on `VacuumCore`.

### Phase A — split the context

- [x] 5.1 Create the empty `GardenerPanel` shell — `library.properties` with `depends=VacuumCore` —
  so the destination exists before anything moves.
- [x] 5.2 Split `AppContext` into a machine context (sensor, pump, vent, vacuum, ownership,
  capabilities, config) and a panel context (buttons, display, buzzer, battery, overlay, routines,
  storage, settings, store bus ends).
- [x] 5.3 Re-route the machine's code onto the machine context — `App`, `Tasks`, `SerialLink` and
  its machine domains, `MachineBleDomain`, the control and interface task bodies — so nothing in
  the machine names a panel part.
- [x] 5.4 Re-route the panel's code onto the panel context plus the machine context it reads —
  `ConnectedUI`, `DisconnectedUI`, `Overlay`, `Routines` and its bindings, and the panel task — and
  move the battery update and the buzzer tick from the control beat to the panel beat, removing the
  `TickService` seam.
- [x] 5.5 Move the store server drain from the interface beat to the panel beat — the panel task
  drains the store queue after the VM poll — so the interface task names no store part.
- [x] 5.6 Split the `device` domain: the machine reports `proto`/`fw`/`chip`/`heap`/`caps`, and the
  panel registers the `batt`/`unit`/`log` fields.
- [x] 5.7 Register the `fs` and `buzzer` serial domains from the panel composition instead of the
  machine's.
- [x] 5.8 Move the BLE `batt` characteristic from the machine domain to the panel domain, and
  register the panel's BLE fields — `unit` and `batt` — from the panel composition instead of the
  machine's.
- [x] 5.9 Split the boot: the machine boots its parts, links and tasks; the panel boots storage,
  settings, the VM, the panel and its registrations; the full build's composition calls both in
  order.
- [x] 5.10 Make the machine's task spawn start only the control and interface tasks; the panel
  starts the panel task from its own boot.
- [x] 5.11 Remove `Tone`, `Play` and `Off` from the machine command queue, and route the `buzzer`
  serial domain and the VM's `buzzer` and `jingles` bindings to the panel's buzzer directly.
- [ ] 5.12 Verify the full device boots after the split, with the panel code still in `VacuumCore`.
- [ ] 5.13 Verify both wire surfaces are unchanged after the split: serial's `device`, `fs` and
  `buzzer` answers, and Bluetooth's `batt` and `unit`.
- [ ] 5.14 Verify the panel still drives by default after the split: Back releases, Back+OK reboots,
  and settings, routines and jingles work.

### Phase B — move the panel into `GardenerPanel`

The composition splits before any file moves. `arduino-cli` puts a local library on the include path
only when the sketch reaches it, and `VacuumCore` cannot include `GardenerPanel`, so the full build
moves panel-side and the sketch includes `GardenerPanel.h` first. After that each item moves one set
of files that nothing left in `VacuumCore` names, so only one copy of each header is ever on the
path.

- [x] 5.15 Extract the machine half of `buildAppContext()` into a machine root in `VacuumCore`.
- [x] 5.16 Move the panel half of the composition into `GardenerPanel`'s root, which builds the
  machine root and composes the panel onto it.
- [x] 5.17 Leave `machineBoot` in `VacuumCore` behind a machine-only header, so a build without a
  panel can call it.
- [x] 5.18 Move `panelBoot`, `App` and `AppContext` into `GardenerPanel`.
- [x] 5.19 Include `GardenerPanel.h` from the sketch, so the panel library is on the include path
  and the full device still boots.
- [x] 5.20 Move the panel task, its press reader and `spawnPanelTask` into `GardenerPanel`, and
  remove them from `Tasks.h` with the button include.
- [x] 5.21 Move `PanelContext` into `GardenerPanel`, once no machine file names it.
- [x] 5.22 Move the button interface and driver — `hal/Button.h`, `hal/ButtonPanel.h`,
  `hal/impl/Buttons.{h,cpp}` — into `GardenerPanel`.
- [x] 5.23 Move the display interface and its Adafruit impl — `hal/Display.h`,
  `hal/impl/AdafruitDisplay.h` — into `GardenerPanel`.
- [x] 5.24 Move the buzzer interface and its PWM impl — `hal/Buzzer.h`, `hal/impl/PwmBuzzer.{h,cpp}`
  — into `GardenerPanel`.
- [x] 5.25 Move the battery interface and its ADC impl — `hal/Battery.h`,
  `hal/impl/AdcBattery.{h,cpp}` — into `GardenerPanel`.
- [x] 5.26 Move `Overlay` into `GardenerPanel`.
- [x] 5.27 Move the panel pages — `UI.h`, `ConnectedUI` and `DisconnectedUI` — into `GardenerPanel`.
- [x] 5.28 Move `Fonts` into `GardenerPanel`.
- [x] 5.29 Move `Asset` into `GardenerPanel`.
- [x] 5.30 Move `Routines` into `GardenerPanel`.
- [x] 5.31 Move the panel's counters — `stopSeq` and `goHomeSeq` — out of `Tasks.h` into the
  panel's own header.
- [x] 5.32 Move the binding surface — `bindings/Module.h`, `bindings/Modules.h`,
  `bindings/Registry.cpp` and `Bindings.{h,cpp}` — into `GardenerPanel`.
- [x] 5.33 Move the `vacuum`, `screen` and `device` bindings into `GardenerPanel`.
- [x] 5.34 Move the `buttons`, `buzzer` and `jingles` bindings into `GardenerPanel`.
- [x] 5.35 Move the `store` binding into `GardenerPanel`.
- [x] 5.36 Move `BuzzerPlayer` into `GardenerPanel`.
- [x] 5.37 Move `Storage` into `GardenerPanel`.
- [x] 5.38 Move `Ini` into `GardenerPanel`.
- [x] 5.39 Move `Settings` into `GardenerPanel`.
- [x] 5.40 Move the store bus and its queue handle into `GardenerPanel`, removing the handle from
  `Tasks.h`.
- [x] 5.41 Move the panel's serial domains — `fs` and `buzzer`, with their factory declarations —
  into `GardenerPanel`, leaving `Domains.h` naming only the machine's.
- [x] 5.42 Move the panel's BLE domain — `PanelBleDomain`, with `unit` and `batt` — into
  `GardenerPanel`.
- [x] 5.43 Verify `VacuumCore` includes nothing panel-shaped.
- [x] 5.44 Verify `VacuumCore` links only the ESP32 core and NimBLE.

## Epic 6 — The headless build

Each sketch is its own composition root. The links a build serves, the domains it registers and the
tasks it starts are named in the sketch, so the two builds differ where a reader can see them rather
than behind a library flag: the full device composes the panel onto the machine and starts three
tasks, and the machine-alone build starts two and registers no panel domain. The libraries keep the
parts, the domains and the task bodies; the sketch says which of them this build has.

The machine-alone sketch ships the machine with no panel. Its stack is `None` alone, so the safety
Epic 4 built is its whole story: an unowned machine vents, and a dropped owner vents.

### Phase A — the full device's sketch takes over its setup

- [x] 6.1 Move the serial link out of `machineBoot`: `App::begin` builds the `SerialLink` and
  registers the machine's `control` and `vacuum` domains on it, and `machineBoot` brings the
  machine's parts up and reports them.
- [x] 6.2 Move the BLE link out of `machineBoot` the same way, and delete the `MachineBoot` struct
  now that no link is handed back.
- [x] 6.3 Move the panel's domain registrations out of `panelBoot` and into `App::begin`, so
  `panelBoot` only brings the panel up and reports what came up.
- [x] 6.4 Move the three task starts out of the boots and into `App::begin`, so the boots bring
  parts up and nothing more.
- [x] 6.5 Move `App::begin`'s body into `gardener.ino` and delete `App`, leaving the sketch as the
  full device's composition root.

### Phase B — the machine-alone sketch

- [x] 6.6 Add a machine-alone sketch that includes only `VacuumCore.h` and builds its own
  `MachineContext` from `buildMachineRoot()`.
- [x] 6.7 Compose the machine-alone capability: no panel, no battery, ownership stop.
- [x] 6.8 Add a machine-side `device` domain in `VacuumCore` answering `proto`, `fw`, `chip`, `heap`
  and `caps` and serving `reboot`, with no battery, log or unit field, and register it in place of
  the panel's in the machine-alone sketch.
- [x] 6.9 Fix the machine-alone radio and diagnostics knobs in the sketch rather than read them from
  settings, which that build does not mount.
- [x] 6.10 Teach [arduino.py](../../scripts/arduino.py) to compile the machine-alone sketch.
- [x] 6.11 Teach [arduino.py](../../scripts/arduino.py) to upload and monitor the machine-alone
  sketch.
- [x] 6.12 Have [arduino.py](../../scripts/arduino.py) refuse the data task for the machine-alone
  sketch, which ships no data image.
- [x] 6.13 Verify the machine-alone build compiles and links without `GardenerPanel`, `berry` and
  the Adafruit drivers.

## Epic 7 — The contract

The specs record what the firmware now does: the stack, the composed wire surface, and the safety
rule every build shares. One fact per item, so each line can be checked against the code alone.

- [x] 7.1 Record in [serial-protocol.md](../specs/serial-protocol.md) that `control get` answers
  serial, bluetooth, panel or none — the top of the stack.
- [x] 7.2 Record in [serial-protocol.md](../specs/serial-protocol.md) that `control acquire` always
  succeeds, and that the `busy` reply is gone.
- [x] 7.3 Record in [bluetooth.md](../specs/bluetooth.md) that the `control` characteristic answers
  serial, bluetooth, panel or none.
- [x] 7.4 Record in [bluetooth.md](../specs/bluetooth.md) that `unit` and `batt` exist only where
  the panel does.
- [x] 7.5 Record in [serial-protocol.md](../specs/serial-protocol.md) that the `fs` and `buzzer`
  domains exist only where the panel does.
- [x] 7.6 Record in [serial-protocol.md](../specs/serial-protocol.md) that the `unit` and `log`
  fields exist only where the panel does.
- [x] 7.7 Record in [serial-protocol.md](../specs/serial-protocol.md) that `batt_v` and `batt_pct`
  are answered by the panel's battery.
- [x] 7.8 Record in [vacuum-control.md](../specs/vacuum-control.md) that the chamber vents whenever
  the top of the stack changes — a claim, a release or a drop.
- [x] 7.9 Record in [vacuum-control.md](../specs/vacuum-control.md) that a machine without a panel
  holds pressure only while owned.
- [x] 7.10 Record in [gestures.md](../specs/gestures.md) that Back releases control and Back+OK is
  the reboot.
- [x] 7.11 Record in [gestures.md](../specs/gestures.md) that a build without keys has no gestures,
  its stop being ownership.
- [x] 7.12 Record in [README.md](../../README.md) that the device ships in two builds and what each
  is for.
- [x] 7.13 Bring the rest of [serial-protocol.md](../specs/serial-protocol.md) onto the stack: the
  connecting rules and the worked examples that still answer `ok control=owned`.
- [x] 7.14 Bring the remaining specs onto the stack: the Back+OK lines in
  [vacuum-control.md](../specs/vacuum-control.md) and [bindings.md](../specs/ui/bindings.md) that say
  a reset clears a held ownership record, and the Bluetooth introduction's one-link claim.

## Epic 8 — Verification

- [ ] 8.1 Build and flash the full device.
- [ ] 8.2 Verify the full device's serial surface matches the contract, including `control get`
  naming the owner and an `acquire` that never answers `busy`.
- [ ] 8.3 Verify the full device's BLE surface matches the contract, including the `control` and
  `caps` characteristics.
- [ ] 8.4 Verify the full device's settings editor still edits and persists.
- [ ] 8.5 Verify the full device's routines and jingles still run.
- [ ] 8.6 Verify the panel is the default controller: Back releases and Back+OK reboots.
- [ ] 8.7 Verify the chamber vents on a claim, on a release and on a drop on the full device.
- [ ] 8.8 Build and flash the machine-alone device.
- [ ] 8.9 Verify the machine-alone `device get` reports `caps` and no `display-size`, `fs`,
  `buzzer`, `unit` or `batt`.
- [ ] 8.10 Verify the machine-alone `caps` characteristic agrees with the serial report, and that
  `unit` and `batt` are absent.
- [ ] 8.11 Verify the machine-alone device refuses to pressurize while unowned.
- [ ] 8.12 Verify the machine-alone device vents when the owning link drops mid-run.
- [ ] 8.13 Measure the machine-alone image size and record it in the parking lot's firmware-size
  section ([parking-lot.md](../plans/parking-lot.md)).
