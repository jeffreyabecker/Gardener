# Refactor plan: layering, folders, and deleting `AppContext`

Status: **proposed** · revision 4 · owner: unassigned · scope: `libraries/VacuumCore` only

Revision 2: script hosting moved out of `services/` into the outside-interface layer,
because the runtime is no longer machine policy. See §3.2.

Revision 3: no shared util. `Util.h` is dissolved into per-owner files (§3.3), and the
plan gains a leak sweep for details that are visible too high up the stack (§3.4).

Revision 4: Q1–Q11 answered and folded in (§10). The layer is named `hosts/`, the
clock seam is gone, `SnapshotBuilder` is a free function, and JSON is dropped for
`key=value` replies (§9) — which deletes the encoder instead of relocating it.

Annotation guide: review questions are numbered **Q1…Q11** and resolved in §10 with the
decision taken; `DECISION:` marks calls made inside the plan itself. Edit this file in place and the plan will be
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
outside interfaces (`hosts/`) instead of `services/` — see §3.2.

### Non-goals

- No behaviour change in phases 0–6: same pins, same BLE protocol, same framing,
  same UI pages, same button mapping. The two exceptions are both in phase 7: the
  pump suction semantics (§8) and the reply encoding (§9).
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
hosts/      how the outside world drives it: command grammar, BLE, script host
ui/         local presentation: renders the snapshot, emits intents
app/        composition root: the only place that knows the whole graph
```

```
libraries/VacuumCore/src/
  VacuumCore.h              includes app/App.h only
  core/
    Config.h                policy tunables, leaf (includes nothing)
    Snapshot.h              SystemSnapshot, DeviceInfo, ProgramInfo, ScriptOverlay,
                            TelemetrySettings, UiIntent, ProgramChecksum
    Math.h                  clamp, and nothing else
    Pressure.h / .cpp       kPa conversions and pressure formatting
    Format.h                number -> string, shared by JSON and the OLED
    Log.h / Log.cpp         level + ring buffer, writes to an installed sink
    Pid.h / Pid.cpp         pure controller math
  hal/
    Pins.h                  GPIO map, I2C addresses, display geometry, PWM settings
    Device.h / Device.cpp   chip model, SDK version, free heap (ESP facts)
    Pump.h / Pump.cpp
    Tachometer.h / .cpp     optional, split out of Pump
    PressureSensor.h / .cpp
    Display.h / Display.cpp SH1106 + draw primitives + flush
    Buttons.h / Buttons.cpp
    Storage.h / Storage.cpp LittleFS mount / file handles
  services/
    ProgramStore.h / .cpp   programs, staged writes, autostart, checksum (private)
    ProgramUploader.h / .cpp frame grammar + upload session, checksum compare
    VacuumControl.h / .cpp  PID + safety state machine
    SnapshotBuilder.h / .cpp buildSnapshot(...): free functions, no state
    SnapshotText.h / .cpp    SystemSnapshot -> key=value status, info and rows
  hosts/
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
| `hosts/` | `core/`, `hal/`, `services/` |
| `ui/` | `core/`, `hal/` |
| `app/` | everything; nothing includes `app/` |

Within `hosts/` the subfolders are one-way as well: `ble/ -> commands/ -> script/`.
`script/` includes nothing else from `hosts/`, which is what keeps the host
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

**Rule: no `util`, `common`, `helpers` or `misc` folder or header.** A helper lives
with its single owner, in that owner's folder, and is private unless a second layer
legitimately needs it. When a second consumer does appear, the helper moves *down* a
layer in a commit of its own — that is the cheap direction, and the point where the
move is justified rather than guessed.

Corollary: **`core/` is visible to every layer**, so anything parked there is public
API whether or not that was the intent. Weight `core/` towards plain data and
domain-neutral math, and treat a new file there as a design decision rather than a
convenience. `clamp` stays because every layer may include `core/` and it carries no
domain knowledge; `crc32` does not, because only two files need it.

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
`services/ -> hosts/ -> services/` a folder-level cycle if the command grammar stayed
in `services/`, even though the classes themselves stay acyclic. Moving the grammar
with the host is what keeps the rule mechanical.

`DECISION:` the three outside interfaces are grouped as `hosts/` (Q9):
`hosts/commands/`, `hosts/ble/`, `hosts/script/`. A future serial console lands as
`hosts/serial/` without touching anything else.

`DECISION:` `ScriptRuntime` stays an abstract class — the one vtable in the
project. It is what keeps Berry headers, which are vendored and macro heavy, out of
`hosts/commands/CommandRouter.h`. The alternative, a single class selected at compile
time with `#if EGGPLAN_WITH_BERRY`, would drag `be_vm*` into the router's include
chain and force `NullRuntime` out of the build entirely; not worth it for one
vtable.

The vendored VM source stays a separate Arduino library (`libraries/Berry/`) so its
licence and build flags remain independent of ours; `hosts/script/berry/` holds only
our glue.

### 3.3 Where today's `Util.h` goes

`Util.h` is the leak in its purest form: five helpers with five different owners,
published to every layer. Each one moves to its owner.

| Today | Single owner | New home |
| --- | --- | --- |
| `clampf` | genuinely generic; no domain knowledge | `core/Math.h` |
| `crc32`, `crc32Update` | program transfer integrity | private in `services/ProgramStore.cpp` + `services/ProgramUploader.cpp` |
| `formatFloat` | number to string for JSON and the OLED | `core/Format.h` |
| `kpaToHpa`, `kpaToBar`, `kpaToPsi`, `kpaToInHg`, `kpaToMmHg` | the pressure domain | `core/Pressure.h/.cpp` |
| `jsonEscape` | JSON encoding | **deleted** — replies are `key=value` with token values, so there is nothing to escape (§9) |

Note what the checksum change actually buys: the client's CRC becomes an opaque
`ProgramChecksum` that the router unpacks and hands to the store without ever
knowing the polynomial, the initial value or the final XOR. Those live in two `.cpp`
files. The `docs/ble-protocol.md` specification stays the only other place the
algorithm is written down, because the host tool has to reproduce it — a protocol
document, not a header.

### 3.4 Leak sweep

Same lens applied to the rest of the plan. Each of these is a detail visible a layer
or two too high.

| Leak | Why it leaks | Fix |
| --- | --- | --- |
| `progproto::kBegin/kData/kEnd/kAbort` in `Protocol.h` | published in a header, but referenced only inside `Protocol.cpp`; `BleService` forwards raw bytes and never sees them | constants become file-private in `ProgramUploader.cpp` — a header is a contract, so keep the frame grammar out of it |
| `CommandRouter::status()` / `info()` read `ESP.getChipModel()`, `runtime->name()`, `store->totalBytes()` | the wire view reaching into four modules, so none of them can change alone | those fields move onto `SystemSnapshot`; `SnapshotText` is a pure function of the snapshot |
| `CommandRouter::emitList()` encodes program JSON with `jsonEscape` | a second encoder, so two layers share one that both must keep correct | `SnapshotText::programRows(snapshot)`; the router returns lines and encodes nothing |
| `Pump::faultReason()` (a human log string) reaching the wire | a log message becoming a machine contract | wire reports a token (`fault=max_run`); the readable string stays in the log |
| `ProgramStore::pathFor()` public | exposes the `/programs/<name>.be` layout to callers | private; `validName()` stays public because it is the name contract |
| `DisplayUI` overlay buffer (`overlayLines_[7]`) | script layout policy inside the display driver, and `ScriptApi` reaches into the driver to write it | `ScriptOverlay` plain data in `core/Snapshot.h`, owned by `App`, written by `ScriptApi`, drawn by `ui/Renderer` — the same pattern as `TelemetrySettings` |
| `Pump::tachIsr(void*)` public static | ISR plumbing is driver internals | private |
| `Pump::setDutyNow()` public | published escape hatch past the ramp | **no callers at all** (verified: declaration + definition only) — delete it; `emergencyStop()` writes zero directly |
| `Storage` exposing `fs::File` in its header | a port would hide it, but we chose no interfaces | **accepted exception**, documented in the header; `ProgramStore` is the only consumer |

`ScriptOverlay` and `DeviceInfo` joining `SystemSnapshot` is what makes the
unification work: one plain-data view of the machine, three consumers (`SnapshotText`,
`ui/Renderer`, the script host's overlay), and nothing outside `App` holding a module
pointer.

## 4. Replacing `AppContext`

**Rule: every class takes exactly what it uses, by reference, in its constructor.**
A missing dependency becomes a compile error.

### 4.1 Composition root

```cpp
// app/App.cpp — the only file that knows the graph

// construction, once
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
TelemetrySettings telemetry{};              // core; hosts/commands writes, hosts/ble reads
CommandRouter router{store, runtime, control, pump, sensor, telemetry};
BleService ble{router, telemetry};          // hosts/*

// every loop
SystemSnapshot snapshot = buildSnapshot(pump, sensor, control, store);  // free function
snapshot.scriptRunning = runtime.running();   // App fills the script fields
snapshot.program = runtime.currentProgram();
ui.render(snapshot);
ble.poll(now, snapshot);
```

`UiController` is constructed without `ProgramStore`: the program list reaches it
through `SystemSnapshot`, which is what keeps `ui/` out of `services/`. For the same
reason `buildSnapshot()` does not take the runtime: it would be a `services/` ->
`hosts/` include. `App` fills the two script fields after building, since it is the
only place allowed to see both layers. `BleService::poll()` receives the snapshot as a
parameter rather than holding the builder, so the transport owns no view state.

### 4.2 Where each `AppContext` field goes

| `AppContext` field | New owner and consumers |
| --- | --- |
| `pump` | `hal/Pump`, injected into `VacuumControl`, `ScriptApi`, `CommandRouter`, `SnapshotBuilder` |
| `sensor` | `hal/PressureSensor`, injected into `VacuumControl`, `ScriptApi`, `CommandRouter`, `SnapshotBuilder` |
| `buttons` | `hal/Buttons`, injected into `ui/UiController`, `hosts/script/ScriptApi` |
| `ui` | `hal/Display` injected into `ui/Renderer`; `hosts/script/ScriptApi` gets `Display&` for the overlay |
| `store` | `services/ProgramStore`, injected into `CommandRouter`, `SnapshotBuilder` |
| `control` | `services/VacuumControl`, injected into `ScriptApi`, `CommandRouter`, `SnapshotBuilder` |
| `runtime` | `hosts/script/ScriptRuntime`, injected into `CommandRouter`; `App` reads it for the snapshot |
| `api` | `hosts/script/ScriptApi`, owned by `App`, injected into the runtime |
| `ble` | `hosts/ble/BleService`; **nothing points back at it** (section 4.3) |

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
  (which carries `bleConnected`) to build its `status`/`info` replies.
- `BleService` **reads** the settings and takes the same snapshot as a poll
  parameter, so it owns the send loop without owning the builder.

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
| `App.h/.cpp` | `app/App.h/.cpp` (+ `services/SnapshotBuilder`, `hal/Device`) |
| `VacuumConfig.h` | split: `core/Config.h` (policy) + `hal/Pins.h` (wiring) — see Q2 |
| `Util.h` | split by owner: `core/Math.h` (clamp), `core/Pressure.*` (kPa), `core/Format.h` (numbers); `crc32` private to `services/`; `jsonEscape` deleted (§3.3, §9) |
| `Log.h` | `core/Log.h/.cpp`, sink installed by `App` |
| `Pump.h/.cpp` | `hal/Pump.h/.cpp` (+ new `hal/Tachometer.h/.cpp`) |
| `PressureSensor.h/.cpp` | `hal/PressureSensor.h/.cpp` |
| `DisplayUI.h/.cpp` | split: `hal/Display.*` (draw + flush), `ui/Renderer.*`, `ui/pages/*.cpp`, `ui/UiController.*`; the 7 line overlay buffer becomes `ScriptOverlay` data in `core/Snapshot.h` |
| `Buttons.h/.cpp` | `hal/Buttons.h/.cpp` |
| `ProgramStore.h/.cpp` | `services/ProgramStore.h/.cpp` over new `hal/Storage.h/.cpp` |
| `Protocol.h/.cpp` | split: `hosts/commands/CommandRouter.*`, `services/ProgramUploader.*` (owns the frame grammar), `services/SnapshotText.*` |
| `VacuumController.h/.cpp` | `services/VacuumControl.h/.cpp` + `core/Pid.h/.cpp` |
| `ScriptRuntime.h/.cpp` | split: `hosts/script/ScriptRuntime.h`, `hosts/script/ScriptApi.h/.cpp`, `hosts/script/NullRuntime.h/.cpp` (+ `hosts/script/berry/` later) |
| `BleService.h/.cpp` | `hosts/ble/BleService.h/.cpp`, telemetry send loop folded in, snapshot passed per poll |
| `DisplayUI::Model` | `core/Snapshot.h` → `SystemSnapshot` |

## 6. Phases

Each phase compiles green and is committed on its own, so any phase can be
reverted independently.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 0 | Probe + guardrail | prove subfolder include paths in the Arduino build; add `scripts/check-layers.ps1`; baseline `arduino-cli compile` | green |
| 1 | Folders | `git mv` into the tree above, include fixes only, no edits | green, zero behaviour change |
| 2 | `core/` | split `Config.h` / `Pins.h`; extract `Math`, `Pressure`, `Format`, `Log` (sink, no `Serial`), `Pid`; make the checksum private to `services/` | green |
| 3 | `hal/` | drivers become self-contained classes with constructor config; `Tachometer` split out; `Display` reduced to primitives + flush | green |
| 4 | `services/` | `Snapshot` + `buildSnapshot()` + `SnapshotText`; `VacuumControl`, `ProgramStore`, `ProgramUploader` take concrete references; `DisplayUI::Model` → `SystemSnapshot`; §3.4 leak sweep | green |
| 5 | `hosts/` | `CommandRouter` out of `Protocol.cpp`; `ScriptApi` + `ScriptRuntime` + `NullRuntime`; `BleService` takes router + settings, snapshot per poll, telemetry folded in | green, wire format unchanged |
| 6 | `ui/` | `Renderer` + per-page files + `UiController` intents; `App` executes intents | green, same pages and buttons |
| 7 | Cleanup + behaviour | delete `AppContext.h`; §8 suction semantics; §9 `key=value` replies and the host tool update; docs | green + docs match |

### 6.1 Definition of done

- [ ] `AppContext.h` no longer exists and nothing is named `ctx_`
- [ ] `scripts/check-layers.ps1` passes and runs in the build task
- [ ] `arduino-cli compile --fqbn esp32:esp32:esp32c3` green with no new warnings
- [ ] every class header names its collaborators; no nullable design pointers
- [ ] no `util`, `common` or `helpers` header or folder exists
- [ ] replies are `key=value` and no JSON encoder exists anywhere in the firmware
- [ ] no layer reaches into a module through a struct, snapshot or settings object it does not own
- [ ] every §3.4 leak sweep row is closed, or explicitly accepted with a note in the header
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
| R6 | dropping JSON is a wire change, so any existing client parsing `status` breaks | it is confined to phase 7 and lands with `docs/ble-protocol.md` and the README tables; the bundled CLI prints replies verbatim and needs only the `ok programs` prefix added |
| R7 | no clock seam means `millis()` is called from `core/`-adjacent code, so PID timing cannot be faked later | accepted (Q1); the PID math stays pure and takes `dt` as a parameter, so only the scheduler is untestable |

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
- `status` gains `floor=…` in kPa (§9)
- SETPOINT page shows a `> pump` hint when the target is unreachable
- `programs/hold_vacuum.be` moves to 50 kPa with a comment explaining the floor

## 9. Dropping JSON for key=value replies

Q10 asked where the JSON encoder should live. The better answer is that there is no
encoder: the only consumers of `status`, `info` and `list` are our own host tool and a
human with a BLE terminal, and neither needs JSON.

### 9.1 Format

One line of space separated `key=value` tokens, stable key order, values are space-free
tokens, and `-` means "not applicable".

    ok kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 fault=- up=123456
    tel kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 fault=- up=123456
    ok fw=0.1.0 chip=ESP32-C3 runtime=none progs=2 store_kb=1440 sensor=1 tach=0 heap=298412

Lists reuse the multi-line convention `cat` already uses: a header, one row per item,
then `eof`.

    -> list
    <- ok programs 2
    <- p hold_vacuum 137 0x1A2B3C4D auto
    <- p pulse 331 0xDEADBEEF -
    <- eof

`cat` and `log` keep their `ok file …` / `ok log` + body + `eof` shape.

Rule: a value that could contain a space is sanitised to `_` at the source. That is why
the pump fault reaches the wire as a token while the readable string stays in the log —
a log message was never a machine contract.

### 9.2 Why

- **No heap at 4 Hz.** `statusJson()` concatenates `String`s today, so every telemetry
  notification reallocates. `snprintf` into a fixed `char[192]` allocates nothing.
- **No escaping, so `jsonEscape` is deleted** rather than relocated — the leak §3.3
  flagged.
- **One producer.** `SnapshotText` encodes; the router returns lines and encodes
  nothing (§3.4).
- **Still machine readable.** `dict(t.split("=", 1) for t in line.split())`, and a host
  that wants JSON converts it in one line.
- **Readable in the middle.** Typing `status` into a BLE terminal shows something a
  human can read without a parser first.

### 9.3 What it costs

Nested and typed values. There are none today — the program list is the only nested
case and it is already line oriented, exactly like `cat`. A third party that wants JSON
gets it from the host tool instead of from the firmware.

### 9.4 Scheduled work (phase 7)

- `services/SnapshotText` replaces `SnapshotJson`; `jsonEscape` is deleted
- `info`, `status` and `list` change shape; `Pump::faultCode()` joins `faultReason()`
- `docs/ble-protocol.md` and the README field tables
- `tools/vacuum_cli.py` prints replies verbatim, so the only change needed is adding
  `ok programs` to `MULTILINE_PREFIXES` so `list` reassembles as a single reply

### 9.5 Proposed keys (see §10.1)

| Reply | Keys |
| --- | --- |
| `status`, `tel` | `kpa` `vac` `pct` `duty` `state` `target` `floor` `fault` `up` `ble` |
| `info` | `fw` `chip` `runtime` `progs` `store_kb` `sensor` `tach` `heap` |
| `list` row | `p <name> <size> <checksum> <auto\|->` |

## 10. Decisions

All eleven review questions are answered; the answers are folded into the plan above.

| # | Question | Decision |
| --- | --- | --- |
| Q1 | clock seam? | **No seam.** Modules call `millis()` directly and `hal/Clock.h` is dropped (§3, R7). |
| Q2 | duty cap, ramp and max run time? | `core/Config.h`, as safety policy the control loop must respect. |
| Q3 | rename `DisplayUI`? | Yes: `hal/Display` (draw + flush) + `ui/Renderer` + `ui/UiController`. |
| Q4 | `SnapshotBuilder` class or free function? | Free functions: `buildSnapshot(...)`. |
| Q5 | `UiIntent` set? | As proposed: `None, Stop, CyclePage, MoveCursor, AdjustTarget, ToggleHold, RunSelected`. |
| Q6 | suction fix timing? | Phase 7, alongside the `VacuumConfig.h` split. |
| Q7 | uploader vs router? | Separate: `services/ProgramUploader` and `hosts/commands/CommandRouter`. |
| Q8 | split `services/` further? | Not yet. |
| Q9 | layer name? | `hosts/`, with all three outside interfaces grouped there. |
| Q10 | where does the JSON encoder live? | Nowhere — JSON is dropped for `key=value` replies (§9), so the encoder and `jsonEscape` are deleted rather than placed. |
| Q11 | `ScriptOverlay` location? | In `core/Snapshot.h`. |

### 10.1 Still open

- the §9.5 key names, to be confirmed before phase 7 changes the wire
- the fault token set: only the run time limit exists today, so the proposal is
  `fault=max_run|sensor|none`; any fault path added to `Pump` needs a token, not a log
  string
