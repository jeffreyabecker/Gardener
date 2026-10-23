# Safety hardening

## The requirement

The machine's safe path must not depend on the controller being correct, well behaved, or alive.

Today several of the device's safe behaviours are the controller's to keep: a stop is refused unless
the sender drives the machine, a command can be dropped by a full queue, a queued `start` can undo a
stop, a target may be any depth, a run may last forever, a release that never reaches ambient is
reported as `IDLE`, and the fail-safe valve can be turned off from a phone. Each of those is a place
where a buggy or foreign controller decides whether the machine is safe.

This plan requires the device to own those decisions instead, and says what each one has to be true
of. It says nothing about how: the smallest mechanism the current single loop and one-machine shape
allow is the implementer's call.

## What does not change

- the control law: the PID, its two tuning inputs and the gains derived from them;
- `target` as the only actuator — there is still no raw-duty command;
- the ownership stack: one link drives, a claim always succeeds, and any change of the top vents;
- the fault latch: only `reset` clears it, and nothing retries on its own;
- the normally-open valve as the default, and the boot path that leaves the pump off and the valve
  open;
- the wire's identity: `ble_proto` stays `1` and the serial `proto` stays `1`. Everything below is
  additive — a new characteristic, a `caps` bit, a fault code, a refusal reason — or a behaviour a
  client of the current table already tolerates.

## The requirements

### 1. A stop is never refused, lost or undone

- **Every link may stop.** A `stop` is accepted from any link on either transport, whether or not it
  drives the machine, and the reply is the one an owner gets today — `ok state=VENT` on the serial
  line, result `0x00` over Bluetooth.
- **A stop cannot be lost.** It does not depend on room in the command queue.
- **A stop ends the queue.** A command queued before a stop is never applied after it. What a stop
  discards is logged.
- **A stop is idempotent.** Stopping a machine that is already releasing neither restarts nor
  extends the release.
- **The panel can always stop.** The panel's Cancel hold stops and vents whatever link drives; the
  panel does not have to be the controller to make the machine safe. The OK+Cancel reset is
  unchanged.

### 2. A command from a link that has been replaced does not run

- **Ownership is checked where a command is applied, not only where it is posted.** A start, target,
  reset or setting posted by a link that has since lost the top is discarded and logged. A stop is
  exempt, by §1.
- **A start is refused while the machine is venting**, whether the vent is a release to ambient or a
  vent to a shallower target.
- **A start is refused for two seconds after a stop.** A retry, a queued duplicate or a reconnecting
  client cannot re-pressurise the chamber in the moment after someone stopped it.
- **The refusal is named.** `venting` is a refusal reason on both links rather than a bare failure:
  `err not_ready venting` on the serial line and a result code in the machine's own range over
  Bluetooth.

### 3. The depth is bounded by the device

- **The fitted floor is the maximum.** A target deeper than the reported `floor` is refused on every
  path that sets one, and the refusal names the bound. Nothing accepts a target the pump cannot
  reach, so a past-reach target can no longer pin the duty indefinitely.
- **There is no second maximum.** The bound a client discovers is the `floor` it already reads — in
  `vacuum get` and in the `floor` characteristic.
- **`floor` is a commissioning value.** It is written only on the local link and never from the
  radio, and a stored value outside 5–100 kPa falls back to the compiled default. How far the pump
  reaches is decided at the bench by whoever assembled the rig.
- **The accepted trade:** an operator with the cable can raise the bound. That is what a
  commissioning value means, not a hole to close.

### 4. A hold has a deadline

- **A run ends by itself** at the compiled maximum of fifteen minutes, measured from the start: pump
  off, valve open, a release, and a log line saying the deadline ended it.
- **A controller may ask for less.** A start may carry a hold in whole seconds, and the run ends at
  the shorter of that and the maximum. A request longer than the maximum is refused, and the
  refusal names the maximum.
- **Both links can express it**, and both report the maximum and the hold granted to the current
  run.
- **The deadline is the run's, not the pressure's.** Time at the target counts, and a vent to a
  shallower target inside a run does not extend it.
- **Expiry is not a fault.** The client sees `VENT` then `IDLE` and may start again; nothing
  latches.

### 5. A release either completes or latches

- **A release that never reaches ambient is a fault.** If the depth has not come within the vent
  margin inside the vent timeout, `vent_timeout` latches and the state stays `FAULT` — pump cut,
  valve open — until reset.
- **A reading lost during a release is a fault.** `sensor` latches exactly as it does during a pull.
- **`IDLE` is never reported above the vent margin.** A machine that comes up with the chamber still
  under vacuum reports `VENT` and is held to the same timeout, so a boot into a leaky chamber cannot
  read as safe.
- **The log agrees with the state.** The vent-timeout warning in the run log becomes the fault
  transition.

### 6. The loop fails on its own evidence

- **No progress.** A pull driven at or above half the duty cap for ten seconds without the depth
  improving by a kilopascal latches `no_progress` and releases. It catches a blocked hose, a dead
  pump, a stuck sensor and a target past reach.
- **Overshoot.** A depth more than five kilopascals past the target on five consecutive readings
  while pulling or holding latches `overshoot` and releases. A sensor spike, a failed valve or a
  retuned loop can break the loop's trust in itself.
- **Both latch.** Only `reset` clears them, like every other fault.

### 7. The fail-safe is reported and not remotely removable

- **`caps` says whether the fitted valve fails safe.** A bit is set at boot from the valve actually
  built and carried both by the characteristic and by `device get caps=`, so a client can refuse a
  build whose de-energised state does not vent.
- **The valve direction is a fact, not a knob.** It stays readable over the radio and is written
  only on the local link.

### 8. Tuning is local and bounded

- **One set of bounds.** `flow`, `volume`, `deadband`, `floor` and `pump_max` are bounded by one
  definition, used when a value is written, when it is loaded, and when either link refuses it.
  Proposed: flow 0.05–10 L/min, volume 0.005–5 L, deadband 0.05–2 kPa, floor 5–100 kPa, and
  `pump_max` above zero up to the part's compiled cap.
- **A phone cannot retune the loop.** Those five and `valve` are read and notified over the radio
  and written only on the local link.
- **One task owns the machine's state.** A setting change is applied the way a command is, so the
  "applied on the next pass" contract holds for every field rather than for some of them.
- **`pump_max` is a duty cap, not a thermal rating.** Its ceiling stays the part's compiled limit; a
  body-safe default tied to the pump's continuous duty is a separate question, deferred below.

### 9. A refusal is answered

- **A refused write says so.** A target or hold the device refuses notifies the field and the
  reason — not driving, out of range, or not a number — so a client cannot mistake silence for
  success. The read-back stays the authority; the result is an addition to it.
- **A wrong-length payload is still ignored** rather than padded or truncated.

### 10. Telemetry can be ordered

- **The radio carries the device's uptime**, so a client can order `HOLD`, `VENT` and command
  results against its own writes and tell a reconnect apart from a restart. The serial readout
  already reports `up`.

### 11. Pairing is bounded, and its limit is stated

- **At most four bonds**, and a local action that forgets every bond. Nothing over the radio can
  forget one, which is why the local action exists.
- **Stated plainly in the contract:** Just Works is proximity, not authorization. Anyone within
  radio range of an advertising machine can pair with it and command pressure. The bond cap bounds
  the standing exposure; the physical stop in §12 is what covers the rest.

### 12. The hardware the firmware assumes

Firmware cannot be the last bound, and this plan does not pretend otherwise. A build used on a body
needs — and its documentation has to say it needs:

- a physical stop that cuts pump power and opens the vent valve without the MCU;
- a mechanical relief or bypass set to the safe depth;
- a normally-open vent valve, reported in `caps`;
- optionally an independent pressure switch in series with pump power.

## The wire

### The serial line

| Where | What changes |
| --- | --- |
| `vacuum stop` | accepted without control — the only state-changing verb that is |
| `vacuum start [<seconds>]` | an optional hold; `err out_of_range field=hold max=<s>` above the maximum; `venting` joins the `err not_ready` reasons |
| `vacuum target` | deeper than `floor` is `err out_of_range field=target max=<floor>` instead of accepted |
| `vacuum get` | gains `max_run=<s>` and `hold=<s>`, the hold granted to the current run and 0 when idle |
| `device get` | `caps=` carries the fail-safe valve bit |
| `device set` | still the way to tune; the bounded fields refuse a value outside their range |
| bonds | a local verb forgets every bond, because the radio cannot |

### Bluetooth, at `ble_proto` 1

| Characteristic | Change |
| --- | --- |
| `caps` | bit `0x04`: a fail-safe, normally-open valve is fitted |
| `valve` | read only; the write is gone |
| `floor`, `flow`, `volume`, `deadband`, `pump_max` | read and notify only; the writes are gone |
| `write_result` | new: the field and the reason a write was refused, notified |
| `uptime_ms` | new: the device's uptime, notified at the fast rate |
| `hold_s` | new: the hold to ask for, and the hold granted |
| `max_run_s` | new: the compiled maximum run time |
| `command` | result `0x04`: the machine is venting, or inside the restart lockout |

New fault codes: `vent_timeout`, `no_progress`, `overshoot`.

## Deferred

The parking lot holds what this plan deliberately leaves out: thermal and continuous-duty derating
with a body-safe `pump_max` default, a low-battery start refusal with a gauge on machine-alone
builds, a local event log, and pairing authorization stronger than Just Works.

## How it is known to hold

Each requirement names an outcome that can be observed on either link or in the log, so a build can
be checked against it without reading the code:

- a stop from a non-owner on both transports, and from the panel while a phone drives;
- a start refused while venting and inside the lockout, with the reason on both links;
- a target past `floor` refused and the read-back unchanged;
- a run that ends by itself at the maximum, and at a shorter hold the client asked for;
- a release held short of ambient latching `vent_timeout` rather than reading `IDLE`;
- a blocked hose latching `no_progress`, and a target stepped past shallow latching `overshoot`;
- `caps` reporting the fitted valve, and a radio write to `valve` or a tuning field changing
  nothing;
- a refused target notifying `write_result`;
- a fifth bond refused, and the local action clearing every bond.

The behavioural pass needs a device, so it belongs with the hardware verification the parking lot
already tracks, and grows a checklist for these guarantees.
