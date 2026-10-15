# Plan: split a vacuum system out of VacuumControl

`VacuumControl` currently holds two different jobs in one class: it *drives a plant* (pump,
valve, sensor, PID, target) and it *runs a session* (state machine, hold timer, fault latch,
start/stop policy). The two have different reasons to change — a new pump curve or a different
actuator affects the first, a new run rule affects the second — and they are already
separated in the code by nothing but a `switch`.

This plan introduces `VacuumSystem` as the mid-level abstraction between the drivers and the
run: it owns the chamber's pressure loop and its release path, and `VacuumControl` keeps the
session on top of it.

## 1. Who owns what

| Concern | Today | After |
| --- | --- | --- |
| `targetKpa`, target set/cleared | `VacuumControl` | **`VacuumSystem`** |
| "the target has been reached" (deadband test) | `VacuumControl` | **`VacuumSystem`** |
| current pressure, vacuum, ambient reference, reading validity | `VacuumControl` (via `sensor_`) | **`VacuumSystem`** |
| the PID: integral, slew, deadband coasting, duty output | `VacuumControl` | **`VacuumSystem`** |
| the control period (`CONTROL_PERIOD_MS`) | `VacuumControl::step` | **`VacuumSystem`** |
| valve: close to pull, open to release | `VacuumControl` | **`VacuumSystem`** |
| pump: duty, stop, emergency stop, fault latch on the pump | `VacuumControl` (calls) | **`VacuumSystem`** (calls) |
| "the chamber is released" (vent settled or timed out) | `VacuumControl` | **`VacuumSystem`** |
| `minReachableKpa` (ambient − pump suction) | `VacuumControl` | **`VacuumSystem`** |
| fault *detection* (pump trip, reading lost while regulating) | `VacuumControl` | **`VacuumSystem`** |
| fault *latch* that requires `reset` | `VacuumControl` | `VacuumControl` |
| run state machine `IDLE/PULL/HOLD/VENT/FAULT` | `VacuumControl` | `VacuumControl` |
| hold timer, countdown, when it starts | `VacuumControl` | `VacuumControl` |
| when to start, when to vent, what a stop means | `VacuumControl` | `VacuumControl` |
| `set target off` releasing the chamber (a run rule) | `VacuumControl` | `VacuumControl`, which also clears the system target |

`DECISION:` the system is told what to do and reports what is true; it never decides. Every
"should" — start, stop, is this run over, is this fault permanent — stays in `VacuumControl`.

## 2. The boundary

`app/VacuumSystem.{h,cpp}`:

```cpp
class VacuumSystem {
 public:
  VacuumSystem(Pump& pump, PressureSensor& sensor, VentValve& vent);

  void begin();                  // pump off, valve open: the safe resting state
  void service(uint32_t nowMs);  // the PID service routine, the vent check, fault detection

  // the target the loop regulates to
  void setTarget(float kpa);
  void clearTarget();
  bool targetActive() const;
  float targetKpa() const;
  bool atTarget() const;         // within CONTROL_DEADBAND_KPA

  // the loop and the release path
  bool start();                  // close the valve, reset the loop, begin regulating
  void vent();                   // pump off, then valve open, put the loop to rest
  bool vented() const;           // settled within VENT_MARGIN_KPA, or VENT_TIMEOUT_MS passed
  bool regulating() const;

  // what the chamber is doing
  float kpa() const;
  float vacuumKpa() const;
  float vacuumPercent() const;
  float ambientKpa() const;
  bool readingValid() const;
  float duty() const;
  float minReachableKpa() const;
  bool setAmbientKpa(float kpa);
  void captureAmbient();

  // what went wrong
  bool faulted() const;
  const char* faultCode() const;
  void resetFault();

 private:
  enum class Activity : uint8_t { Resting, Regulating, Venting };
  void regulate(uint32_t nowMs);
  void fail(const char* code);
  void pidReset();
  float pidUpdate(float dt, float error, float measurementSlew);
  ...
};
```

`app/VacuumControl.{h,cpp}` shrinks to the session:

```cpp
class VacuumControl {
 public:
  explicit VacuumControl(VacuumSystem& system);

  void update(uint32_t nowMs);          // services the system, then decides transitions

  // run-aware commands: these are the ones that mean something about a *run*
  void setTarget(float kpa);            // delegates to the system
  void clearTarget();                   // delegates, and releases if a run is active
  void setDurationSeconds(uint32_t seconds);
  bool startRun();                      // preconditions, then system.start()
  void stopRun();                       // system.vent()
  void clearFault();                    // clears the system fault and the latch

  RunState state() const;
  uint32_t durationSeconds() const;
  uint32_t remainingSeconds() const;
  ...
};
```

`DECISION:` `Activity` is a three-value enum, not two booleans, because "regulating and
venting at once" is not a state the plant can be in. It is **not** a second copy of the run
state machine: it has no decisions in it, and `PULL` vs `HOLD` — the one distinction the
operator cares about — exists only in `VacuumControl`.

`DECISION:` `VacuumControl::update()` services the system and then evaluates its own
transitions, so there is exactly one entry point and no way for the loop to be serviced twice
or out of order.

## 3. What deliberately does not move

* **The drivers keep their `update()` calls where they are.** `App` services `PressureSensor`,
  `Pump` and `Battery` every loop; the system reads the latest filtered pressure but does not
  own the sampling cadence. One place polls hardware, and it is the same place for all three.
* **The snapshot stays the interface between the machine and its readers.**
* **Nothing in `hal/` changes at all.**

## 4. Behaviour that must not change

P1 is a pure split, so this is its contract. Each line is a thing to check in the diff:

1. the valve closes **before** the first duty command, so the pump never pulls against an open port
2. a release stops the pump **then** opens the valve, and resets the loop output to zero
3. a fault emergency-stops the pump **then** opens the valve, and latches the code
4. the PID steps at `CONTROL_PERIOD_MS`, and only while regulating
5. the target-reached test is the same comparison against the same deadband
6. vent completion is "within `VENT_MARGIN_KPA` of ambient" or "`VENT_TIMEOUT_MS` elapsed"
7. fault codes stay `max_run` (from the pump) and `sensor` (reading lost while regulating)
8. `set target off` during `PULL`/`HOLD` still releases the chamber; during `IDLE` it still
   asserts the resting state
9. `startRun()` still refuses with no target, no valid reading, or a latched fault
10. `minReachableKpa` is still `max(ambient − PUMP_MAX_SUCTION_KPA, CONTROL_MIN_TARGET_KPA)`
11. every payload value is unchanged: `kpa`, `vac`, `pct`, `floor`, `duty`, `state`, `remain`, `total`
12. `begin()` still asserts the safe resting state once at boot
13. the countdown still starts on the **first** reach of the target, and only from `PULL`
14. `durationSeconds() == 0` still means "no timer": `remain = 0`, and holding never expires
15. the deep-target case still coasts (`error < -deadband` → pump stopped, chamber bleeds up)

## 5. Wiring

| | Today | After P1 | After P2 |
| --- | --- | --- | --- |
| `App` members | sensor, pump, vent, battery, panel, control, snapshot, page, link | + `system_` | same |
| `SerialLink` refs | pump, sensor, control | unchanged | **control, system** |
| `SnapshotBuilder` refs | pump, sensor, control, battery | unchanged | **system, control, battery** |
| `VacuumControl` ctor | pump, sensor, vent | **system** | same |
| `VacuumControl::begin()` | yes | gone; `App` calls `system_.begin()` | same |

P2 is the surface reduction that the split makes possible: `SerialLink` stops needing the pump
and the sensor, `SnapshotBuilder` stops needing the pump and the sensor, and `VacuumControl`
stops exposing `targetActive`, `targetKpa` and `minReachableKpa` because the readers now ask
the system, which owns them.

## 6. Phases

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | Split the plant out | add `app/VacuumSystem.{h,cpp}` with the PID, target, valve, pump calls, pressure reads, vent completion and fault detection; `VacuumControl` becomes the session over it; `SerialLink` and `SnapshotBuilder` untouched | green, layer rule ok, §4 checklist read against the diff |
| 2 | Retarget the readers | `SnapshotBuilder` takes (system, control, battery); `SerialLink` takes (control, system), and the control's target/floor delegates go | green |
| 3 | Docs | README: describe the two levels where the architecture and control model are explained; this plan deleted | green |

## 7. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | "this is one state machine cut in two" | the system has three values and no decisions; §1 and §2 name the split, and `PULL`/`HOLD`/`FAULT` live only above it |
| R2 | behaviour drifts while code moves | §4 is a 15 line checklist, and P1 is one commit that changes no reader |
| R3 | the run rule "clearing the target releases the chamber" gets lost on the way down | it stays in `VacuumControl::clearTarget`, which delegates only the value (§1, last row) |
| R4 | the loop gets serviced twice, or from two places | one entry point: `control_.update(now)` calls `system_.service(now)` |
| R5 | two similar names confuse the next reader | the classes are named for what they own: the system is the plant, the control is the session |
| R6 | the split adds a hop in front of every plant call for no immediate benefit | the benefit is that the run rules and the plant rules stop sharing a class; keep the system's API to what the control and the readers actually use, with nothing speculative in it |
| R7 | no host test can cover the boundary, because the collaborators are concrete drivers | accepted: verification is the build, the layer rule and the checklist. This boundary is where fakes would go if a host harness ever arrives, but adding virtual interfaces for that is a different decision and not part of this plan |

## 8. Questions

Recommendations are marked, so "all as recommended" is a valid answer.

**Answered (rev 2): all fourteen as recommended, so no design changes and no section is
rewritten.** The four worth re-reading before the split lands, because they are the ones that
decide where code ends up: the valve moves down wholly and the control stops touching it (Q2),
fault *detection* splits from the fault *latch* (Q3), the ambient reference moves with the loop
(Q10), and `App` keeps polling the drivers (Q12).

| # | Question | Recommendation |
| --- | --- | --- |
| Q1 | Name and place: `app/VacuumSystem.{h,cpp}`? | **yes**; `hal/` stays drivers only, since this coordinates three of them and runs a loop. Alternatives if you dislike the name: `VacuumPlant`, `Chamber`, `PressureLoop` |
| Q2 | Does the valve move wholly into the system, so `VacuumControl` never touches it? | **yes** |
| Q3 | Fault *detection* in the system (pump trip, reading lost while regulating), fault *latch* in the control? | **yes** |
| Q4 | Does vent completion (settled or timed out) move into the system as `vented()`? | **yes**, so the control asks a question instead of comparing a pressure |
| Q5 | System activity as `enum class Activity { Resting, Regulating, Venting }` rather than two booleans? | **yes** |
| Q6 | One entry point: `VacuumControl::update()` services the system, and `App` calls only the control? | **yes** |
| Q7 | Does `atTarget()` (the deadband test) live in the system? | **yes**: the deadband is part of the loop's definition |
| Q8 | Does `minReachableKpa()` move down, and the control's copy go? | **yes** |
| Q9 | Delete `VacuumControl::begin()`; `App` calls `system_.begin()` instead? | **yes** |
| Q10 | Does the system take the ambient reference API (`setAmbientKpa`, `captureAmbient`, `ambientKpa`), so the interface can drop `PressureSensor&`? | **yes**: it is where "vacuum" is defined |
| Q11 | Keep the name `VacuumControl` for the session layer, rather than renaming to something like `VacuumRun`? | **yes**: the protocol and README already talk about the run, and renaming churns them for no functional gain |
| Q12 | Should `App` keep servicing the drivers (`sensor_.update()`, `pump_.update()`), with the system only reading the filtered value? | **yes**, as §3 argues: one place polls hardware |
| Q13 | P2 as written: readers retargeted, control delegates deleted? | **yes** |
| Q14 | README: add a short "two levels" note where the control model is described? | **yes**, since the diagram and prose there describe only one thing today |

## 9. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: `VacuumSystem` split, run-aware delegates kept, reader retargeting as P2 | all |
| rev 2 | Q1–Q14 answered as recommended; no design changes | §8 |
