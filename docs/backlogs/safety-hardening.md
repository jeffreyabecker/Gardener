# Backlog — safety hardening

The tasklist for [the safety hardening plan](../plans/safety-hardening.md): the device's safe path
made independent of the controller. Each item is numbered `epic.item`, so every one has a stable
name for its branch and its commit. An item names the code that is wrong now, what is true when it
lands, and the check that shows it — the plan's problem and solution, cut to the size of one
commit.

The epics are worked in order and every item leaves the tree building. The spec line a behaviour
changes is written in the commit that changes it, so Epic 11 is the read-back rather than the only
place the contract is written.

## Epic 1 — The stop path

- [x] 1.1 Accept `vacuum stop` from a link that is not controlling. Today `VacuumDomain::execute` in
  `serial/domains/Vacuum.cpp` gates `vacuum stop` behind `requireControl`, so a non-owner's stop is
  refused. Route `stop` before the gate and answer it exactly as an owner's stop is answered today.
  Check: `control release` then `vacuum stop` answers `ok state=VENT`, never `err not_controlling`.

- [x] 1.2 Accept the stop opcode from a bonded link that is not controlling. Today
  `MachineBleDomain::execute` in `ble/MachineBleDomain.cpp` returns `kResultNotControlling`
  before it
  reads the opcode. Handle `kOpStop` before the ownership check. Check: a connected but non-owning
  client writes `0x02` and is notified `0x00`.

- [x] 1.3 Make a failed queue post retry instead of dropping. Today `postMachineCommand` in
  `Tasks.cpp` calls `xQueueSend` with a zero timeout and discards the result, so a command posted
  while the queue is full is lost. Keep the queue as the only way into the machine — it is the one
  path that reaches the one task writing the vacuum system — and return the send result; each link
  holds a failed command and requeues it on its next poll. Check: a command posted while the queue
  is full is queued again on the next poll and applied — delayed, never lost.

- [x] 1.4 Collapse the queued commands before applying them. Today `drainMachineCommands` in
  `Tasks.cpp` applies every queued command in order, so a burst of target changes each opens and
  closes the valve in turn and a stop can be preceded by a start that runs after it. When the queue
  is drained, collapse it: a stop discards every command queued before it, and several
  pressure-setting commands collapse to the last one so intermediate targets are never applied; log
  what was collapsed. Check: queue start, target 40, target 20, then stop in one beat — the start
  and both targets never run and the log names all three; queue start then target 40 then target 20
  without a stop — only target 20 is applied and the log names target 40.

- [x] 1.5 Make a stop idempotent while releasing. Today `VacuumSystem::stop` in `VacuumSystem.cpp`
  calls `release` again when already venting, which resets `ventStartMs_` and restarts the vent
  timeout. A stop sent while `VENT` must leave the timer alone. Check: mid-release, send stop — the
  run log's `VENT -> IDLE after N ms` line is not extended.

- [x] 1.6 Stop and vent from the panel's Cancel hold whatever link drives. Today `buttonsTick` in
  `PanelTasks.cpp` gates the five-second Cancel on `ownedBy(panel)`, so the panel cannot stop the
  machine while a phone owns it. Drop the gate on the quit gesture. Check: while a phone drives,
  Cancel held five seconds vents the chamber and returns the panel to the readout.

## Epic 2 — The command surface

- [x] 2.1 Make a target of `0` mean vent to ambient and stop. Today `VacuumSystem::setTarget`
  refuses
  anything below `CONTROL_MIN_TARGET_KPA`, and the serial `target` verb uses `-` to clear, so `0` is
  not a target at all. Accept `0` as the ambient target: writing it releases the chamber, and
  `vacuum get` reports `target=0.0` rather than `-`. Keep `-` as a synonym. Check: `vacuum target 0`
  answers `ok target=0.0`, the state goes `VENT` then `IDLE`, and `vacuum get` shows `target=0.0`.

- [x] 2.2 Make a start valid at a target of `0`. Today `VacuumSystem::startRefusal` returns
  `no_target` when the target is zero, and the BLE link maps that to result `0x01`. Remove the
  refusal: a start is refused only for no valid reading or a latched fault. Check: with target `0`,
  `vacuum start` answers `ok` and BLE opcode `0x01` answers `0x00`; result `0x01` is no longer
  produced.

- [x] 2.3 Keep a shallower target venting down to it and a deeper one pulling to it. Today
  `VacuumSystem::setTarget` already routes a shallower target through `startTargetVent` and a deeper
  one to `PULL` while running. Preserve that through 2.4. Check: `HOLD` at 40 kPa, target 20 — the
  state goes `VENT` then `HOLD` at 20; target 50 — the state goes `PULL` toward 50.

- [x] 2.4 Make the vent, start and stop decision in one place. Today `setTarget`, `start`, `stop`
  and `clearTarget` in `VacuumSystem.cpp` each decide for themselves whether to open the valve,
  close
  it or run the pump. Reduce them to one decision — given the command, the state and the reading
  against the target, does the valve open, close, or the pump run — that the wire, the panel and the
  routine bindings all call; remove `clearTarget` or make it an alias of target `0`. Check: both
  sketches build, and 2.1 through 2.3 still hold.

## Epic 3 — The depth the device will pull to

- [x] 3.1 Refuse a target deeper than the fitted `floor`. Today `VacuumSystem::setTarget` accepts
  any
  depth, and both wire handlers pass it through. Refuse a target deeper than
  `settings().floorPressureKpa()` on every path that sets one, naming the bound, and leave the
  target
  unchanged on refusal. Check: `vacuum target <floor+1>` answers `err out_of_range field=target
  max=<floor>`; a deeper BLE write leaves the read-back unchanged.

- [x] 3.2 Clamp a stored `floor` and refuse an out-of-range write. Today `MachineSettings::load` and
  `setFloorPressureKpa` bound `floor` only below. Bound it to 5–100 kPa: a stored value outside that
  range falls back to `CONTROL_FLOOR_KPA` at load, and `device set floor` outside it is refused.
  Check: store `floor=150`, reboot, `device get` reads the default; `device set floor 150` is
  refused.

- [x] 3.3 Make `floor` local-only. Today the `floor` characteristic is writable over Bluetooth.
  Remove the write so a phone cannot move the bound; keep `device set floor` on the serial line.
  Check: a BLE write to `floor` changes nothing and the read-back is unchanged; `device set floor`
  still applies.

## Epic 4 — The deadline on a hold

- [x] 4.1 End a run by itself after fifteen minutes. Today nothing times a run: the comment in
  `VacuumSystem.h` says the device holds until it is told to stop. Add `CONTROL_MAX_RUN_MS` at
  900000 in `Machine.h` and end the run as a release when it expires, with a log line naming the
  deadline; expiry is `VENT` then `IDLE`, not a fault. Check: with the constant shortened on the
  bench, the run releases by itself and the log names the deadline.

- [x] 4.2 Accept a hold in whole seconds on `vacuum start` and refuse one above the maximum. Today
  `VacuumDomain::start` in `serial/domains/Vacuum.cpp` takes no argument. Accept
  `vacuum start [<seconds>]`, end the run at that hold, and refuse a hold above the maximum with
  `err out_of_range field=hold max=<s>`. Check: `vacuum start 5` releases about five seconds later;
  `vacuum start 99999` is refused naming the maximum.

- [x] 4.3 Carry the requested hold over Bluetooth. Add a `hold_s` uint16 characteristic to
  `MachineBleDomain`, written before `start`, and end the run at it. Check: write `hold_s=5`, send
  `0x01`, and the machine releases about five seconds later.

- [x] 4.4 Publish the maximum and the granted hold on both links. Add `max_run=<s>` and `hold=<s>`
  to
  `VacuumDomain::get` (`hold` reads `0` when idle) and `max_run_s` plus `hold_s` reads to
  `MachineBleDomain`. Check: `vacuum get` shows both fields; BLE reads the compiled maximum and the
  granted hold.

## Epic 5 — A release that either completes or latches

- [x] 5.1 Judge a release against the vent floor, not ambient. Today `VacuumSystem::released` is
  satisfied when the depth is within `VENT_MARGIN_KPA` of zero, but the rig's chamber never vents to
  zero. Add `VENT_FLOOR_KPA` in `Machine.h` — the depth the rig settles at with the valve open,
  measured on the bench — and complete a release when the depth is within `VENT_MARGIN_KPA` of it.
  Check: a release ends at the rig's vent floor and `IDLE` is reported only below it.

- [x] 5.2 Latch `vent_timeout` when the vent floor is not reached. Today a release that misses the
  margin falls through to `IDLE` after `VENT_TIMEOUT_MS`. Latch `vent_timeout` and hold `FAULT` —
  pump cut, valve open — until reset. Check: block the vent, observe `FAULT` with `fault
  vent_timeout`, and `state` never reports `IDLE`.

- [x] 5.3 Latch `sensor` when the reading is lost during a release. Today `VacuumSystem::service`
  faults a lost reading only while pulling, holding or venting to a target, so a release with a dead
  sensor times out to `IDLE`. Extend the check to releases. Check: unplug the sensor mid-release —
  `FAULT` with `fault sensor`, not `IDLE`.

- [x] 5.4 Report `VENT`, not `IDLE`, when the machine comes up above the vent floor. Today
  `VacuumSystem::begin` rests to `IDLE` whatever the chamber holds. If the reading is above the vent
  floor at boot, enter `VENT` and hold it to the release timeout. Check: boot with the chamber above
  the floor — the state reads `VENT`.

- [x] 5.5 Report the two release faults through the run log. Today `logRunChanges` in `Tasks.cpp`
  warns `vent timed out` and moves on. Let the fault transition carry the code instead. Check: the
  log shows `VENT -> FAULT (fault vent_timeout)` and the same for `sensor`.

## Epic 6 — The loop's own faults

- [x] 6.1 Latch `no_progress` for a pull that makes no headway. Today a pump running at the duty cap
  against a blocked hose or a stuck sensor is left to the controller to notice. Latch `no_progress`
  when a pull is driven at or above half the duty cap for ten seconds without one kilopascal of
  progress, and release the chamber. Check: block the hose and start a pull — `FAULT` with `fault
  no_progress`.

- [x] 6.2 Latch `overshoot` for a depth that runs past the target. Today the PID is trusted not to
  overshoot. Latch `overshoot` when the depth is more than five kilopascals past the target on five
  consecutive readings while pulling or holding, and release the chamber. Check: step the target far
  shallower — `FAULT` with `fault overshoot`.

## Epic 7 — The fail-safe valve

- [x] 7.1 Report the fail-safe valve in `caps`. Add a fail-safe-valve bit `0x04` to the `Capability`
  enum in `Machine.h`, set it at boot from the fitted valve, and carry it in `device get caps=`. A
  normally-open valve sets it; a normally-closed one does not. Check: `device get` shows `caps` with
  bit `0x04` set on a normally-open build and clear on a normally-closed one.

- [x] 7.2 Make `valve` read-only over the radio. Today the `valve` characteristic is writable, so a
  bonded phone can change the fitted direction for the next boot. Drop the write and keep the read;
  `device set valve` on the serial line still applies at next boot. Check: a BLE write to `valve`
  changes nothing; `device set valve` still works.

## Epic 8 — Tuning, local and bounded

- [x] 8.1 Bound the four loop inputs from one definition. Add bounds in `Machine.h` — flow
  0.05–10 L/min, volume 0.005–5 L, deadband 0.05–2 kPa, pump_max above zero up to
  `PUMP_DUTY_MAX_PERCENT` — and use them in `MachineSettings::load`, the setters, and both links'
  refusals. Check: `device set flow 100` is refused; a stored out-of-range value falls back to its
  default at boot.

- [x] 8.2 Make the four fields read-only over the radio. Drop the writes on `flow`, `volume`,
  `deadband` and `pump_max` and keep read plus notify; the serial `device set` stays the way to
  tune.
  Check: a BLE write to each changes nothing; `device set` still applies.

- [x] 8.3 Apply every setting on the control task. Today `MachineBleDomain::applyPending` writes the
  settings directly from the interface task while the control task reads them. Post a setting change
  as a command so one task owns the machine state, answering the refusal at write time and applying
  on the next control pass. Check: the build passes, and a setting's read-back reflects the value on
  the next control pass.

## Epic 9 — Refusals and order

- [x] 9.1 Notify a refused write with the field and the reason. Add a `write_result` characteristic
  — the field's suffix plus a reason byte — to `MachineBleDomain`, notified when a target or hold
  write
  is refused: not controlling, out of range, or not a number. The read-back stays the authority.
  Check: write a refused target and observe `write_result` notifying the field and the reason.

## Epic 10 — Telemetry

- [x] 10.1 Put the device's uptime on the radio. Add a `uptime_ms` uint32 characteristic to
  `MachineBleDomain`, read and notified at the fast rate. Check: a BLE read returns the running
  milliseconds and the value advances across notifications.

## Epic 11 — The record

- [x] 11.1 Update `docs/specs/serial-protocol.md` for the ungated `vacuum stop`,
  `vacuum start [<seconds>]`, target `0` meaning ambient, the new refusals, `max_run` and `hold` in
  `vacuum get`, and the `caps` bit. Check: the document's tables and examples match the code.

- [x] 11.2 Update `docs/specs/bluetooth.md` for the new `write_result`, `uptime_ms`, `hold_s` and
  `max_run_s` characteristics, the removed writes on `valve`, `floor`, `flow`, `volume`, `deadband`
  and `pump_max`, the command result change, and the `caps` bit. Check: the characteristic table and
  the write table match the code.

- [x] 11.3 Update `docs/specs/vacuum-control.md` for target `0` meaning ambient, start valid at `0`,
  stop leaving the target, the vent floor, the run deadline, and the `vent_timeout`, `no_progress`
  and `overshoot` faults. Check: the state table and the protections list match the code.

- [x] 11.4 Update `docs/specs/ui/bindings.md` for `vacuum.start()` no longer refusing on a missing
  target, and `vacuum.state()` being able to report the new faults. Check: the binding table and the
  `state` record match the code.
