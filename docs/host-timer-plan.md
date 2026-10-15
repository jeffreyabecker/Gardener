# Plan: the run's duration belongs to the controller

`VacuumControl` currently owns a countdown: `durationSeconds_`, `remainingSeconds_`,
`countdownEndMs_`, `setDurationSeconds()`, a `PULL → HOLD` transition whose only side effect is
arming the clock, a `HOLD` branch that does nothing but tick it, and an expiry that vents the
chamber. The device decides when a hold is over. The controller asks for it.

That is the wrong end of the cable for a duration. The device has no idea why the run exists —
whether the hold is a 30 second check or a 20 minute degas — and the number only ever comes
from the controller in the first place. This plan removes the device's clock and leaves the
duration, and the decision to end the run, with the controller.

## 1. Who owns what

| Concern | Today | After |
| --- | --- | --- |
| `durationSeconds`, the configured hold | `VacuumControl` | **gone** |
| `remainingSeconds`, `countdownEndMs` | `VacuumControl` | **gone** |
| when the hold started | the device, on the `PULL → HOLD` edge | the controller, watching for that same edge |
| when the hold ends | the device, on expiry, then `VENT` | the controller, with `stop` |
| has the target been reached (the deadband test) | `VacuumSystem` | `VacuumSystem`, unchanged |
| that being at target is a distinct state | `RunState::Holding` | `RunState::Holding`, re-explained |
| `set timer`, `remain`, `total` | serial interface | **gone** |
| the countdown field on the page | `StatusPage` | **gone** |

`DECISION:` nothing replaces the timer in the device. No elapsed counter, no "held for" field,
no pushed display value. The controller has a clock; a second one in the device would be the
same number computed twice, and this device exists to hold a pressure, not to keep time.

## 2. What the state machine means afterwards

```
        start
IDLE ──────────> PULL ──(target reached)──> HOLD ─────────────┐
  ^               │                                          │
  │               │  stop / link loss / fault                 │
  │               v                                          v
  └──(vent complete)──────────────── VENT <───────────────────┘
                                                      stop / fault
```

`HOLD` stays, and it is worth keeping for a reason that gets *stronger* with this change: the
target-reached edge is the one event the controller cannot work out for itself. It can watch
`kpa`, but the deadband that decides "this is close enough to count as arrived" is the device's
loop, so the `PULL → HOLD` transition is the device telling the controller "start counting now".
After this change that is the state's whole job:

* `PULL` — regulating, still coming down
* `HOLD` — regulating, at target, waiting for the controller to say when
* the only ways out of `HOLD` are `stop`, `set target off`, link loss, or a fault

`DECISION:` no rename. `Holding` still means holding; what changed is that nothing in the device
decides when it is over.

## 3. What deliberately does not move

* **The deadband.** `atTarget()` still lives in `VacuumSystem` and still drives the transition
  and the PID's zero-error band. It is the loop's definition, not a timer's.
* **`stop` still vents.** The controller ending a hold is the normal case now, and it takes the
  same release path it always did.
* **Link loss still stops and vents** (the Q4 rule). It goes from "one of several endings" to
  the device's only automatic one, and it is what stops a hold if the controller dies.
* **The vent settle rule.** `VENT_MARGIN_KPA` or `VENT_TIMEOUT_MS`, unchanged — that timeout is
  about the chamber, not the run.
* **Nothing else on the wire changes shape.** `kpa vac pct target floor duty state batt sensor
  fault up` keep their order, units and decimals.

## 4. Behaviour that must not change

1. `start` closes the valve before the first duty command
2. reaching the target still moves `PULL → HOLD`, once, on the same `CONTROL_DEADBAND_KPA`
   comparison against the same filtered pressure
3. the PID still runs while regulating in either state, and still coasts when the chamber is
   deeper than the target
4. `stop` still vents, from `PULL`, `HOLD` or `VENT`
5. `set target off` during `PULL` or `HOLD` still releases the chamber, and in `IDLE` still
   asserts the resting state
6. link loss still stops and vents
7. faults still latch (`sensor`, `max_run`) and still need `reset`
8. vent completion is still "within `VENT_MARGIN_KPA` of ambient" or "`VENT_TIMEOUT_MS` elapsed"
9. every payload key that survives is byte identical
10. the device no longer ends a hold by itself: after this change there is no timeout anywhere
    in the run model

## 5. Wiring, protocol and version

| | Today | After |
| --- | --- | --- |
| Commands | `info status help start stop reset reboot ambient set target\|timer\|ambient\|log` | `set target\|ambient\|log`; `set timer` is gone |
| `status` payload | `... state remain total batt ...` | `... state batt ...` |
| `info` payload | `proto=3` | `proto=4` |
| Firmware version | `0.2.0` | `0.3.0` |
| `Snapshot.h` | `remainSeconds`, `totalSeconds` | gone, so the snapshot carries no time at all |
| `StatusPage` | `drawCountdown`, `twoDigits` | gone; the band at y=36..52 goes to the duty bar |

The wire breaks, so `proto` goes to 4 by the rule already written in `core/Config.h`, and `fw`
moves with it because a v3 controller parsing a v4 reply would silently see fewer keys.

## 6. Phases

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | The device stops timing | drop the countdown, its fields and `setDurationSeconds` from `VacuumControl`; drop `remain`/`total` from `Snapshot`, `SnapshotBuilder`, `SerialLink`; drop `set timer`; `proto` 4, `fw` 0.3.0 | green, layer rule ok, §4 checklist read against the diff, payload key order unchanged |
| 2 | The page stops showing a clock | delete `drawCountdown` and `twoDigits`; give the freed band to the duty bar (y=38, height 22) | green; the page still fills the panel top to bottom |
| 3 | Docs and the CLI | `docs/serial-protocol.md` and README: the state table, the run model, the payload tables, the examples and the version; drop `timer` from the CLI's `set` choices and its example | green, `py_compile` |

## 7. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | an unattended hold now runs until someone stops it, and a normally open valve draws coil current for the whole time | accepted and documented: the duration was always the controller's business, and link loss still releases. A controller that hangs with the port open holds forever, which is the price of the controller owning the clock |
| R2 | the pump's `PUMP_MAX_RUN_MS` (15 minutes of continuous running) becomes the practical ceiling on a pull, and a deep target that keeps pumping will fault before the controller is done | document it, no code change: it is the pump's protection, it counts *running* time, and a hold that has settled at target with duty at zero does not accumulate it. Today a 30 minute timer hits the same wall |
| R3 | a v3 controller silently misreads a v4 payload, because the keys it wants are simply absent | `proto` bump, and the doc, CLI and README move in the same phase as the wire |
| R4 | the operator standing at the rig loses the only "how much longer" cue on the panel | accepted: that number was never the device's. If the rig needs it later, the controller can push a display value, which is a separate change and not part of this one |
| R5 | `HOLD` keeps a name that used to mean "counting down", so trimming the doc's prose would leave it wrong | the state table, the diagram and the prose are rewritten in phase 3, and the state is documented as the controller's cue rather than a timer |

## 8. Questions

Recommendations are marked, so "all as recommended" is a valid answer.

| # | Question | Recommendation |
| --- | --- | --- |
| Q1 | Does `HOLD` survive as a state, meaning "at target, waiting for the controller", with only its timer removed? | **yes**: and it matters more now, because `PULL → HOLD` is the cue the controller times from and it cannot work the deadband out for itself |
| Q2 | Remove `set timer`, `remain` and `total` outright, with nothing in their place? | **yes** |
| Q3 | `proto` 3 → 4 and `fw` 0.2.0 → 0.3.0? | **yes**: keys disappear, so the wire breaks |
| Q4 | Is `stop`, `set target off`, link loss and a fault the complete set of ways a run ends? | **yes**, and the device never ends one on its own |
| Q5 | Display: delete the countdown field and give y=36..52 to the duty bar (y=38, height 22)? | **yes**. Alternatives: pressure digits to size 3 with the rows re-spaced, or leave the band empty. The bar is the only activity indicator left, and a taller bar is a smaller change than a bigger type size |
| Q6 | No time-related telemetry in its place — no elapsed-at-target, no "held for" key? | **yes**. If the panel ever needs a countdown again, the controller pushes it: that is a display value, not a device clock |
| Q7 | `set target off` during `PULL`/`HOLD` still releases the chamber? | **yes**, unchanged: a run without a target is not a run |
| Q8 | Link loss still stops and vents? | **yes**, unchanged, and now the device's only automatic ending |
| Q9 | Document that `PUMP_MAX_RUN_MS` is the practical ceiling on a pull, rather than changing or removing it? | **yes**, document only |
| Q10 | Keep the `Holding` name rather than renaming it `AtTarget` or `Run`? | **yes**: the protocol, the page and the docs all say `HOLD`, and it is still what the device is doing |
| Q11 | CLI: drop `timer` from the `set` field choices and the docstring example, and change nothing else? | **yes** |
| Q12 | Is the `PULL → HOLD` transition still worth having, given nothing happens on it but a reported state change? | **yes**: it is the only thing that tells a controller the target has been reached by the device's own deadband, which is exactly when its countdown should start |

## 9. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: no device clock, `HOLD` kept as the controller's cue, wire at proto 4 | all |

## 10. Assumption

This plan assumes the serial interface survives. You asked separately about dropping the serial
protocol altogether; that question is unanswered and untouched here, because this change is
about who owns a duration, not about how it is transported. Every decision above would hold
unchanged if the transport were replaced — the controller would still be the one counting.
