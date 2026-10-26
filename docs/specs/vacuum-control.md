# Vacuum control

One class, because the state the chamber is in and the state the run is in turned out to be the
same state. `VacuumSystem` is the vacuum system and the machine on top of it: the target, the PID
on the depth the sensor reports, the valve, the pump, the run state and the fault latch. It is
handed the sensor, the pump and the valve, so the control law is policy and the pins stay in
`hal/`.

Whoever is commanding owns the target, the hold and when to start and stop. The device owns what
happens when nobody is watching — the deadline on a run and every fault — and nothing else. Any
link may command at any time, and the last command to reach the control task wins.

Every pressure the machine holds or reports — in the loop, on the serial line and over Bluetooth —
is a depth below ambient in kPa: zero at rest, larger as the chamber is pumped down, smaller as the
valve lets it back toward ambient. The sensor's negative gauge output becomes that depth in its
driver under `hal/`, held at zero rather than going negative, so nothing above the part deals in a
signed gauge pressure. A target of `0` is the ambient target: the chamber released, which is what a
stop asks for too. Any non-zero target is held to `floor`, the deepest depth this rig can reach.

## States

```
       vacuum start                          vacuum stop / fault / back hold
IDLE ──────────────> PULL ──(target reached)──> HOLD ───────────────────────> VENT ──(complete)──> IDLE
  ^                                                 │                           │
  │                                                 │                           v
  └───────────────── vacuum reset ──────────────────┴─────────────────────── FAULT
```

`VENT` is the valve open to let the chamber back toward ambient. It has two endings: a release goes
to `IDLE` once the depth reaches this rig's vent floor within the vent margin, and a vent to a
shallower target seals the chamber and returns to `PULL`/`HOLD`. A release that has not reached the
floor after 30 s latches `vent_timeout` instead of reporting `IDLE`.

| Event | From | To |
| --- | --- | --- |
| `vacuum start [<s>]` | anything but `FAULT` | closes the valve, then runs the stored target, `0` included, for the hold or the maximum; `err not_ready` with `no_reading` or `latch`. From `VENT` it abandons the release. A start while running is a no-op |
| `vacuum stop` | anything | opens the valve and stops the pump; from `PULL`, `HOLD` or `VENT` that is `VENT` then `IDLE`, and from `IDLE` or `FAULT` the state does not move. The target is kept |
| Cancel held for 5 s (local key) | any | `vacuum stop` with no command and no reply, and the panel returns to the home scene, whatever else is connected |
| OK+Cancel held for 5 s (local keys) | any | safe reset: stop, vent up to 2 s, restart |
| `vacuum target <kPa\|->` | anything but `FAULT` | the target changes. `0` (or `-`) is the ambient target and releases the chamber; a target shallower than the reading while `PULL` or `HOLD` stops the pump, opens the valve and is `VENT` until it reaches the chosen landing, then `PULL`/`HOLD`; a deeper one is `PULL` until it is reached. Deeper than `floor` is refused and changes nothing |
| target reached | `PULL` | `HOLD`, and the run's hold is timed from here by the controller |
| target set deeper | `PULL`, `HOLD` | `PULL`; the pump works to the new depth and `HOLD` follows |
| vent to a shallower target | `PULL`, `HOLD` | `VENT` with `duty=0`; it returns to `PULL`/`HOLD` rather than `IDLE` |
| the run's hold expires | `PULL`, `HOLD` | `VENT` then `IDLE`: the chamber is released and the log names the deadline |
| sensor loss | any state but `IDLE` | `FAULT` with `sensor`, the pump cut and the valve open |
| `vacuum reset` | `FAULT` | `VENT` then `IDLE` |
| `device reboot` | any | safe reset: stop, vent up to 2 s, restart |

Every link can command the machine at any time; there is no ownership stack and no hand-over. A
command is applied on the control task's next beat, so when two links disagree the last one to
arrive is the one in force. Neither a serial detach nor a Bluetooth disconnect stops a run: it
continues to its hold, and the deadline is the bound. A build with a panel and a build without one
behave the same way.

## The control law

The pump is the only actuator, so the loop can pull down but never vent — the vent valve is the
only way back toward ambient. A target set shallower than the chamber while a run is active uses
that valve: the pump stops, the valve opens, and `VENT` holds until the reading reaches the new
target, then the run resumes. A deeper target re-enters `PULL` and the pump works to it; `HOLD`
follows when it arrives.

`HOLD` is the device's own deadband saying that the target has been reached, and that is all it
means. The valve is closed through `PULL` and `HOLD` and open in `IDLE` and `VENT`; whether
"closed" means energised is the fitted part's.

The deadband bounds an arrival by pulling. A vent to a shallower target is the exception: it lands
a lead deeper than the target and the loop calls that arrived, so `HOLD` can follow a vent with the
reading up to `VENT_TARGET_LEAD_KPA` deeper than the target rather than within `deadband`. The pump
stays off until the leak brings it back inside the band, which is the rest of the travel.

A release finishes when the depth reaches this rig's vent floor within 2 kPa — `VENT_FLOOR_KPA`,
measured on the bench — or latches `vent_timeout` after 30 s without getting there. The floor rather
than ambient is the criterion because a chamber with the valve open does not settle at 0: the tube,
the orifice and the sensor's offset leave it a little above. A vent to a shallower target finishes at
its landing and returns to `PULL`/`HOLD`, keeping the target.
`CONTROL_VENT_TO_TARGET` in `Machine.h` chooses the landing: with it, the valve closes
`VENT_TARGET_LEAD_KPA` deeper than the target so the travel after the coil drops covers the rest;
without it, the valve stays open until the reading passes the target and the pump pulls back.

A run has a deadline. `vacuum start` may carry a hold in whole seconds, and a start without one gets
`CONTROL_MAX_RUN_MS` (fifteen minutes); the hold is published as `hold` and the maximum as
`max_run`. When it expires the machine releases the chamber and the run log names it. That is a
release, not a fault: the machine did what it was told for as long as it was allowed to.

The gains are derived from the pump's projected free-flow rate and the chamber volume —
`PUMP_FLOW_LPM` (L/min) and `CHAMBER_VOLUME_L` (L), two build symbols in `Machine.h`. A different
pump or chamber is retuned by building with the two numbers rather than by re-deriving the gains. The
loop's other rig numbers are build symbols the same way: `CONTROL_FLOOR_KPA` (the pump's measured
reach), `PUMP_DUTY_MAX_PERCENT` (the duty cap), `CONTROL_DEADBAND_KPA` (the target-reached
tolerance) and `VALVE_NORMALLY_OPEN` (the fitted valve's direction). A build sets them with `-D` —
`scripts/arduino.py --define NAME=VALUE` — and a value that could not work, a flow or volume of
zero or a duty cap outside 0–100, is a compile error rather than a strange boot. None of them is
read from `/cfg/settings.ini` or written over a link: they describe the rig rather than a preference a
device holds.

## Protections

All of these act without consulting the controller:

* duty is hard capped at `PUMP_DUTY_MAX_PERCENT`, with `PUMP_RAMP_PERCENT_PER_SEC` avoiding step
  changes into the pump
* a target deeper than `CONTROL_FLOOR_KPA` is refused on every path that sets one — the serial
  line, Bluetooth and a routine alike — and the refusal names the bound
* if the sensor stops producing a plausible reading while the machine is not idle, the pump is cut
  and the state latches `sensor` — a pull, a hold, a target vent and a release all steer on it
* a release that has not reached the vent floor after 30 s latches `vent_timeout`
* a pull driven at or above half the duty cap for ten seconds without a kilopascal of headway
  latches `no_progress`
* a depth more than 5 kPa past the target on five consecutive readings latches `overshoot`
* a run ends as a release at its hold, fifteen minutes at most
* holding Cancel for 5 s stops the pump, vents and returns the panel to the home scene, whatever
  else is connected
* holding OK and Cancel for 5 s reboots the device: stop, vent up to 2 s, restart

A latched fault cuts the pump, opens the valve and holds `FAULT` until it is cleared. Only
`vacuum reset` clears one, and the device never retries on its own.

## What a controller has to know

* **The hold's duration is yours, within a bound.** `HOLD` is the cue, not a countdown: it means
  the device's own deadband has decided the target is reached, which is the one thing you cannot
  work out for yourself, because the deadband is the device's loop. A start may carry a shorter
  hold, and the run releases itself at the maximum whatever you do — the device will not hold the
  pump for a controller that has stopped talking.
* **Holding Cancel for 5 s is the local force-quit** — `vacuum stop` and back to the home scene,
  whatever else is connected — and **OK+Cancel for 5 s is the local reset.** The reset runs whatever
  else is happening.
* **A run ends at its hold whatever the links do.** A link that stops answering does not keep the
  pump running; the deadline releases the chamber.
* **A stop is always available.** Any link may send it, so a bystander on the same wire or radio can
  vent the chamber.
* **A long hold is the dominant battery cost.** A normally open solenoid draws current for the whole
  hold, which is why the run's deadline matters as much as the pump's.
* **The vent valve is normally open by default**, so de-energised is open and running a vacuum
  means energising the coil. `VALVE_NORMALLY_OPEN=0` on the build line picks the normally closed
  part in `Machine.cpp` instead, and removes the fail-safe along with the `caps` bit.
* **The panel is the device's.** It shows connection presence, the run state and the battery; there
  is no drawing command, so everything else — a vent timeout, the duty, the fault code — is only on
  the wire. Poll `vacuum get` to see it.
