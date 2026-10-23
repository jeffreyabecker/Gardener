# Safety hardening

The machine's safe path must not depend on the controller being correct, well behaved, or alive.
Each section states one problem the controller must not decide for the machine, and the change that
closes it. The proposed solution says what has to be true when the work lands, not which file or
class gets it there.

## Preserved

These are not changed and are not up for rework:

- the PID and its two tuning inputs, `flow` and `volume`, and the gains derived from them;
- `target` as the only actuator — there is still no raw-duty command;
- the ownership stack: one link drives, a claim always succeeds, any change of the top vents — which
  is also what makes a command from an evicted link harmless: it is applied, then vented within the
  same 50 ms beat;
- the fault latch: only `reset` clears it, nothing retries on its own;
- the normally-open valve as the default, and the boot path that leaves the pump off and the valve
  open;
- the wire's identity: `ble_proto` and the serial `proto` stay `1`. Everything below is additive — a
  new characteristic, a `caps` bit, a fault code, a refusal reason — or a behaviour a client of the
  current table already tolerates.

## The problems and what to do about them

### 1. A stop can be refused, dropped or undone

**Problem.** The one command that makes the machine safe is the one command allowed to fail: a
non-owner's stop is refused before the opcode is even read, a stop behind a full command queue is
dropped, a stop sent to a machine that is already venting restarts its vent timer, and a start that
was queued before the stop can run after it.

**Proposed solution.**

- Accept `stop` from any link on either transport, before the ownership gate, and answer it exactly
  as an owner's stop is answered today.
- Keep the queue as the only way into the machine, and make a command the queue cannot take be
  requeued by its link on the next poll rather than dropped; a stop takes precedence over every
  command queued before it, and whatever it discards is logged.
- Make stop idempotent: stopping a machine that is already venting does not restart its vent timer.
- Stop and vent from the panel's Cancel hold whatever link drives, without the panel having to own
  the machine. OK+Cancel stays the harder reset.

### 2. The control surface is tangled

**Problem.** `setTarget`, `clearTarget`, `start` and `stop` each work out for themselves whether to
open the valve, close it or run the pump, so the same vent/start/stop decision is re-derived in
four places: setting a target can vent, clearing a target stops, a start while venting abandons
the vent. The command queue is not the problem — it is drained to empty every 50 ms beat — the
problem is that the decision has no single owner.

**Proposed solution.** Three orthogonal commands whose meaning does not depend on the state the
machine is in.

- A **target** is a setpoint, nothing else. `0` means ambient — the chamber released — not "no
  target": writing `0` vents to ambient and stops. A target deeper than the reading pulls down to
  it; a target shallower than the reading vents down to it, and a non-zero one then holds it. A
  non-zero target written while stopped is stored, not acted on.
- **`start`** begins maintaining the stored target, and is valid at any target, `0` included. It is
  refused only when there is no valid reading or a fault is latched; a start while already running
  is a no-op.
- **`stop`** vents to ambient and stops, and leaves the target where it is; a stop while already
  venting or stopped is a no-op. A stop does not clear the target, so stopping and then starting
  again resumes the same setpoint.
- **A stale start cannot undo a stop.** A start that was already queued when a stop arrives is
  discarded by problem 1, and a start from a link that has lost the top is vented within the same
  beat by the top-change vent; a start sent after the stop is the controller's decision and is
  honoured.
- The three are applied behind **one surface** that the wire, the panel and the routines all call —
  "given the command, the state and the reading against the target, does the valve open, close, or
  the pump run" — so the vent/start/stop decision is made once instead of re-derived by each caller.

### 3. The depth is unbounded on the device

**Problem.** `setTarget` accepts any depth, including far past `floor`, so a buggy or foreign
controller can command a depth the pump cannot reach and pin the duty at the cap forever. The
ceiling exists only in the app.

**Proposed solution.**

- Refuse a target deeper than the fitted `floor` on every path that sets one, and name the bound in
  the refusal; a refused value leaves the target unchanged.
- Clamp a stored `floor` to 5–100 kPa when it is loaded, and refuse a write outside that range.
- Make `floor` a commissioning value: written only on the local link, read-only over the radio. The
  accepted trade is that an operator with the cable can raise the bound — that is what commissioning
  means, not a hole to close.

### 4. A hold has no deadline

**Problem.** Nothing on the device times a run, so a phone that keeps the link open but stops
talking holds the pump indefinitely.

**Proposed solution.**

- End the run by itself fifteen minutes after it starts, as a release, with a log line saying the
  deadline ended it.
- Let a start carry a shorter hold in whole seconds, end the run at it, and refuse a hold above the
  maximum, naming the maximum.
- Publish the maximum and the hold granted to the current run on both links.
- Treat expiry as a normal release — `VENT` then `IDLE` — not as a fault.

### 5. The release criterion is ambient, which the rig never reaches

**Problem.** A release is judged complete when the depth falls within `VENT_MARGIN_KPA` of ambient,
but a chamber with the valve open does not settle at 0 — the tube, the valve orifice and the
sensor's offset leave it sitting a few kPa above ambient, so "vented" never means "at 0". Today the
vent timeout papers over that mismatch and lets an unfinished release fall through to `IDLE` with
no evidence; latching on the strict margin instead would fault every ordinary release.

**Proposed solution.**

- Give the machine a **vent floor**: the pressure this rig settles at with the valve open, a fitted
  fact measured at boot or commissioned, not ambient. A release is complete when the depth reaches
  the vent floor within the vent margin, and only then reports `IDLE`.
- Latch `vent_timeout` when the depth has not reached the vent floor inside the vent timeout — the
  fault means the vent did not get where this rig vents, not that the chamber is short of 0.
- Latch `sensor` when the reading is lost during a release, as it already does during a pull.
- Report `VENT`, not `IDLE`, when the machine comes up above the vent floor.
- Report both through the run log as fault transitions, replacing the vent-timeout warning.

### 6. Two loop failure modes have no bound

**Problem.** A pull that is driven hard but makes no headway, and a depth that runs past its target,
are both left to the controller to notice.

**Proposed solution.**

- Latch `no_progress` for a pull driven at or above half the duty cap for ten seconds without a
  kilopascal of progress, and release the chamber.
- Latch `overshoot` for a depth more than five kilopascals past the target on five consecutive
  readings, and release the chamber.
- Both clear only by reset, like every other fault.

### 7. The fail-safe valve is unreported and remotely writable

**Problem.** `valve` is writable over the radio, so a bonded phone can silently replace the fitted
direction for the next boot, and `caps` says nothing about which way the fitted valve fails. The
normally-open valve is the whole of the fail-safe: de-energised means vented, so the chamber cannot
pressurise unless the coil is actively held closed — a crash, a cut wire or a dead coil leaves it
unable to build pressure at all. A normally-closed part inverts that, sealing the chamber when
power is lost.

**Proposed solution.**

- Set a fail-safe-valve bit in `caps` from the fitted valve at boot, and carry it in
  `device get caps=`, so a client can refuse a build whose resting state is sealed.
- Make `valve` read-only over the radio, keeping `device set valve` on the local link.

### 8. Tuning is loosely bounded and remotely writable

**Problem.** `flow`, `volume`, `deadband` and `pump_max` accept implausible values, are writable
from a bonded phone, and are applied off the control task.

**Proposed solution.**

- Bound all four from one definition used by the setters, the loader and both links' refusals.
- Make them read and notify only over the radio; tune on the local link.
- Apply every machine setting on the control task, posted like a command, with a refusal still
  answered at write time.

### 9. Refused writes are silent

**Problem.** A refused target or hold notifies nothing, so a client cannot tell a refusal from a
lost write and may mistake silence for success.

**Proposed solution.**

- Notify a refusal with the field and the reason — not driving, out of range, or not a number. The
  read-back stays the authority; the notification is an addition to it.
- Keep a wrong-length payload ignored rather than padded or truncated.

### 10. Telemetry has no ordering on the radio

**Problem.** Notifications carry no sequence and the radio has no uptime, so a client cannot place
`HOLD`, `VENT` and command results in time, or tell a reconnect apart from a restart.

**Proposed solution.**

- Add the device's uptime to the Bluetooth table, notified at the fast rate. The serial readout
  already reports `up`.

## Consequences for the wire

### The serial line

| Where | What changes |
| --- | --- |
| `vacuum stop` | accepted without control — the only state-changing verb that is; vents to ambient and leaves the target unchanged |
| `vacuum start [<seconds>]` | an optional hold; `err out_of_range field=hold max=<s>` above the maximum; refused with `err not_ready no_reading` or `latch`; a start while running is a no-op |
| `vacuum target <kPa>` | `0` vents to ambient and stops; deeper than the reading pulls to it, shallower vents down to it; deeper than `floor` is `err out_of_range field=target max=<floor>` |
| `vacuum get` | gains `max_run=<s>` and `hold=<s>`, the hold granted to the current run and 0 when idle |
| `device get` | `caps=` carries the fail-safe valve bit |
| `device set` | still the way to tune; the bounded fields refuse a value outside their range |

### Bluetooth, at `ble_proto` 1

| Characteristic | Change |
| --- | --- |
| `caps` | bit `0x04`: a fail-safe, normally-open valve is fitted |
| `valve` | read only; the write is gone |
| `floor`, `flow`, `volume`, `deadband`, `pump_max` | read and notify only; the writes are gone |
| `target` | `0` vents to ambient and stops; deeper than the reading pulls to it, shallower vents down to it |
| `write_result` | new: the field and the reason a write was refused, notified |
| `uptime_ms` | new: the device's uptime, notified at the fast rate |
| `hold_s` | new: the hold to ask for, and the hold granted |
| `max_run_s` | new: the compiled maximum run time |
| `command` | `start` is valid at a target of `0`, so result `0x01` (no target) is no longer produced; the rest of the result set is unchanged |

New fault codes: `vent_timeout`, `no_progress`, `overshoot`.

## Deferred

The parking lot holds what this plan deliberately leaves out: thermal and continuous-duty derating
with a body-safe `pump_max` default, a low-battery start refusal with a gauge on machine-alone
builds, a local event log, and pairing — authorization stronger than Just Works, and the bond cap
and forget-bonds action that bound it.

## How it is known to hold

Each solution names an outcome that can be observed on either link or in the log, so a build can be
checked against it without reading the code:

- a stop from a non-owner on both transports, and from the panel while a phone drives;
- writing a target of `0` vents to ambient and stops; sending `stop` vents to ambient and leaves the
  target unchanged;
- a start at a target of `0` applied, and a start while running a no-op;
- a shallower target venting down to it and then holding, and a deeper one pulling to it;
- a target past `floor` refused and the read-back unchanged;
- a run that ends by itself at the maximum, and at a shorter hold the client asked for;
- a release that never reaches the vent floor latching `vent_timeout` rather than reading `IDLE`;
- a blocked hose latching `no_progress`, and a target stepped past shallow latching `overshoot`;
- `caps` reporting the fitted valve, and a radio write to `valve` or a tuning field changing
  nothing;
- a refused target notifying `write_result`.

The behavioural pass needs a device, so it belongs with the hardware verification the parking lot
already tracks, and grows a checklist for these guarantees.
