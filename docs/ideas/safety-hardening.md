# Safety hardening — Gardener

Status: review summary, 2026-10-23. Not a contract and not an implementation plan.

## Why this exists

OpenPump is adding support for the Gardener as a second pump driver. Reviewing the
[Bluetooth contract](../specs/bluetooth.md) and the [vacuum control](../specs/vacuum-control.md)
against the way OpenPump commands a pump turned up a set of device-side gaps: places where
the machine relies on a well-behaved controller, or where a failure is reported as a safe
state when it is not.

This document records those gaps and the changes that would close them, for the Gardener
project to accept, reject or re-order. It is a summary of concerns and suggestions, not a
commitment and not a step-by-step plan.

**What is already strong** and should not be weakened:

- the normally-open vent valve is a hardware guarantee: de-energised vents;
- the fault latch owns the state until `resetFault()`, and `fail()` cuts the pump and opens
  the valve;
- `CONTROL_MIN_TARGET_KPA` refuses a target that would regulate around ambient;
- a claim always vents, and a departing owner vents on the control beat;
- every write is read-back-authoritative, and a refused value is not echoed;
- there is no raw-duty command: `target` is the only actuator;
- the pump driver clamps duty to `PUMP_DUTY_MAX_PERCENT` even if a setting were higher.

Most of what follows is about making the safe path independent of the controller, not about
changing the control law.

## Summary

| # | Concern | Suggested change | Priority |
|---|---|---|---|
| 1 | A stop can be refused, dropped or unavailable locally | Stop before the ownership gate; priority stop path; panel Cancel always; idempotent stop | P0 |
| 2 | No device-side maximum depth | `CONTROL_MAX_TARGET_KPA`; refuse past `floor`; mechanical relief | P0 |
| 3 | No run deadline | `CONTROL_MAX_RUN_MS` and/or a hold duration; controller heartbeat | P0 |
| 4 | An incomplete vent is reported as `IDLE` | Latch `vent_timeout` / `sensor`; never `IDLE` above the margin | P0 |
| 5 | A stale start can undo a stop | Refuse start while venting; run epoch/lockout | P0 |
| 6 | Ownership is checked before the command is applied | Carry the owner epoch in `MachineCommand`; re-check on apply | P0 |
| 7 | Just Works pairing with no local authorization | Pairing window, passkey or physical confirm; bond cap | P0 |
| 8 | The fail-safe valve is not reported and is remotely writable | `caps` bit; make `valve` local-only | P0 |
| 9 | A target past reach pins duty forever | No-progress timeout | P1 |
| 10 | Overshoot is trusted to the PID | Overshoot cutoff | P1 |
| 11 | Settings are loosely bounded and remotely writable | Bound `deadband`, `flow`, `volume`, `pump_max`; local-only commissioning fields | P1 |
| 12 | Refused writes are silent | `write_result` / `last_error` notification | P1 |
| 13 | No telemetry sequence or uptime on BLE | `seq` or `uptime_ms` characteristic | P1 |
| 14 | Settings are written off the control task | Post setting changes as `MachineCommand`s | P1 |
| 15 | No thermal or continuous-duty protection | Run-time/thermal derating | P1 |
| 16 | A low battery cannot refuse a start | `low_battery` refusal; gauge on machine-alone builds | P1 |
| 17 | No local record of what happened | Last-N event log in NVS | P1 |
| 18 | Two failure modes have no independent bound | Physical kill switch; independent pressure switch | hardware |

---

## P0 — before this device is used on a body

### 1. A stop must be impossible to refuse or lose

**Today.**

- [`MachineBleDomain::execute()`](../../libraries/VacuumCore/src/ble/MachineBleDomain.cpp) (line 366)
  returns `kResultNotControlling` before it looks at the opcode. A non-owner's `stop` is
  refused. (A second bonded controller can still claim the top, and a claim vents, but the
  stop opcode itself should not depend on ownership.)
- The control queue is depth 8 and
  [`postMachineCommand()`](../../libraries/VacuumCore/src/Tasks.cpp) (line 109) ignores the result
  of `xQueueSend`. Eight queued targets followed by a stop means the stop is dropped.
- [`PanelTasks.cpp`](../../libraries/PanelLink/src/PanelTasks.cpp) line 61 gates Cancel-5s on
  `ownedBy(panel)`, so the panel cannot force-quit while a phone owns the machine.
- [`VacuumSystem::stop()`](../../libraries/VacuumCore/src/VacuumSystem.cpp) (line 72) calls
  `release()` again when already venting, which resets `ventStartMs_` and the 30-second
  vent timeout.

**Risk.** The one command that makes the machine safe is the one command allowed to fail.

**Suggestion.**

- Handle `kOpStop` before the ownership check and accept it from any bonded link.
- Give stop a dedicated priority path: an atomic `stopRequested` flag checked at the top of
  the control tick before the queue drain, or `xQueueSendToFront` plus a checked result.
  Log every dropped command.
- Make panel Cancel-5s always stop and vent, regardless of owner. Keep OK+Cancel reboot as
  the harder reset.
- Make `stop()` idempotent: do not restart the vent timer when already venting.

**Compatibility.** Behavioural only; existing clients see a stop succeed where it used to
return `0xFE`. Document it as a safety fix, not a protocol break.

### 2. Add a device-side maximum depth

**Today.** [`Machine.h`](../../libraries/VacuumCore/src/Machine.h) line 16 sets only the
shallowest target. [`VacuumSystem::setTarget()`](../../libraries/VacuumCore/src/VacuumSystem.cpp) (line 112)
accepts anything deeper, including far past `floor`; the comment says the duty pins at the
cap and the run never arrives. `floor` is a reported setting, not enforced by `setTarget`.
OpenPump's ceiling is entirely app-side, so a buggy or foreign controller can command any
depth.

**Risk.** The ceiling invariant depends on the controller. A device-side limit is the only
one a bad client cannot exceed.

**Suggestion.**

- Add `CONTROL_MAX_TARGET_KPA` as a compile-time constant (not wire-writable) and refuse or
  clamp in `setTarget()`, the BLE handler, the serial handler and `MachineSettings::load()`.
- Refuse targets deeper than `floor` instead of accepting them and pinning duty — or add a
  reach timeout (P1 item 9).
- Add an independent hardware bound: a mechanical relief/bleed valve or a pressure switch
  that cuts pump power at the limit. Firmware can hang; a spring cannot.

**Compatibility.** A target that used to be accepted may now be refused. That is the point;
publish the new `max` alongside the existing `min` in the wire reply.

### 3. Add a run deadline

**Today.** [`VacuumSystem.h`](../../libraries/VacuumCore/src/VacuumSystem.h) states the
design: "The run's duration is not a fact about the vacuum system and is not here: the
device holds until it is told to stop, and nothing times a run." A phone that crashes but
keeps the BLE link open leaves the pump holding indefinitely. The only local escape is the
five-second reboot gesture.

**Risk.** An unattended hold is bounded only by the phone, the battery, or a person noticing.

**Suggestion.**

- Add `CONTROL_MAX_RUN_MS` and auto-stop/vent when it expires, independent of the
  controller. Make it a build/commissioning constant, not a wire-writable setting.
- Better: extend the command contract with a hold duration (`start` carries seconds, or a
  `hold_s` characteristic), so the device ends the hold at the requested time even if the
  phone dies.
- Add a controller heartbeat: if the owning link sends no command/target/keep-alive for N
  seconds while `PULL` or `HOLD`, release. This is the only way to catch a hung app over a
  live link.

**Compatibility.** A device timer is a behaviour change to "the hold's duration is yours".
Choose a default above any legitimate OpenPump hold and expose it in the status reply.

### 4. Do not report an incomplete vent as `IDLE`

**Today.** [`released()`](../../libraries/VacuumCore/src/VacuumSystem.cpp) (line 81) returns true
after `VENT_TIMEOUT_MS` even if the pressure never fell, and `service()` then calls
`rest()` → `Idle`. A leaky rig or a blocked vent becomes "idle" with vacuum still applied.
The sensor check in `service()` covers `running || targetVent_`, so a sensor failure during
a release is not faulted; the release can time out to `Idle` with no evidence.

**Risk.** A client that treats `state == IDLE` as "vented" is wrong, and the app's vent
confirmation loses its strongest signal.

**Suggestion.**

- On vent timeout, latch `fault=vent_timeout`, keep the valve open, and require a reset.
  Never enter `Idle` while `kpa > VENT_MARGIN_KPA`.
- On sensor loss during a release, latch `fault=sensor` rather than timing out to `Idle`.
- Report the fault code over BLE and serial.

**Compatibility.** New fault codes; `state` may stay `FAULT` where it used to become
`IDLE`. Clients must already handle a latched fault.

### 5. Prevent a stale start from undoing a stop

**Today.** [`start()`](../../libraries/VacuumCore/src/VacuumSystem.cpp) (line 51) closes the valve
and pulls even from `VENT`, abandoning the release. A duplicate or delayed `start` after a
stop can re-pressurize the chamber. There is no post-stop lockout and no run token.

**Risk.** A retry, a queued command or a reconnecting client can restart the pump after the
person stopped it.

**Suggestion.**

- Refuse `start` while `VENT`, or for a short `CONTROL_RESTART_LOCKOUT_MS` after any stop.
- Add a run epoch/sequence byte to commands; reject a start whose epoch is not newer than
  the last stop. This also covers duplicate commands across reconnects.
- If resume-from-vent is wanted, make it an explicit opcode with a token, not a bare start.

**Compatibility.** A protocol addition. Old clients that send a bare `start` during a vent
will be refused; that is the intended change.

### 6. Close the ownership race

**Today.** [`execute()`](../../libraries/VacuumCore/src/ble/MachineBleDomain.cpp) (line 366)
checks `owned(link)` on the interface task, then posts a `MachineCommand` to the control
task, which applies it later without re-checking ownership
([`drainMachineCommands`](../../libraries/VacuumCore/src/Tasks.cpp), line 70). A link that lost the
top between check and apply can still get a start applied.

**Risk.** A command from an evicted controller can run after the hand-over vent.

**Suggestion.**

- Carry the owner epoch/link id in `MachineCommand` and re-check it in
  `applyMachineCommand()`.
- Drain ownership changes before commands, or make ownership and command application one
  serialized step.

**Compatibility.** Internal; no wire change.

### 7. Require authorization to pair

**Today.** [`BleLink::begin()`](../../libraries/VacuumCore/src/ble/BleLink.cpp) (line 38) uses
Just Works, bonded, with `BLE_HS_IO_NO_INPUT_OUTPUT`. The code's own comment calls it "a
guard against a casual neighbour at the bench rather than an attacker with a radio." Once
bonded, a device can claim (vent) and command pressure.

**Risk.** Anyone within radio range can pair and command the machine. Physical proximity is
not authorization for a device that applies vacuum to a body.

**Suggestion.**

- Open a pairing window only on a physical gesture (panel key/button), not permanently.
- Prefer Passkey Entry or Numeric Comparison with the code shown on the panel, or require a
  physical confirm.
- Cap the number of bonds and provide a local "forget all bonds" action.

**Compatibility.** Pairing UX changes; existing bonds can be kept.

### 8. Make the fail-safe valve a reported, protected property

**Today.** `valve` is a `WRITE_ENC` characteristic
([`kSpecs`](../../libraries/VacuumCore/src/ble/MachineBleDomain.cpp), line 60). A remote write
changes the setting, and the valve part is chosen from it at the next boot
([`Machine.cpp`](../../libraries/VacuumCore/src/Machine.cpp)). `caps` has no bit for the
fitted direction.

**Risk.** A bonded client can silently remove the one hardware guarantee — de-energised
vents — with a write that takes effect on the next power cycle.

**Suggestion.**

- Add a `caps` bit for **fail-safe valve fitted** (normally open), set from the actual valve
  at boot. OpenPump can then refuse a non-fail-safe build for body use.
- Remove `valve` from the BLE table, or make it serial/panel-only with a physical unlock and
  a persistent warning.
- Keep the normally-open valve as the default and document that a normally-closed build
  inverts the fail-safe.

**Compatibility.** Additive `caps` bit; a restricted characteristic.

---

## P1 — defense in depth

### 9. No-progress timeout

If `PULL` at high duty produces no depth change for T seconds, stop and latch
`fault=no_progress`. This catches a blocked hose, a dead pump, a stuck sensor, and a target
deeper than reach. It is the firmware counterpart to refusing targets past `floor`.

### 10. Overshoot cutoff

If `kpa > target + margin` for several consecutive readings, stop/vent and latch
`fault=overshoot`. The PID is currently trusted not to overshoot; a sensor spike, a valve
failure or a retuned loop can break that trust. OpenPump has a comparable optional
protection; the device should not depend on it.

### 11. Bound the settings and restrict who can write them

- `deadband` accepts any value > 0. A 100 kPa deadband makes `atTarget()` true at 0 kPa.
  Bound it (suggest 0.1–2 kPa).
- `flow` and `volume` are any value > 0; implausible values destabilize the loop. Bound
  them to a plausible range derived from the fitted hardware.
- `pump_max` accepts up to `PUMP_DUTY_MAX_PERCENT` (90). Set a body-safe compiled default,
  cap the writable value there, and allow lowering only — never raising.
- `floor` is only bounded below by `CONTROL_MIN_TARGET_KPA`; an inaccurate floor makes the
  app's clamp wrong. Treat it as a commissioning value.
- Make `flow`, `volume`, `floor`, `deadband`, `pump_max` and `valve` serial/panel or
  physical-unlock only. A bonded phone should not be able to retune the loop.

### 12. Make refusals explicit

A refused `target` or setting write is silent today; the client must read back. Add a
`write_result`/`last_error` notification (or make the characteristic notify the unchanged
value plus a result code) so a refusal cannot be mistaken for success. Keep the read-back as
the authority; this is an additional signal, not a replacement.

### 13. Put a sequence or timestamp on BLE telemetry

`up` exists on serial but not on the GATT table, and notifications carry no sequence. Add an
`uptime_ms` characteristic or a `seq` byte so a client can order `HOLD`/`VENT`/command
results against its own writes. This strengthens vent evidence and makes reconnects
unambiguous.

### 14. Apply settings on the control task

`flow`, `volume`, `floor`, `valve`, `pump_max` and `deadband` are written from the interface
task in `applyPending()`, while the control task reads them. Post them as `MachineCommand`s
so one task owns the machine state and the "applied on the next pass" contract holds for
every field.

### 15. Thermal and continuous-duty protection

`pump_max` is a duty cap, not a thermal limit. A long hold at 90% can overheat the pump. Add
a maximum continuous run time, a current/temperature sense, or a derating curve. At minimum,
document the pump's rated continuous duty and cap `pump_max` accordingly.

### 16. Refuse a start on a low battery

`startRefusal()` should return `low_battery` when a gauge is fitted and below a threshold. A
brownout mid-hold must vent (normally-open valve) or latch a fault. Fit a gauge to
machine-alone builds so this is possible without a panel.

### 17. Keep a small event log

Record the last N claims, commands, stops, transitions and faults in NVS, readable over
serial and BLE diagnostics. This is not a control, but it is what makes an incident
understandable after the fact.

---

## Hardware that bounds the firmware

- **Physical stop/kill switch** that cuts pump power and opens the vent valve without the
  MCU. This is the only stop that survives a firmware hang.
- **Mechanical relief/bleed valve** set to the safe maximum depth.
- **Independent pressure switch** in series with pump power.
- **Normally-open valve mandatory** for body use; report it in `caps`.
- **Hardware watchdog and brownout detector**; on reset, pump off and valve open (already
  the boot path).
- **Battery gauge** on machine-alone builds, so low-battery refusal is possible.

## Contract and versioning

- Prefer **additive** changes: new `caps` bits, `write_result`, `uptime`/`seq`, `hold_s`.
  Bump `ble_proto` to 2 only for breaking semantics (stop without ownership, new result
  codes). OpenPump's planned driver refuses anything other than `1`, so a bump needs the
  app updated in lockstep.
- Publish the new maximum target and the vent-timeout fault in the status/serial replies, so
  a client can discover them rather than assuming.
- Update [bluetooth.md](../specs/bluetooth.md) and
  [vacuum-control.md](../specs/vacuum-control.md) with the new guarantees and the new fault
  codes.
- Add a hardware checklist for the new guarantees: stop without ownership, stop from a full
  queue, vent timeout faults, run deadline, pairing authorization, fail-safe `caps` bit.

## Suggested first patches

If the whole list is too much at once, these three are small, local and remove the sharpest
edges:

1. **Stop before ownership, and a priority stop path** — `MachineBleDomain::execute` and
   `Tasks.cpp`; plus panel Cancel always.
2. **Vent timeout and sensor loss during a release latch a fault** — `VacuumSystem::released`
   and `VacuumSystem::service`.
3. **A hard `CONTROL_MAX_TARGET_KPA`** — `Machine.h`, `VacuumSystem::setTarget`, the two wire
   handlers and `MachineSettings::load`.

They are independent of the contract additions and can ship before the pairing and protocol
work.

## Open questions

1. What is the body-safe maximum depth for this pump and chamber? A number chosen for the
   reference rig may not fit another.
2. What run deadline is long enough for legitimate holds and short enough to matter? A
   heartbeat interval is a related choice.
3. Is resume-from-vent a real use case? If not, refusing `start` while venting is simpler
   and safer than an epoch token.
4. Is a passkey display acceptable on the panel, or is a physical pairing button the only
   flow someone will complete?
5. Can the hardware take a mechanical relief valve and a kill switch without changing the
   enclosure? If not, the firmware bounds carry all the weight.
