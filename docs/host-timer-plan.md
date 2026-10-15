# Plan: the controller owns the clock

`VacuumControl` owns a countdown: `durationSeconds_`, `remainingSeconds_`, `countdownEndMs_`, a
`PULL → HOLD` transition whose only side effect is arming the clock, a `HOLD` state that does
nothing but tick it, and an expiry that vents the chamber. Underneath, `Pump` owns a second
clock: `PUMP_MAX_RUN_MS`, a 15 minute continuous-run trip that faults the run. And `Config.h`
owns a third thing that is really a time: `PROTOCOL_VERSION`, a number whose only job is to say
which shape the wire is in.

All three come from the same place. The device is deciding things about duration that only the
controller knows, and the duration only ever arrives from the controller in the first place.
This plan removes every one of them.

Rev 2 answers change three of my recommendations: the `HOLD` state goes, the pump's run
protection goes rather than being documented, and the protocol version concept goes with it.
The countdown stays on the panel, but the controller feeds it.

## 1. Who owns what

| Concern | Today | After |
| --- | --- | --- |
| `durationSeconds`, the configured hold | `VacuumControl` | **gone** |
| `remainingSeconds`, `countdownEndMs` | `VacuumControl` | **gone** |
| when a hold ends | the device, on expiry, then `VENT` | the controller, with `stop` |
| `atTarget()`, the public "has the target been reached" query | `VacuumSystem` | **gone**; the deadband itself stays inside the loop |
| "at target" as a distinct state | `RunState::Holding` | **gone**: a run is `PULL` from `start` to `stop` |
| the countdown field on the page | counted down by the device | **rendered** from a value the controller pushes |
| the pump's 15 minute run trip | `Pump` | **gone**, with `PUMP_MAX_RUN_MS` |
| the wire version | `cfg::PROTOCOL_VERSION`, `proto=` | **gone**; `fw` is the only version marker left |
| fault codes | `max_run`, `sensor` | `sensor` |
| has the target been reached | the device decides and says so | the controller reads `kpa` against `target` |

`DECISION:` the device owns no duration in any form — not to act on, not to report, not to
derive. The one time-shaped thing that survives is a number the controller hands down for the
panel to draw, and the device never does arithmetic on it.

## 2. The run model afterwards

```
        start                                    stop / link loss / fault
IDLE ──────────> PULL ──────────────────────────────────────> VENT ──(vent complete)──> IDLE
                   ^                                            │
                   │                                            v
                   └────────────── reset ──────────────────── FAULT
```

* `start` — `IDLE` → `PULL`, valve closed before the first duty command
* `PULL` — regulating, and there is no other run state. It lasts until the controller stops it
* `stop`, link loss, `set target off` or a fault → `VENT`, then `IDLE` or `FAULT`
* nothing in the device ends a run: no duration, no timeout, no run-time trip

Two consequences worth stating, because they are visible to a controller:

* **The device no longer reports the arrival edge.** With `HOLD` gone, the controller compares
  `kpa` against `target` itself. The loop still applies `CONTROL_DEADBAND_KPA` to duty, so
  `kpa <= target` is the arrival condition a controller should use, and it is the moment its
  own countdown should start.
* **`PULL` is the state for the whole run**, so the panel's state row reads `PULL` throughout.
  The cue a person standing at the rig gets for "the hold has started" is the countdown the
  controller pushes.

## 3. What deliberately does not move

* **The deadband.** `CONTROL_DEADBAND_KPA` still decides where the PID's error goes to zero.
  Only the public query that asked "have we arrived" goes.
* **`stop` still vents**, and it is now the normal way a run ends.
* **Link loss still stops and vents.** It is the device's only automatic ending and it is what
  releases the chamber if the controller dies.
* **The vent settle rule.** `VENT_MARGIN_KPA` or `VENT_TIMEOUT_MS` — that timeout is about the
  chamber, not the run, and it stays.
* **The pump's duty cap and slew limit.** With the run trip gone these are the pump's only
  protections, so neither is touched.
* **Everything else on the wire.** `kpa vac pct target floor duty state batt sensor fault up`
  keep their order, units and decimals.

## 4. Behaviour that must not change

1. `start` closes the valve before the first duty command
2. the PID still steps at `CONTROL_PERIOD_MS` while regulating, and still coasts when the
   chamber is deeper than the target
3. `stop` still vents, from `PULL` or from `VENT`
4. `set target off` during a run still releases the chamber, and in `IDLE` still asserts the
   resting state
5. link loss still stops and vents
6. faults still latch and still need `reset`, with `sensor` the only code left
7. vent completion is still "within `VENT_MARGIN_KPA` of ambient" or "`VENT_TIMEOUT_MS` elapsed"
8. every surviving payload key is byte identical, in the same order
9. duty is still capped at `PUMP_DUTY_MAX_PERCENT` and still slew limited, even though the run
   trip that guarded the pump is gone
10. a run that reaches the target keeps regulating there indefinitely, exactly as it does today
    when no timer is set
11. nothing in the firmware ends a run by itself, and no field, key or constant represents a
    duration

## 5. Wiring, protocol and version

| | Today | After |
| --- | --- | --- |
| Commands | `... set target\|timer\|ambient\|log` | `... set target\|ambient\|log\|hold` |
| `status` payload | `... state remain total batt ...` | `... state batt ...` |
| `info` payload | `proto=3 fw=0.2.0 ...` | `fw=0.3.0 ...` |
| `fault` values | `none`, `max_run`, `sensor` | `none`, `sensor` |
| `state` values | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT` | `IDLE`, `PULL`, `VENT`, `FAULT` |
| `core/Config.h` | `PROTOCOL_VERSION`, `PUMP_MAX_RUN_MS` | both gone |
| `hal/Pump` | run-time trip, `runStartMs_`, `running_` | gone |
| `core/Snapshot.h` | `remainSeconds`, `totalSeconds` | `holdSeconds`: pushed, drawn, never counted |
| `app/StatusPage` | draws a clock it owns | draws the number it was handed |

`DECISION:` with `proto` gone there is no wire version, so `fw` becomes the only marker a
controller can branch on. That is why it moves now rather than with some later change: it is
the whole of the contract's versioning.

## 6. Phases

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | The device stops timing | `VacuumControl` loses the countdown and `Holding`; `VacuumSystem` loses `atTarget`; `Snapshot`/`SnapshotBuilder` swap `remain`/`total` for the pushed hold; `SerialLink` drops `set timer`, `remain`, `total` and `proto`, and gains `set hold`; `fw` 0.3.0 | green, layer rule ok, §4 read against the diff, surviving key order unchanged |
| 2 | The pump loses its run-time protection | `PUMP_MAX_RUN_MS`, `runStartMs_`, `running_`, the trip, and the `max_run` fault code | green; no `max_run` left anywhere |
| 3 | The page draws the controller's countdown | `drawCountdown` gates on a run in progress and renders `holdSeconds`; `twoDigits` stays, and the layout does not move | green; the panel still fills top to bottom |
| 4 | Docs and the CLI | protocol doc and README: the run model, state table, payload tables, examples, protections and versions; CLI gains `hold` and loses `timer` | green, `py_compile` |

## 7. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | a run now has no ending of its own at all. An unattended hold runs until someone stops it, the normally open valve draws coil current the whole time, and a target below the reachable floor pins duty at the cap with nothing left to stop it | accepted and documented, with the duty cap, the slew limit and link loss as what remains. The controller can read `floor` to avoid the unreachable target, and the safety section already says the firmware's limits are software limits only |
| R2 | link loss covers a controller that leaves, not one that hangs with the port open | accepted: that is the price of the controller owning the clock, and it is stated in the docs rather than defended against in firmware |
| R3 | with `proto` gone a controller cannot detect the layout change from the wire itself; it would silently see fewer keys | `fw` is bumped in the same phase as the wire, and the doc, README and CLI move with it |
| R4 | Q10 asked about keeping the `HOLD` name and Q12 about keeping the arrival transition; Q1 removes the state, which makes both moot, so the two readings cannot both be right | resolved in §8 Q13 before any code moves |
| R5 | the protocol doc's run model is written around `HOLD` and a countdown, so it needs rewriting rather than trimming | phase 4 rewrites the model, the state table and the prose together |

## 8. Questions

Q1–Q12 are answered as recorded. Two of them cannot both be true, which Q13 settles.

| # | Question | Answer (rev 2) |
| --- | --- | --- |
| Q1 | Does `HOLD` survive as a state? | **no: hold goes away** |
| Q2 | Remove `set timer`, `remain` and `total` outright? | yes |
| Q3 | `proto` bump and `fw` bump? | **drop the protocol version concept entirely**; `fw` is covered by Q17 |
| Q4 | Is `stop`, `set target off`, link loss and a fault the complete set of endings? | yes |
| Q5 | Display: delete the countdown field? | **no: still display it, fed from the controller** |
| Q6 | No time-related telemetry in its place? | **forced in from the controller** (same as Q5) |
| Q7 | `set target off` during a run still releases the chamber? | correct |
| Q8 | Link loss still stops and vents? | yes |
| Q9 | Document `PUMP_MAX_RUN_MS` as the ceiling? | **drop the constant entirely** |
| Q10 | Keep the `Holding` name? | yes — **moot**, there is no state left to name (Q13) |
| Q11 | CLI: drop `timer`, change nothing else? | yes, plus the `hold` counterpart (Q11 in rev 1 only knew about removal) |
| Q12 | Keep the `PULL → HOLD` transition? | yes — **moot**, the transition goes with the state (Q13) |

New decisions the answers created:

| # | Question | Recommendation |
| --- | --- | --- |
| Q13 | Confirm that `HOLD` is gone and that Q10 and Q12 are therefore moot? | **yes**: Q1 is the explicit statement, and a state deleted by one answer cannot be named or transitioned by two others |
| Q14 | The controller feeds the countdown with `set hold <seconds\|off>`, answering `ok hold=572`, and `set hold off` clears it? | **yes**: the wording matches what the panel shows, and `timer` would be a lie now that the device keeps no time |
| Q15 | Does `start` clear the pushed countdown, so a new run shows `--:--` until the controller sends one, and is the value drawn only while a run is in `PULL`? | **yes**: otherwise a value set for one run lingers into the next |
| Q16 | Does `Pump` keep `emergencyStop`/`tripped` as its own interlock — it is what makes `setDuty` a no-op after a fault — even though the `sensor` fault is now its only producer? | **yes**: it is the driver's own protection and the reason a fault cannot be pumped through, so it stays |
| Q17 | `fw` 0.2.0 → 0.3.0? | **yes**: with `proto` gone, `fw` is the only version marker a controller can branch on |

## 9. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: no device clock, `HOLD` kept as the controller's cue, wire at proto 4, run protection documented | all |
| rev 2 | Q1 removes `HOLD` (making Q10 and Q12 moot), Q3 removes the protocol version concept, Q5/Q6 keep a countdown fed from the controller, Q9 removes the pump's run-time protection; display and docs phases split out | §1–§8 |

## 10. Assumption

This plan assumes the serial interface survives. You asked separately about dropping the serial
protocol altogether; that question is unanswered and untouched here, because this change is
about who owns a duration, not about how it is transported. Every decision above would hold
unchanged if the transport were replaced — the controller would still be the one counting.
