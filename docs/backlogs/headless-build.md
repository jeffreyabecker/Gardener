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
- [ ] 5.2 Split `AppContext` into a machine context (sensor, pump, vent, vacuum, ownership,
  capabilities, config) and a panel context (buttons, display, buzzer, battery, overlay, routines,
  storage, settings, store bus ends).
- [ ] 5.3 Re-route the machine's code onto the machine context — `App`, `Tasks`, `SerialLink` and
  its machine domains, `MachineBleDomain`, the control and interface task bodies — so nothing in
  the machine names a panel part.
- [ ] 5.4 Re-route the panel's code onto the panel context plus the machine context it reads —
  `ConnectedUI`, `DisconnectedUI`, `Overlay`, `Routines` and its bindings, and the panel task — and
  move the battery update and the buzzer tick from the control beat to the panel beat, removing the
  `TickService` seam.
- [ ] 5.5 Move the store server drain from the interface beat to the panel beat — the panel task
  drains the store queue after the VM poll — so the interface task names no store part.
- [ ] 5.6 Split the `device` domain: the machine reports `proto`/`fw`/`chip`/`heap`/`caps`, and the
  panel registers the `batt`/`unit`/`log` fields.
- [ ] 5.7 Register the `fs` and `buzzer` serial domains from the panel composition instead of the
  machine's.
- [ ] 5.8 Move the BLE `batt` characteristic from the machine domain to the panel domain, and
  register the panel's BLE fields — `unit` and `batt` — from the panel composition instead of the
  machine's.
- [ ] 5.9 Split the boot: the machine boots its parts, links and tasks; the panel boots storage,
  settings, the VM, the panel and its registrations; the full build's composition calls both in
  order.
- [ ] 5.10 Make the machine's task spawn start only the control and interface tasks; the panel
  starts the panel task from its own boot.
- [ ] 5.11 Remove `Tone`, `Play` and `Off` from the machine command queue, and route the `buzzer`
  serial domain and the VM's `buzzer` and `jingles` bindings to the panel's buzzer directly.
- [ ] 5.12 Verify the full device boots and behaves identically after the split, with the panel
  code still in `VacuumCore`.

### Phase B — move the panel into `GardenerPanel`

- [ ] 5.13 Move the panel's HAL parts and their impls — buttons, display, buzzer.
- [ ] 5.14 Move `Overlay`, `UI`, `ConnectedUI` and `DisconnectedUI`.
- [ ] 5.15 Move `Fonts` and `Asset`.
- [ ] 5.16 Move `Routines` and the eight bindings.
- [ ] 5.17 Move the jingles and `BuzzerPlayer`.
- [ ] 5.18 Move `Ini`, `Settings` and `Storage`.
- [ ] 5.19 Move the store bus.
- [ ] 5.20 Move the panel task, the button reader and the panel composition root.
- [ ] 5.21 Move the panel's serial domains (`fs`, `buzzer`) and the panel's BLE domain (`unit`,
  `batt`).
- [ ] 5.22 Write the panel's composition root and boot in `GardenerPanel`: build the machine root,
  compose the panel onto it, and register its domains.
- [ ] 5.23 Verify the machine library includes nothing panel-shaped and links only the ESP32 core
  and NimBLE.

## Epic 6 — The headless build

The machine-alone sketch ships the machine with no panel. Its stack is `None` alone, so the safety
Epic 4 built is its whole story: an unowned machine vents, and a dropped owner vents. What remains
is the second build itself.

- [ ] 6.1 Add the machine-alone sketch with a composition root for the pressure loop, the ownership
  stack, the serial and Bluetooth links and the capability — no panel, so nothing pushes itself
  above `None`.
- [ ] 6.2 Compose the machine-alone capability: no panel, no battery, ownership stop.
- [ ] 6.3 Register only the machine's domains in the machine-alone link composition — no `fs`, no
  `buzzer`, no `unit`, no `batt`.
- [ ] 6.4 Ship no data image for the machine-alone build.
- [ ] 6.5 Teach [arduino.py](../../scripts/arduino.py) to build, upload and monitor the second sketch
  and skip its data task.
- [ ] 6.6 Verify the machine-alone build compiles and links without `GardenerPanel`, `berry` and the
  Adafruit drivers.

## Epic 7 — The contract

The specs record what the firmware now does: the stack, the composed wire surface, and the safety
rule every build shares.

- [ ] 7.1 Record in [serial-protocol.md](../specs/serial-protocol.md) that `control get` answers
  serial, bluetooth, panel or none — the top of the stack — and that `acquire` always succeeds, with
  no `busy` reply.
- [ ] 7.2 Record in [bluetooth.md](../specs/bluetooth.md) that the `control` characteristic answers
  serial, bluetooth, panel or none, and that the `unit` and `batt` fields exist only where the panel
  does.
- [ ] 7.3 Record in [serial-protocol.md](../specs/serial-protocol.md) that the `fs` and `buzzer`
  domains and the `unit`/`log` fields exist only where the panel does, and that `batt_v`/`batt_pct`
  are answered by the panel's battery.
- [ ] 7.4 Record in [vacuum-control.md](../specs/vacuum-control.md) that the chamber vents whenever
  the top of the stack changes — a claim, a release or a drop — and that a machine without a panel
  holds pressure only while owned.
- [ ] 7.5 Record in [gestures.md](../specs/gestures.md) that Back releases control and Back+OK is the
  reboot, and that a build without keys has no gestures, its stop being ownership.
- [ ] 7.6 Record in [README.md](../../README.md) that the device ships in two builds and what each
  is for.

## Epic 8 — Verification

- [ ] 8.1 Build and flash the full device; verify the serial and BLE surfaces match the contract,
  including the ownership report naming the owner.
- [ ] 8.2 Verify the full device's settings editor, routines and jingles still work, and that the
  panel is the default controller with Back releasing and Back+OK rebooting.
- [ ] 8.3 Verify the chamber vents on claim, release and drop on the full device.
- [ ] 8.4 Build the machine-alone device; verify `device get` reports `caps` and no `display-size`,
  `fs`, `buzzer`, `unit` or `batt`, and the BLE `caps` characteristic agrees.
- [ ] 8.5 Verify a machine-alone device refuses to pressurize while unowned and vents when the
  owning link drops mid-run.
- [ ] 8.6 Verify `VacuumCore` builds and links without `GardenerPanel`.
- [ ] 8.7 Measure the machine-alone image size and record it in the parking lot's firmware-size
  section ([parking-lot.md](../plans/parking-lot.md)).
