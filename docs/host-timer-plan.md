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

The answers change three of my recommendations: the pump's run protection goes rather than being
documented, the protocol version concept goes with it, and the countdown stays on the panel fed
by the controller rather than being deleted. `HOLD` also stays: what goes is the clock it was
carrying.

`DECISION:` rev 3 settles Q1 as "the hold's *timer* goes, the *state* stays", so `PULL → HOLD`
survives as the cue the controller times from, and `atTarget()` survives with it.

## 1. Who owns what

| Concern | Today | After |
| --- | --- | --- |
| `durationSeconds`, the configured hold | `VacuumControl` | **gone** |
| `remainingSeconds`, `countdownEndMs` | `VacuumControl` | **gone** |
| when a hold ends | the device, on expiry, then `VENT` | the controller, with `stop` |
| `atTarget()`, the public "has the target been reached" query | `VacuumSystem` | `VacuumSystem`, unchanged |
| being at target as a distinct state | `RunState::Holding` | `RunState::Holding`: the timer goes, the cue stays |
| the countdown field on the page | counted down by the device | **rendered** from a value the controller pushes |
| the pump's 15 minute run trip | `Pump` | **gone**, with `PUMP_MAX_RUN_MS` |
| the wire version | `cfg::PROTOCOL_VERSION`, `proto=` | **gone**; `fw` is the only version marker left |
| fault codes | `max_run`, `sensor` | `sensor` |
| has the target been reached | the device decides and says so | the device still decides it; that edge is now the controller's cue to start its own countdown |

`DECISION:` the device owns no duration in any form — not to act on, not to report, not to
derive. The one time-shaped thing that survives is a number the controller hands down for the
panel to draw, and the device never does arithmetic on it.

## 2. The run model afterwards

```
        start                                              stop / link loss / fault
IDLE ──────────> PULL ──(target reached)──> HOLD ───────────────────> VENT ──(complete)──> IDLE
  ^                                              │                      │
  │                                              │                      v
  └──────────────────── reset ───────────────────┴─────────────────── FAULT
```

* `start` — `IDLE` → `PULL`, valve closed before the first duty command
* `PULL → HOLD` — the target-reached edge, on the same deadband comparison as today
* `HOLD` — regulating at the target, waiting for the controller to say the hold is over. It no
  longer counts anything: the countdown that used to live here belongs to the controller
* `stop`, link loss, `set target off` or a fault → `VENT`, then `IDLE` or `FAULT`
* nothing in the device ends a run: no duration, no timeout, no run-time trip

`HOLD` earns its place for a reason this change makes *stronger*: the target-reached edge is the
one event the controller cannot work out for itself. It can watch `kpa`, but the deadband that
decides "close enough to count as arrived" is the device's loop, so `PULL → HOLD` is the device
telling the controller "start counting now" — and with the device's own clock gone, that is the
state's whole job.

## 3. What deliberately does not move

* **The deadband and the arrival query.** `CONTROL_DEADBAND_KPA` still decides where the PID's
  error goes to zero, and `atTarget()` still answers "have we arrived" for the transition.
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
10. reaching the target still moves `PULL → HOLD`, once, on the same `CONTROL_DEADBAND_KPA`
    comparison against the same filtered pressure
11. a run that reaches the target keeps regulating there indefinitely, exactly as it does today
    when no timer is set
12. `HOLD` is still reported on the wire and on the page, and is still what says the target has
    been reached
13. nothing in the firmware ends a run by itself, and no field, key or constant represents a
    duration

## 5. Wiring, protocol and version

| | Today | After |
| --- | --- | --- |
| Commands | `... set target\|timer\|ambient\|log` | `... set target\|ambient\|log\|hold` |
| `status` payload | `... state remain total batt ...` | `... state batt ...` |
| `info` payload | `proto=3 fw=0.2.0 ...` | `fw=0.3.0 ...` |
| `fault` values | `none`, `max_run`, `sensor` | `none`, `sensor` |
| `state` values | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT` | unchanged: `HOLD` stays |
| `core/Config.h` | `PROTOCOL_VERSION`, `PUMP_MAX_RUN_MS` | both gone |
| `hal/Pump` | run-time trip, `runStartMs_`, `running_` | gone |
| `core/Snapshot.h` | `remainSeconds`, `totalSeconds` | `holdSeconds`: pushed, drawn, never counted |
| `app/StatusPage` | counts down a clock it owns | draws, while a run is in `PULL` or `HOLD`, a number it was handed |

`DECISION:` with `proto` gone there is no wire version, so `fw` becomes the only marker a
controller can branch on. That is why it moves now rather than with some later change: it is
the whole of the contract's versioning.

## 6. Phases

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | The device stops timing | `VacuumControl` loses the countdown and keeps `Holding`; `Snapshot`/`SnapshotBuilder` swap `remain`/`total` for the pushed hold; `SerialLink` drops `set timer`, `remain`, `total` and `proto`, and gains `set hold`; `fw` 0.3.0 | green, layer rule ok, §4 read against the diff, surviving key order unchanged, the `PULL → HOLD` edge untouched |
| 2 | The pump loses its run-time protection | `PUMP_MAX_RUN_MS`, `runStartMs_`, `running_`, the trip, and the `max_run` fault code | green; no `max_run` left anywhere |
| 3 | The page draws the controller's countdown | `drawCountdown` gates on `PULL` or `HOLD` and renders `holdSeconds`; `twoDigits` stays, and the layout does not move | green; the panel still fills top to bottom |
| 4 | Docs and the CLI | protocol doc and README: the run model, state table, payload tables, examples, protections and versions; CLI gains `hold` and loses `timer` | green, `py_compile` |

## 7. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | a run now has no ending of its own at all. An unattended hold runs until someone stops it, the normally open valve draws coil current the whole time, and a target below the reachable floor pins duty at the cap with nothing left to stop it | accepted and documented, with the duty cap, the slew limit and link loss as what remains. The controller can read `floor` to avoid the unreachable target, and the safety section already says the firmware's limits are software limits only |
| R2 | link loss covers a controller that leaves, not one that hangs with the port open | accepted: that is the price of the controller owning the clock, and it is stated in the docs rather than defended against in firmware |
| R3 | with `proto` gone a controller cannot detect the layout change from the wire itself; it would silently see fewer keys | `fw` is bumped in the same phase as the wire, and the doc, README and CLI move with it |
| R4 | Q1 read as "the state goes" contradicts Q10 and Q12, which keep the name and the transition | settled in rev 3: Q1 meant the *timer* goes, so the state, the transition and `atTarget()` all stay, and the doc's prose about `HOLD` is rewritten rather than deleted |
| R5 | the protocol doc's run model is written around `HOLD` and a countdown, so it needs rewriting rather than trimming | phase 4 rewrites the model, the state table and the prose together |

## 8. Questions

Q1–Q12 are answered as recorded. Q13 settles how Q1, Q10 and Q12 fit together.

| # | Question | Answer (rev 2) |
| --- | --- | --- |
| Q1 | Does `HOLD` survive as a state? | **the hold's clock goes, the state stays** (Q13) |
| Q2 | Remove `set timer`, `remain` and `total` outright? | yes |
| Q3 | `proto` bump and `fw` bump? | **drop the protocol version concept entirely**; `fw` is covered by Q17 |
| Q4 | Is `stop`, `set target off`, link loss and a fault the complete set of endings? | yes |
| Q5 | Display: delete the countdown field? | **no: still display it, fed from the controller** |
| Q6 | No time-related telemetry in its place? | **forced in from the controller** (same as Q5) |
| Q7 | `set target off` during a run still releases the chamber? | correct |
| Q8 | Link loss still stops and vents? | yes |
| Q9 | Document `PUMP_MAX_RUN_MS` as the ceiling? | **drop the constant entirely** |
| Q10 | Keep the `Holding` name? | yes: nothing is renamed, and `HOLD` now means "at target, waiting to be told" |
| Q11 | CLI: drop `timer`, change nothing else? | yes, plus the `hold` counterpart (Q11 in rev 1 only knew about removal) |
| Q12 | Keep the `PULL → HOLD` transition? | yes: it is the controller's cue that the target has been reached by the device's own deadband |

New decisions the answers created:

| # | Question | Recommendation |
| --- | --- | --- |
| Q13 | Does Q1 remove the state or the clock? | **the clock**: `HOLD` survives with its timer removed, so Q10 and Q12 stand and `atTarget()` stays |
| Q14 | The controller feeds the countdown with `set hold <seconds\|off>`, answering `ok hold=572`, and `set hold off` clears it? | **yes**: the wording matches what the panel shows, and `timer` would be a lie now that the device keeps no time |
| Q15 | Does `start` clear the pushed countdown, so a new run shows `--:--` until the controller sends one, and is the value drawn only while a run is in `PULL` or `HOLD`? | **yes**: otherwise a value set for one run lingers into the next, and the gate then matches today's, which is the same pair of states |
| Q16 | Does `Pump` keep `emergencyStop`/`tripped` as its own interlock — it is what makes `setDuty` a no-op after a fault — even though the `sensor` fault is now its only producer? | **yes**: it is the driver's own protection and the reason a fault cannot be pumped through, so it stays |
| Q17 | `fw` 0.2.0 → 0.3.0? | **yes**: with `proto` gone, `fw` is the only version marker a controller can branch on |

## 9. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: no device clock, `HOLD` kept as the controller's cue, wire at proto 4, run protection documented | all |
| rev 2 | Q1 removes `HOLD` (making Q10 and Q12 moot), Q3 removes the protocol version concept, Q5/Q6 keep a countdown fed from the controller, Q9 removes the pump's run-time protection; display and docs phases split out | §1–§8 |
| rev 3 | Q13 settles Q1: the timer goes and the state stays, so `HOLD`, the transition and `atTarget()` survive; the countdown is drawn for `PULL` or `HOLD` | §1–§8 |

## 10. Assumption

This plan assumes the serial interface survives. You asked separately about dropping the serial
protocol altogether; that question is unanswered and untouched here, because this change is
about who owns a duration, not about how it is transported. Every decision above would hold
unchanged if the transport were replaced — the controller would still be the one counting.
