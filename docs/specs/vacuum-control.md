# Vacuum control

One class, because the state the chamber is in and the state the run is in turned out to be the
same state. `VacuumSystem` is the vacuum system and the machine on top of it: the target, the PID
on the depth the sensor reports, the valve, the pump, the run state and the fault latch. It is
handed the sensor, the pump and the valve, so the control law is policy and the pins stay in
`hal/`.

The controller owns the target, the duration and when to start and stop once it has acquired
control. The device owns what happens when nobody is watching, and nothing else.

## States

```
       vacuum start                          vacuum stop / control release / fault / back hold
IDLE ──────────────> PULL ──(target reached)──> HOLD ───────────────────────> VENT ──(complete)──> IDLE
  ^                                                 │                           │
  │                                                 │                           v
  └───────────────── vacuum reset ──────────────────┴─────────────────────── FAULT
```

`VENT` is the valve open to raise pressure. It has two endings: a release goes to `IDLE` within
2 kPa of ambient (or on the timeout), and a vent to a shallower target seals the chamber and
returns to `PULL`/`HOLD`.

| Event | From | To |
| --- | --- | --- |
| `control acquire` | any | no machine change; the link becomes the one controller |
| `control release` | any | the same as `vacuum stop`; ownership clears and the panel returns to the disconnected page |
| `vacuum start` | anything but `FAULT` | closes the valve, then `PULL`; `err not_ready` with `no_target`, `no_reading` or `latch`. From `VENT` it abandons the release and pulls again |
| `vacuum stop` | anything | opens the valve and stops the pump; from `PULL`, `HOLD` or `VENT` that is `VENT` then `IDLE`, and from `IDLE` or `FAULT` the state does not move |
| Back held for 3 s (local key) | any | the same as `vacuum stop`, with no command and no reply |
| Back+OK held for 5 s (local keys) | any | safe reset: stop, vent up to 2 s, restart; clears ownership |
| `vacuum target <kPa\|->` | anything but `FAULT` | the target changes. A target shallower than the reading while `PULL` or `HOLD` stops the pump, opens the valve and is `VENT` until it reaches the chosen landing, then `PULL`/`HOLD`; a deeper one is `PULL` until it is reached; `-` while pulling or holding releases the chamber |
| target reached | `PULL` | `HOLD`, and the controller's countdown should start here |
| target set deeper | `PULL`, `HOLD` | `PULL`; the pump works to the new depth and `HOLD` follows |
| vent to a shallower target | `PULL`, `HOLD` | `VENT` with `duty=0`; it returns to `PULL`/`HOLD` rather than `IDLE` |
| sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `vacuum reset` | `FAULT` | `VENT` then `IDLE` |
| `device reboot` | any | safe reset: stop, vent up to 2 s, restart |

## The control law

The pump is the only actuator, so the loop can pull down but never vent — the vent valve is the
only way back up. A target set shallower than the chamber while a run is active uses that valve:
the pump stops, the valve opens, and `VENT` holds until the reading reaches the new target, then
the run resumes. A deeper target re-enters `PULL` and the pump works to it; `HOLD` follows when it
arrives.

`HOLD` is the device's own deadband saying that the target has been reached, and that is all it
means. The valve is closed through `PULL` and `HOLD` and open in `IDLE` and `VENT`; whether
"closed" means energised is the fitted part's.

A release finishes when the pressure is within 2 kPa of ambient, or after 30 s, whichever comes
first — a leaky rig takes longer, and that is logged rather than treated as an error. A vent to a
shallower target finishes at its landing and returns to `PULL`/`HOLD`, keeping the target.
`CONTROL_VENT_TO_TARGET` in `core/Config.h` chooses the landing: with it, the valve closes
`VENT_TARGET_LEAD_KPA` above the target so the travel after the coil drops covers the rest; without
it, the valve stays open until the reading passes the target and the pump pulls back.

Gains and the deadband are `PID_KP`, `PID_KI`, `PID_KD` and `CONTROL_DEADBAND_KPA` in
`core/Config.h`. They are starting points, not tuned values — tune them for your chamber volume and
pump.

## Protections

All of these act without consulting the controller:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`, and `PUMP_RAMP_PERCENT_PER_SEC` avoids step
  changes into the pump
* if the sensor stops producing a plausible reading mid-run, the pump is cut and the state latches
  to `FAULT`
* a target deeper than `floor` — the pump's own reach, reported in `vacuum get` — pins the duty at
  the cap, and nothing stops it
* holding Back for 3 s stops the pump and vents
* holding Back and OK for 5 s releases the chamber, restarts, and clears a held ownership record

Faults latch: only `vacuum reset` clears one, and the device never retries on its own.

## What a controller has to know

* **The hold's duration is yours.** The device has no timer, no timeout and no maximum run time,
  so a run continues until you send `vacuum stop`. `HOLD` is the cue, not a countdown: it means
  the device's own deadband has decided the target is reached, which is the one thing you cannot
  work out for yourself, because the deadband is the device's loop.
* **A Back hold is a local `vacuum stop`**, and **Back+OK for 5 s is the local reset.** Both run
  whatever a controller is doing. A link that dropped while owning cannot be recovered any other
  way.
* **A link that drops while owning does not stop the run.** Ownership stays held, the machine keeps
  its state and the panel keeps showing that link. The device only detects a controller that
  leaves; one that hangs with the port open holds the run open.
* **With no timer in the device, an unattended run holds until something stops it.** A normally
  open solenoid draws current for the whole hold, which is why a long hold, not the pump, is the
  dominant battery cost.
* **The vent valve is normally open by default**, so de-energised is open and running a vacuum
  means energising the coil. `NormallyClosedValve` in `app/Build.cpp` inverts that and removes the
  fail-safe.
* **The panel is the device's.** It shows the controlling link's icon, the run state and the
  battery; there is no drawing command, so everything else — a vent timeout, the duty, the fault
  code — is only on the wire. Poll `vacuum get` to see it.
