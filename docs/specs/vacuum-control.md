# Vacuum control

One class, because the state the chamber is in and the state the run is in turned out to be the
same state. `VacuumSystem` is the vacuum system and the machine on top of it: the target, the PID
on the depth the sensor reports, the valve, the pump, the run state and the fault latch. It is
handed the sensor, the pump and the valve, so the control law is policy and the pins stay in
`hal/`.

The controller owns the target, the duration and when to start and stop once it has acquired
control. The device owns what happens when nobody is watching, and nothing else.

Every pressure the machine holds or reports — in the loop, on the serial line and over Bluetooth —
is a depth below ambient in kPa: zero at rest, larger as the chamber is pumped down, smaller as the
valve lets it back toward ambient. The sensor's negative gauge output becomes that depth in its
driver under `hal/`, held at zero rather than going negative, so nothing above the part deals in a
signed gauge pressure. A target of `0` means none rather than ambient: the loop accepts nothing
shallower than `CONTROL_MIN_TARGET_KPA`.

## States

```
       vacuum start                          vacuum stop / control release / fault / back hold
IDLE ──────────────> PULL ──(target reached)──> HOLD ───────────────────────> VENT ──(complete)──> IDLE
  ^                                                 │                           │
  │                                                 │                           v
  └───────────────── vacuum reset ──────────────────┴─────────────────────── FAULT
```

`VENT` is the valve open to let the chamber back toward ambient. It has two endings: a release goes
to `IDLE` within 2 kPa of ambient (or on the timeout), and a vent to a shallower target seals the
chamber and returns to `PULL`/`HOLD`.

| Event | From | To |
| --- | --- | --- |
| `control acquire` | any | the claimant takes the top of the stack and the chamber vents; the new controller starts from an open chamber |
| `control release` | any | this link leaves the stack and the chamber vents; the link beneath is the controller again, or `none` |
| `vacuum start` | anything but `FAULT` | closes the valve, then `PULL`; `err not_ready` with `no_target`, `no_reading` or `latch`. From `VENT` it abandons the release and pulls again |
| `vacuum stop` | anything | opens the valve and stops the pump; from `PULL`, `HOLD` or `VENT` that is `VENT` then `IDLE`, and from `IDLE` or `FAULT` the state does not move |
| Cancel held for 5 s (local key) | any | `vacuum stop` with no command and no reply, and the panel returns to the readout; acts only while the panel drives |
| OK+Cancel held for 5 s (local keys) | any | safe reset: stop, vent up to 2 s, restart |
| `vacuum target <kPa\|->` | anything but `FAULT` | the target changes. A target shallower than the reading while `PULL` or `HOLD` stops the pump, opens the valve and is `VENT` until it reaches the chosen landing, then `PULL`/`HOLD`; a deeper one is `PULL` until it is reached; `-` while pulling or holding releases the chamber |
| target reached | `PULL` | `HOLD`, and the controller's countdown should start here |
| target set deeper | `PULL`, `HOLD` | `PULL`; the pump works to the new depth and `HOLD` follows |
| vent to a shallower target | `PULL`, `HOLD` | `VENT` with `duty=0`; it returns to `PULL`/`HOLD` rather than `IDLE` |
| sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `vacuum reset` | `FAULT` | `VENT` then `IDLE` |
| `device reboot` | any | safe reset: stop, vent up to 2 s, restart |

The controller is whoever is on top of the ownership stack, and **any change of the top vents the
chamber** — a claim, a release, or a link that drops. A new controller therefore always starts from
an open chamber. Only the link on top can leave the stack, so a link that loses its transport
beneath another one stays there and leaves when it reaches the top, venting then exactly as if it
had dropped while driving: a vanished link can never leave the machine held.

A build with no panel has nothing beneath an external link: its stack is `none` alone until
something claims, so **it holds pressure only while a link owns it**. An unowned machine is vented,
and `vacuum start` from a link that is not on top is refused. A panel build differs in one respect
only — the panel is a controller of its own, pushed at boot, so the device can hold pressure with no
external link attached.

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

A release finishes when the pressure is within 2 kPa of ambient, or after 30 s, whichever comes
first — a leaky rig takes longer, and that is logged rather than treated as an error. A vent to a
shallower target finishes at its landing and returns to `PULL`/`HOLD`, keeping the target.
`CONTROL_VENT_TO_TARGET` in `Machine.h` chooses the landing: with it, the valve closes
`VENT_TARGET_LEAD_KPA` deeper than the target so the travel after the coil drops covers the rest;
without it, the valve stays open until the reading passes the target and the pump pulls back.

The gains are derived from the pump's projected free-flow rate and the chamber volume — two device
settings, `flow` (L/min) and `volume` (L), with compile-time defaults in `Machine.h`, restored from
`/settings.cfg`, and overridable over the wire (`device set flow`, `device set volume`). A different
pump or chamber is retuned by setting the two numbers, not by re-deriving the gains. The loop's
other rig numbers are settings the same way: `floor` (the pump's measured reach), `pump_max` (the
duty cap) and `deadband` (the target-reached tolerance), each with a `Machine.h` default and a
`device set` verb. `valve` (`open` or `closed`) names the fitted valve's direction and is read once
at boot, so it is the one setting a change to does not take up until the next boot.

## Protections

All of these act without consulting the controller:

* duty is hard capped at the `pump_max` setting, with the pump driver's own
  `PUMP_DUTY_MAX_PERCENT` as the part's ceiling, and `PUMP_RAMP_PERCENT_PER_SEC` avoids step
  changes into the pump
* if the sensor stops producing a plausible reading mid-run, the pump is cut and the state latches
  to `FAULT`
* a target deeper than `floor` — the pump's own reach, reported in `vacuum get` — pins the duty at
  the cap, and nothing stops it
* holding Cancel for 5 s stops the pump, vents and returns the panel to the readout
* holding OK and Cancel for 5 s reboots the device: stop, vent up to 2 s, restart

Faults latch: only `vacuum reset` clears one, and the device never retries on its own.

## What a controller has to know

* **The hold's duration is yours.** The device has no timer, no timeout and no maximum run time,
  so a run continues until you send `vacuum stop`. `HOLD` is the cue, not a countdown: it means
  the device's own deadband has decided the target is reached, which is the one thing you cannot
  work out for yourself, because the deadband is the device's loop.
* **Holding Cancel for 5 s is the local force-quit** — `vacuum stop` and back to the readout, only
  while the panel drives — and **OK+Cancel for 5 s is the local reset.** The reset runs whatever a
  controller is doing, so a link that dropped while owning cannot be recovered any other way.
* **A link that drops while owning stops the run.** It leaves the stack, so the chamber vents and
  control passes to the link beneath — the panel or `none`. The device cannot tell a controller that
  left from one that hung, though: a link that stops answering while its port stays open keeps the
  top until OK+Cancel.
* **With no timer in the device, an unattended run holds until something stops it.** A normally
  open solenoid draws current for the whole hold, which is why a long hold, not the pump, is the
  dominant battery cost.
* **The vent valve is normally open by default**, so de-energised is open and running a vacuum
  means energising the coil. `NormallyClosedValve` in `Build.cpp` inverts that and removes the
  fail-safe.
* **The panel is the device's.** It shows the controlling link's icon, the run state and the
  battery; there is no drawing command, so everything else — a vent timeout, the duty, the fault
  code — is only on the wire. Poll `vacuum get` to see it.
