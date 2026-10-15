# Refactor plan: layering, folders, and deleting `AppContext`

Status: **proposed** · revision 2 · owner: unassigned · scope: `libraries/VacuumCore` only

Revision 2: script hosting moved out of `services/` into `io/`, because the runtime
is an outside interface rather than machine policy. See §3.2.

Annotation guide: open questions are numbered **Q1…Q8**, decisions taken in this
document are marked `DECISION:`. Edit this file in place and the plan will be
picked up from your edits.

## 1. Goals

1. Remove `AppContext` — it hides real dependencies and makes cycles possible.
2. Draw a clear line between base hardware abstractions (pump, sensor, display,
   buttons, storage) and the higher level abstractions built on them.
3. Use folders so the structure of the code is visible without reading it.

Chosen approach: **folders + one-way layer rule + constructor injection, with no
virtual interfaces.** Drivers stay concrete classes injected by reference. Where
this leaves a coupling (command routing reaches for telemetry state) the plan
resolves it with plain data rather than a back-pointer or a vtable, and notes the
forward-declaration fallback you approved. Script hosting is grouped with the other
outside interfaces (`io/`) instead of `services/` — see §3.2.

### Non-goals

- No behaviour change in phases 0–6: same pins, same BLE protocol and framing,
  same UI pages, same button mapping.
- No Berry VM work here (separate task).
- No host unit tests: this machine has no C++ toolchain (no gcc/clang), so
  verification stays `arduino-cli compile` for the ESP32-C3 target.
- No new features. The one intentional behaviour change is the pump suction fix
  in phase 7, which is config semantics only (section 8).

## 2. Diagnosis

### 2.1 `AppContext` is a service locator

`AppContext` is a struct of nine nullable raw pointers to every module
([AppContext.h](../libraries/VacuumCore/src/AppContext.h)). Consequences:

- **Dependencies are invisible.** `ScriptApi` holds `AppContext&` and reaches into
  `Pump`, `PressureSensor`, `DisplayUI`, `Buttons` and `VacuumController` — five
  dependencies behind one pointer, none of them in the header.
- **Failures are runtime, not compile time.** `ctx_` must be populated before any
  `begin()` call or the pointer is null. Nothing enforces the order.
- **Real cycle.** [Protocol.cpp](../libraries/VacuumCore/src/Protocol.cpp)
  includes `BleService.h` while `BleService` owns a `CommandRouter`. The router
  only needs this to call `setTelemetry()` and to read `connected()`.

### 2.2 No layer boundary

`Pump.cpp` (a MOSFET gate and a PWM timer) is a peer of `Protocol.cpp` (command
grammar) in one flat folder. Nothing prevents a driver from calling policy, and
nothing prevents policy from calling the UI.

### 2.3 Flat `src/`

21 files in one directory. Drivers, protocol codec, presentation and application
wiring all look the same, so the graph has to be reconstructed by reading.

## 3. Target structure

One-way dependencies, strictly downward. Nothing below includes anything above it.

```
core/       pure logic: no drivers, no NimBLE, no Adafruit, no Arduino peripherals
hal/        one file per physical device, plus the board pin map
services/   machine policy: what the vacuum system does, driven by the main loop
io/         how the outside world drives it: command grammar, BLE, script host
ui/         local presentation: renders the snapshot, emits intents
app/        composition root: the only place that knows the whole graph
```

```
libraries/VacuumCore/src/
  VacuumCore.h              includes app/App.h only
  core/
    Config.h                policy tunables, leaf (includes nothing)
    Snapshot.h              SystemSnapshot, ProgramInfo, TelemetrySettings, UiIntent
    Units.h / Units.cpp     kPa conversions, clamp, number formatting
    Crc32.h / Crc32.cpp
    Clock.h                 nowMs(): OPTIONAL, see Q1
    Log.h / Log.cpp         level + ring buffer, writes to an installed sink
    Pid.h / Pid.cpp         pure controller math
    Telemetry.h / .cpp      SystemSnapshot -> JSON
    UploadFrames.h / .cpp   B / D / E / X frame decode, pure
  hal/
    Pins.h                  GPIO map, I2C addresses, display geometry, PWM settings
    Pump.h / Pump.cpp
    Tachometer.h / .cpp     optional, split out of Pump
    PressureSensor.h / .cpp
    Display.h / Display.cpp SH1106 + draw primitives + flush
    Buttons.h / Buttons.cpp
    Storage.h / Storage.cpp LittleFS mount / file handles
  services/
    ProgramStore.h / .cpp   program list, staged writes, autostart
    ProgramUploader.h / .cpp chunked upload session (uses UploadFrames + ProgramStore)
    VacuumControl.h / .cpp  PID + safety state machine
    SnapshotBuilder.h / .cpp pump, sensor, control, store -> SystemSnapshot
  io/
    commands/
      CommandRouter.h / .cpp   line grammar (today's Protocol.cpp)
    ble/
      BleService.h / .cpp      NimBLE only; telemetry send loop
    script/
      ScriptRuntime.h          abstract host interface
      ScriptApi.h / .cpp       the machine surface handed to guest code
      NullRuntime.h / .cpp     build without a VM
      berry/                   glue for libraries/Berry, later
        BerryRuntime.h / .cpp
        VacuumModule.h / .cpp
  ui/
    Renderer.h / Renderer.cpp
    UiController.h / .cpp
    pages/StatusPage.cpp
    pages/ProgramsPage.cpp
    pages/SetpointPage.cpp
    pages/GraphPage.cpp
    pages/InfoPage.cpp
  app/
    App.h / App.cpp
```

### 3.1 Layer rules

| Layer | May include |
| --- | --- |
| `core/` | `core/` only |
| `hal/` | `core/` |
| `services/` | `core/`, `hal/` |
| `io/` | `core/`, `hal/`, `services/` |
| `ui/` | `core/`, `hal/` |
| `app/` | everything; nothing includes `app/` |

Within `io/` the subfolders are one-way as well: `ble/ -> commands/ -> script/`.
`script/` includes nothing else from `io/`, which is what keeps the host
independent of the transport it happens to be driven through.

`ui/` never including `services/` is possible without interfaces because
`SystemSnapshot` carries pressure, vacuum, duty, state, program list and BLE state
as values, and `UiController` returns an intent instead of calling the control
loop. That struct already exists as `DisplayUI::Model`.

`DECISION:` includes are written as `#include "hal/Pump.h"`, i.e. from the library
`src/` root. This depends on `arduino-cli` adding every subfolder of a library's
`src/` to the include path — **phase 0 proves it** (section 7, R1).

`scripts/check-layers.ps1` parses every `#include "…"` and fails on an upward
include. It is wired into the build task so a rotted graph cannot compile.

### 3.2 Why the script host is not a service

`ScriptRuntime`, `ScriptApi` and the concrete runtimes hold no vacuum state,
nothing in the main loop depends on them, and no policy decision lives in them.
They are an **adapter at the machine boundary**: a way for foreign code (Berry
today, possibly a macro recorder or a rule engine later) to drive the machine. That
puts them in the same family as the BLE transport — one speaks GATT, the other
speaks Berry — and not in the same family as `VacuumControl` or `ProgramStore`.

The dependency graph says the same thing:

- **inbound**: `CommandRouter` (`run`, `stop`, `eval`) and `App` (autostart, cancel)
- **outbound**: `ScriptApi` -> `Pump`, `PressureSensor`, `Display`, `Buttons`,
  `VacuumControl`

So the host sits *above* `services/` (`VacuumControl` is a service) and *below*
whatever issues commands. That is precisely the position that would make
`services/ -> io/ -> services/` a folder-level cycle if the command grammar stayed
in `services/`, even though the classes themselves stay acyclic. Moving the grammar
with the host is what keeps the rule mechanical.

`DECISION:` the three outside interfaces are grouped as `io/`: `io/commands/`,
`io/ble/`, `io/script/`. A future serial console lands as `io/serial/` without
touching anything else.

`DECISION:` `ScriptRuntime` stays an abstract class — the one vtable in the
project. It is what keeps Berry headers, which are vendored and macro heavy, out of
`io/commands/CommandRouter.h`. The alternative, a single class selected at compile
time with `#if EGGPLAN_WITH_BERRY`, would drag `be_vm*` into the router's include
chain and force `NullRuntime` out of the build entirely; not worth it for one
vtable.

The vendored VM source stays a separate Arduino library (`libraries/Berry/`) so its
licence and build flags remain independent of ours; `io/script/berry/` holds only
our glue.

## 4. Replacing `AppContext`

**Rule: every class takes exactly what it uses, by reference, in its constructor.**
A missing dependency becomes a compile error.

### 4.1 Composition root

```cpp
// app/App.cpp — the only file that knows the graph
Storage storage;                                               // hal
PressureSensor sensor{Wire, Pins::MPRLS_I2C_ADDR};
Pump pump{Pins::PUMP_PWM, Pins::PUMP_PWM_FREQ_HZ, Pins::PUMP_PWM_RESOLUTION};
Buttons buttons{Pins::BTN_UP, Pins::BTN_DOWN, Pins::BTN_OK, Pins::BTN_BACK};
Display display{Wire, Pins::OLED_I2C_ADDR, Pins::OLED_WIDTH, Pins::OLED_HEIGHT};
ProgramStore store{storage};
VacuumControl control{pump, sensor, Config::PID_KP, Config::PID_KI, Config::PID_KD};
ScriptApi api{pump, sensor, display, buttons, control};
NullRuntime runtime{api};
Renderer renderer{display};
UiController ui{renderer, buttons};
SnapshotBuilder snapshots{pump, sensor, control, store};    // services
TelemetrySettings telemetry{};                              // core; io/commands writes, io/ble reads
CommandRouter router{store, runtime, control, pump, sensor, telemetry};
BleService ble{router, telemetry, snapshots};               // io/*
```

`UiController` is constructed without `ProgramStore`: the program list reaches it
through `SystemSnapshot`, which is what keeps `ui/` out of `services/`. For the same
reason `SnapshotBuilder` does not take the runtime: it would be a `services/` ->
`io/` include. `App` fills the two script fields (`scriptRunning`, `program`) after
building the snapshot, since it is the only place allowed to see both layers.

### 4.2 Where each `AppContext` field goes

| `AppContext` field | New owner and consumers |
| --- | --- |
| `pump` | `hal/Pump`, injected into `VacuumControl`, `ScriptApi`, `CommandRouter`, `SnapshotBuilder` |
| `sensor` | `hal/PressureSensor`, injected into `VacuumControl`, `ScriptApi`, `CommandRouter`, `SnapshotBuilder` |
| `buttons` | `hal/Buttons`, injected into `ui/UiController`, `io/script/ScriptApi` |
| `ui` | `hal/Display` injected into `ui/Renderer`; `io/script/ScriptApi` gets `Display&` for the overlay |
| `store` | `services/ProgramStore`, injected into `CommandRouter`, `SnapshotBuilder` |
| `control` | `services/VacuumControl`, injected into `ScriptApi`, `CommandRouter`, `SnapshotBuilder` |
| `runtime` | `io/script/ScriptRuntime`, injected into `CommandRouter`; `App` reads it for the snapshot |
| `api` | `io/script/ScriptApi`, owned by `App`, injected into the runtime |
| `ble` | `io/ble/BleService`; **nothing points back at it** (section 4.3) |

`AppContext.h` is deleted in phase 7.

### 4.3 The command router / BLE coupling

Today `CommandRouter` calls `ctx_.ble->setTelemetry()` and `ctx_.ble->connected()`.

`DECISION:` replace the back-pointer with shared plain data:

```cpp
struct TelemetrySettings {          // core/Snapshot.h
  bool enabled = false;
  uint32_t periodMs = cfg::TELEMETRY_DEFAULT_MS;
};
```

- `CommandRouter` **writes** `TelemetrySettings&`; it reads `const SystemSnapshot&`
  (which carries `bleConnected`) for its `status`/`info` JSON.
- `BleService` **reads** both and owns the send loop.

No cycle, no header coupling, no vtable, and the dependency direction stays
`BleService -> CommandRouter`. Ownership is also acyclic: `App` owns the router,
the settings and the service; the service holds the router by reference.

Fallback if you prefer it: forward-declare `class BleService;` in
`CommandRouter.h` and hold a `BleService*` set by `App`. Same file-level cycle
that exists today, just acknowledged. Section 4.3 as written is strictly less
coupling at the same cost.

## 5. Current file → new file map

| Today | Becomes |
| --- | --- |
| `AppContext.h` | **deleted** |
| `App.h/.cpp` | `app/App.h/.cpp` (+ `services/SnapshotBuilder`) |
| `VacuumConfig.h` | split: `core/Config.h` (policy) + `hal/Pins.h` (wiring) — see Q2 |
| `Util.h` | `core/Units.h/.cpp` + `core/Crc32.h/.cpp` |
| `Log.h` | `core/Log.h/.cpp`, sink installed by `App` |
| `Pump.h/.cpp` | `hal/Pump.h/.cpp` (+ new `hal/Tachometer.h/.cpp`) |
| `PressureSensor.h/.cpp` | `hal/PressureSensor.h/.cpp` |
| `DisplayUI.h/.cpp` | split: `hal/Display.*` (draw + flush), `ui/Renderer.*`, `ui/pages/*.cpp`, `ui/UiController.*` |
| `Buttons.h/.cpp` | `hal/Buttons.h/.cpp` |
| `ProgramStore.h/.cpp` | `services/ProgramStore.h/.cpp` over new `hal/Storage.h/.cpp` |
| `Protocol.h/.cpp` | split: `io/commands/CommandRouter.*`, `services/ProgramUploader.*`, `core/Telemetry.*`, `core/UploadFrames.*` |
| `VacuumController.h/.cpp` | `services/VacuumControl.h/.cpp` + `core/Pid.h/.cpp` |
| `ScriptRuntime.h/.cpp` | split: `io/script/ScriptRuntime.h`, `io/script/ScriptApi.h/.cpp`, `io/script/NullRuntime.h/.cpp` (+ `io/script/berry/` later) |
| `BleService.h/.cpp` | `io/ble/BleService.h/.cpp`, telemetry send loop folded in |
| `DisplayUI::Model` | `core/Snapshot.h` → `SystemSnapshot` |

## 6. Phases

Each phase compiles green and is committed on its own, so any phase can be
reverted independently.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 0 | Probe + guardrail | prove subfolder include paths in the Arduino build; add `scripts/check-layers.ps1`; baseline `arduino-cli compile` | green |
| 1 | Folders | `git mv` into the tree above, include fixes only, no edits | green, zero behaviour change |
| 2 | `core/` | split `Config.h`/`Pins.h`; extract `Units`, `Crc32`, `Log` (sink, no `Serial`), `Pid`, `UploadFrames`, `Telemetry` | green |
| 3 | `hal/` | drivers become self-contained classes with constructor config; `Tachometer` split out; `Display` reduced to primitives + flush | green |
| 4 | `services/` | `Snapshot` + `SnapshotBuilder`; `VacuumControl`, `ProgramStore`, `ProgramUploader` take concrete references; `DisplayUI::Model` → `SystemSnapshot` | green |
| 5 | `io/` | `CommandRouter` out of `Protocol.cpp`; `Telemetry` JSON to `core/`; `ScriptApi` + `ScriptRuntime` + `NullRuntime`; `BleService` takes router + settings + snapshots with telemetry folded in | green, BLE commands byte-identical |
| 6 | `ui/` | `Renderer` + per-page files + `UiController` intents; `App` executes intents | green, same pages and buttons |
| 7 | Cleanup + suction fix | delete `AppContext.h`; section 8 changes; README layout and dependency table | green + docs match |

### 6.1 Definition of done

- [ ] `AppContext.h` no longer exists and nothing is named `ctx_`
- [ ] `scripts/check-layers.ps1` passes and runs in the build task
- [ ] `arduino-cli compile --fqbn esp32:esp32:esp32c3` green with no new warnings
- [ ] every class header names its collaborators; no nullable design pointers
- [ ] pins, BLE protocol and UI are unchanged from today
- [ ] README structure table matches the tree

### 6.2 Reduced scope

If this is more churn than you want right now, **phases 1 and 7 alone** (folders +
delete `AppContext` + the suction fix) capture most of the benefit; phases 2–6 are
the part that buys the clean layering. The plan is ordered so that stopping after
any phase leaves a coherent tree.

## 7. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | `arduino-cli` may not add library `src/` subfolders to the include path | phase 0 probe before any file moves; fallback is quoted relative includes (`#include "../hal/Pump.h"`) |
| R2 | Mechanical include churn across 21 files | one phase per commit, compile after each, `git mv` to preserve history |
| R3 | Splitting `DisplayUI` risks visual regressions | move code unchanged, edits only where a signature changes; review diff for edits-as-opposed-to-moves |
| R4 | `AppContext` removal touches every module's signature | no port layer to introduce; each phase hands modules the same references they already use internally |
| R5 | `Storage` indirection changes LittleFS error paths | keep mount/format calls byte-identical in phase 3, only relocate them |

## 8. Pump suction semantics (phase 7)

Measured pump capability: **-60 kPa**, i.e. 60 kPa of suction *below ambient*.
Today's config treats 60 as `WORKING_RANGE_MAX_KPA`, a ceiling on target pressure —
the wrong meaning, and it produces a bad default: at sea level the reachable
absolute floor is `101.3 - 60 = 41.3 kPa`, so the shipped `hold_vacuum.be` target of
40 kPa is unreachable and would pin duty at the cap forever.

```cpp
// core/Config.h
constexpr float PUMP_MAX_SUCTION_KPA = 60.0f;   // capability, ambient relative
constexpr float CONTROL_MIN_TARGET_KPA = 5.0f;  // hard sanity floor

// services/VacuumControl
float minReachableKpa() const {   // = max(CONTROL_MIN_TARGET_KPA, ambient - PUMP_MAX_SUCTION_KPA)
```

Changes:

- delete `WORKING_RANGE_MAX_KPA`
- `setTarget()` warns only when the target is below the reachable floor, and says
  what the floor is; it does not reject
- `status` JSON gains `floor_kpa`
- SETPOINT page shows a `> pump` hint when the target is unreachable
- `programs/hold_vacuum.be` moves to 50 kPa with a comment explaining the floor

## 9. Open questions

- **Q1** — keep `core/Clock.h` (`nowMs()` with a fakeable tick source) or let
  modules call `millis()` directly? Recommendation: keep it; it is one small file
  and it removes wall-clock access from `core/`.
- **Q2** — do the pump's duty cap, ramp and max-run-time limits belong in
  `core/Config.h` as safety policy, or in `hal/Pump` as motor characteristics?
  Recommendation: `core/Config.h`, because the control loop is what must respect
  them.
- **Q3** — rename `DisplayUI` to `Display` (`hal/Display` + `ui/Renderer`)?
  Recommendation: yes.
- **Q4** — `SnapshotBuilder` as a class or a free function
  `buildSnapshot(pump, sensor, control, store, runtime)`? Recommendation: free
  function, it holds no state.
- **Q5** — confirm the `UiIntent` set:
  `None, Stop, CyclePage, MoveCursor, AdjustTarget, ToggleHold, RunSelected`.
- **Q6** — does the suction fix land in phase 7 of this refactor, or as its own
  commit *before* phase 1? Recommendation: phase 7, since `VacuumConfig.h` is
  being split there anyway.
- **Q7** — keep `ProgramUploader` (`services/`) and `CommandRouter`
  (`io/commands/`) separate, or fold the upload session into the router?
  Recommendation: separate — the BLE `prog` characteristic talks to the uploader,
  not to the router.
- **Q8** — should `services/` be split further? Recommendation: not yet, and
  revision 2 removed the script host from it, so the pressure is off.
- **Q9** — is `io/` the right name for the outside-interface layer, versus
  `hosts/`, `interfaces/` or `transport/`? And is grouping **all three** interfaces
  there right, or should only the script host move out of `services/` and the
  command grammar stay put — which reintroduces the `services/ -> io/ ->
  services/` folder cycle described in §3.2? Recommendation: keep the three
  together under `io/`.
