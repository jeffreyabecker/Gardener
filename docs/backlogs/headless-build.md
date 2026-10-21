# Backlog — the machine without the panel

The machine and the panel split into two libraries with a one-way dependency: `VacuumCore` keeps the
vacuum system and its parts, the ownership record, the loggers, the command queue, the control and
interface tasks, and the two links; a new `GardenerPanel` keeps the display, the keys, the buzzer,
the interpreter, the storage and settings, and the panel task. The panel reads and commands the
machine only through the surface a controller uses. Storage, settings and the buzzer are panel; the
machine's knobs — radio, diagnostics, battery gauge — are configuration the composition root hands
it.

Both links become pluggable. The serial link already speaks through domains; the BLE link gains the
same idea, so each link's surface is whatever the build registers. The machine registers its own
domains — `control`, `vacuum`, `device`, and the core BLE characteristics — and the panel registers
its own — `fs`, `buzzer`, the BLE `unit` field, and the `device` `unit`/`log` fields. One capability
record, composed at build time and served identically by both links, tells a host which assemblies
are fitted and how the device stops itself.

The settled safety rule: a machine with no panel may hold pressure only while a controller owns it,
and losing the owner releases the chamber and returns the device to unowned. The full device keeps
its Back hold and its present semantics.

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
- [ ] 3.7 Move the press classifier and the OK-during-hold go-home into the panel's page reader,
  drained on the panel beat.
- [ ] 3.8 Move the panel's I²C bring-up and the `display.begin()` abort out of the machine's boot
  into the composition that fits a panel.
- [ ] 3.9 Make the machine's boot succeed with no display fitted.
- [ ] 3.10 Move the buzzer tick out of the control beat to an installable tick the composition
  provides.
- [ ] 3.11 Verify the full device boots and behaves identically after the seams land.

## Epic 4 — The panel library

The panel code moves into `GardenerPanel`, which depends on the machine library and nothing the
other way. The panel's domains, safety reader and buzzer tick are registered from its own
composition root.

- [ ] 4.1 Create `GardenerPanel` beside `VacuumCore`.
- [ ] 4.2 Move the panel's HAL parts and their impls — buttons, display, buzzer — into it.
- [ ] 4.3 Move `Overlay`, `UI`, `ConnectedUI` and `DisconnectedUI` into it.
- [ ] 4.4 Move `Fonts` and `Asset` into it.
- [ ] 4.5 Move `Routines` and the eight bindings into it.
- [ ] 4.6 Move the jingles and `BuzzerPlayer` into it.
- [ ] 4.7 Move `Ini`, `Settings` and `Storage` into it.
- [ ] 4.8 Move the store bus — client and server — into it, so the panel owns the route from the VM
  to flash.
- [ ] 4.9 Move the panel task, the press queue, the press classifier and the two holds into it.
- [ ] 4.10 Move the panel's serial domains — `fs`, `buzzer` — and the `device` `unit`/`log` fields
  into it, registered from the panel's composition.
- [ ] 4.11 Move the panel's BLE `unit` domain into it, registered from the panel's composition.
- [ ] 4.12 Write the panel's composition root and boot: build the machine root, compose the panel
  onto it, register its domains, and install the safety reader and the buzzer tick.
- [ ] 4.13 Make the machine's task spawn start only the control and interface tasks; the panel
  starts the panel task from its own boot.
- [ ] 4.14 Remove `Tone`, `Play` and `Off` from the machine command queue, leaving `Start`, `Stop`,
  `ResetFault`, `SetTarget`, `ClearTarget` and `Reboot`.
- [ ] 4.15 Route the VM's `buzzer` and `jingles` bindings to the panel's buzzer.
- [ ] 4.16 Verify the machine library includes nothing panel-shaped and links only the ESP32 core
  and NimBLE.

## Epic 5 — The headless build

The machine-alone sketch ships the machine with no panel, and its safety is ownership: it cannot
pressurize while unowned, and it releases when its owner leaves.

- [ ] 5.1 Add the machine-alone sketch with a composition root for the parts, the ownership record,
  the two links and the capability — no panel.
- [ ] 5.2 Let the machine-alone composition leave the battery gauge out for a wired-only unit.
- [ ] 5.3 Compose the machine-alone capability: no panel, battery by build, ownership-stop posture.
- [ ] 5.4 Register only the machine's domains in the machine-alone link composition — no `fs`, no
  `buzzer`, no `unit`.
- [ ] 5.5 Gate pressurization on ownership in the headless build: refuse to raise or hold pressure
  while no live controller owns the device.
- [ ] 5.6 Release the chamber and clear the record when the owning link drops, returning the device
  to unowned and acquirable.
- [ ] 5.7 Ship no data image for the machine-alone build.
- [ ] 5.8 Teach [arduino.py](../../scripts/arduino.py) to build, upload and monitor the second sketch
  and skip its data task.
- [ ] 5.9 Verify the machine-alone build compiles and links without `GardenerPanel`, `berry` and the
  Adafruit drivers.

## Epic 6 — The contract

The specs record what the firmware now does: the composed wire surface, the capability, and the
headless safety rule.

- [ ] 6.1 Record in [serial-protocol.md](../specs/serial-protocol.md) that the `fs` and `buzzer`
  domains and the `unit`/`log` fields exist only where the panel does.
- [ ] 6.2 Record in [serial-protocol.md](../specs/serial-protocol.md) that `batt_v`/`batt_pct` are
  answered by the battery capability.
- [ ] 6.3 Record in [serial-protocol.md](../specs/serial-protocol.md) that a panel-less device
  requires ownership to pressurize.
- [ ] 6.4 Record in [bluetooth.md](../specs/bluetooth.md) that the `unit` field exists only where
  the panel does and `batt` follows the battery capability.
- [ ] 6.5 Record in [bluetooth.md](../specs/bluetooth.md) that a panel-less device refuses to
  pressurize while unowned.
- [ ] 6.6 Record in [vacuum-control.md](../specs/vacuum-control.md) that a machine without a panel
  holds pressure only while owned and releases the chamber when its owner leaves.
- [ ] 6.7 Record in [gestures.md](../specs/gestures.md) that a build without keys has no device
  gestures, and its stop is ownership.
- [ ] 6.8 Record in [README.md](../../README.md) that the device ships in two builds and what each
  is for.

## Epic 7 — Verification

- [ ] 7.1 Build and flash the full device; verify its serial and BLE surfaces are unchanged.
- [ ] 7.2 Verify the full device's settings editor, routines and jingles still work after the
  library move.
- [ ] 7.3 Build the machine-alone device; verify `device get` reports `caps` and no `display-size`,
  `fs`, `buzzer` or `unit`, and the BLE `caps` characteristic agrees.
- [ ] 7.4 Verify a machine-alone device refuses to pressurize while unowned.
- [ ] 7.5 Verify a machine-alone device releases the chamber when the owning link drops mid-run.
- [ ] 7.6 Verify a wired-only unit reports no battery and omits `batt_v`/`batt_pct` and the BLE
  `batt`.
- [ ] 7.7 Verify `VacuumCore` builds and links without `GardenerPanel`.
- [ ] 7.8 Measure the machine-alone image size and record it in the parking lot's firmware-size
  section ([parking-lot.md](../plans/parking-lot.md)).
