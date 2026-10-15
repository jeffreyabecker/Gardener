# Adopt the simplified VacuumSystem

Status: approved plan, executed separately. No behavior change.

## Goal

Reduce `VacuumSystem`'s public surface to **7 commands + 1 read**, per the approved
proposal. The wire protocol (`status` / `info` payloads) must be byte-identical to today,
and the build must stay green.

Current surface: 23 public members, of which 15 are read getters that `App` copies
one-by-one into `SystemSnapshot`.

## Target surface

```cpp
class VacuumSystem {
 public:
  VacuumSystem(Pump& pump, PressureSensor& sensor, VentValve& vent);

  void begin();
  void service(uint32_t nowMs);

  // --- commands ---
  void setTarget(float kpa);
  void clearTarget();
  bool start();                  // false: no target/reading, or faulted
  void stop();                   // the one release path: stop + vent
  void resetFault();
  bool setAmbient(float kpa);    // renamed from setAmbientKpa; false when out of range
  void captureAmbient();

  // --- the one read ---
  PlantState state() const;

 private:
  void setState(RunState state);
  bool atTarget() const;
  bool vented() const;
  void vent();                   // now private; used by stop() and resetFault()
  void rest();                   // now private; used by begin() and idle settle
  void regulate(uint32_t nowMs);
  void fail(const char* code);
  float minReachableKpa() const; // now private; folded into state()
  void pidReset();
  float pidUpdate(float dt, float error, float measurementSlew);

  // ... existing private members unchanged ...
};
```

## Changes

### 1. `core/Snapshot.h`

Add `PlantState` beside `RunState`:

```cpp
struct PlantState {
  RunState runState = RunState::Idle;
  float kpa = 0, vacuumKpa = 0, vacuumPercent = 0, ambientKpa = 0;
  float targetKpa = 0; bool targetActive = false;
  float duty = 0;
  float floorKpa = 0;                     // deepest reachable target
  bool sensorOk = false, readingValid = false;
  bool faulted = false;
  const char* faultCode = "";
};
```

Replace the 13 flat plant fields in `SystemSnapshot` with one member:

```cpp
struct SystemSnapshot {
  PlantState plant;
  // battery + device fields unchanged
};
```

### 2. `app/VacuumSystem.h` / `.cpp`

- Adopt the target surface above.
- `setAmbientKpa` becomes `setAmbient`; keep the range check semantics.
- `vent()`, `rest()`, `minReachableKpa()` become private.
- Implement `PlantState state() const` from the existing getters, then delete the
  individual public getters.

### 3. `app/App.cpp`

- `refreshSnapshot()`: replace the 13 `system_.xxx()` assignments with
  `snapshot_.plant = system_.state();`, keeping the battery/uptime assignments.
- `begin()` line ~57: `lastState_ = system_.state();` → `system_.state().runState`.
- `logRunChanges()` line ~102: `RunState state = system_.state();` →
  `RunState state = system_.state().runState;`.

### 4. `app/SerialLink.cpp`

- Command handlers: `setAmbientKpa` → `setAmbient` (two sites), `ambientKpa()` →
  `state().ambientKpa` (two sites).
- `statusBody()`: rename flat snapshot fields through `snapshot_.plant.*` per the table.

## Field mapping (old flat snapshot → PlantState)

| Old | New |
| --- | --- |
| `snapshot_.kpa` | `snapshot_.plant.kpa` |
| `snapshot_.vacuumKpa` | `snapshot_.plant.vacuumKpa` |
| `snapshot_.vacuumPercent` | `snapshot_.plant.vacuumPercent` |
| `snapshot_.ambientKpa` | `snapshot_.plant.ambientKpa` |
| `snapshot_.sensorOk` | `snapshot_.plant.sensorOk` |
| `snapshot_.readingValid` | `snapshot_.plant.readingValid` |
| `snapshot_.duty` | `snapshot_.plant.duty` |
| `snapshot_.pumpTripped` | `snapshot_.plant.faulted` |
| `snapshot_.faultCode` | `snapshot_.plant.faultCode` |
| `snapshot_.state` | `snapshot_.plant.runState` |
| `snapshot_.targetActive` | `snapshot_.plant.targetActive` |
| `snapshot_.targetKpa` | `snapshot_.plant.targetKpa` |
| `snapshot_.floorKpa` | `snapshot_.plant.floorKpa` |

`batteryPresent`, `batteryLow`, `batteryVolts`, `batteryPercent`, `uptimeMs` are untouched.

## Acceptance criteria

- `powershell -File scripts/arduino.ps1 -Task build` succeeds (runs the layer check).
- `status` and `info` payloads are unchanged from before the change.
- No behavior change anywhere else; `VacuumSystem` has exactly the public surface above.

## Not in scope

- Berry / `ProgramApi` (a later change; it will read `vacuum.state()`).
- Serial protocol documentation (nothing on the wire changes).
