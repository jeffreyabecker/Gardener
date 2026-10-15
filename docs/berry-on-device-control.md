# On-device control with Berry

Status: plan, not yet implemented.

## 1. Goal

Bring on-device control back to Gardener: store Berry **programs** on the device, run one
at a time from a local button UI or the serial line, and keep it running when the host goes
away. The device stays a serial peripheral when a host is attached; it becomes a standalone
instrument when one is not.

This replaces the current model where every decision comes from the controller and the
device has no local control surface ([README.md](../README.md)).

Terminology: a **program** is a stored Berry program. `default` is the magic program the
device boots into.

## 2. Constraints this plan must satisfy

1. Berry programs run on a **different core** from the serial link.
2. Programs take **parameters**, and setting them has a sensible UX on both the wire and
   the buttons.
3. The **buttons come back**, and there is a **program browser**.
4. **Disconnected operation**: a running program survives the host leaving.
5. The **system snapshot is extensible** so programs can read the machine and publish their
   own state.
6. The **Display instance** must be exposed to programs safely.

## 3. Headline decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Board | **ESP32-S3** (`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`) | Done. Dual-core **and** native USB via the USB-Serial-JTAG CDC on GPIO19/20, which keeps `(bool)Serial` host-presence detection and the ROM downloader on the same cable. |
| Core split | Plant and serial link on core 0, Berry VM on core 1 | Plant and serial are both short, bounded work; the VM is the only unbounded work and gets core 1 to itself. |
| Plant ownership | Sensor, pump, valve, battery, display and buttons stay on **one** task, on core 0 with the serial link | The control loop is the safety-critical code and must stay off the VM's core so a runaway program cannot touch it. |
| Display to programs | Programs get a **ProgramScreen** overlay, never the `Display` instance | `Display` is non-thread-safe I2C plus a framebuffer; handing it across cores invites races and lets a program clobber the battery overlay. |
| Boot behavior | The reserved program `default` is always present, and is loaded **and started** at boot | The device always comes up doing something; that something is the home screen and the headless fallback. |
| Run model | One program at a time; the plant is the single owner; switching programs always vents first | Keeps one writer to the pump and valve, and every program change starts from a released chamber. |
| Parameter model | Manifest-declared typed params; "last run" values persisted; running is easy, editing is deliberate | See sections 6 and 8. |

## 4. Board (done)

Phase 0 has landed. The device now builds for the ESP32-S3.

- FQBN `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`, pinned in
  [sketch.yaml](../firmware/gardener/sketch.yaml), [arduino.ps1](../scripts/arduino.ps1)
  and `.vscode/arduino.json`. `USBMode=hwcdc` routes USB to the USB-Serial-JTAG CDC on
  GPIO19/20, so `(bool)Serial` is a real host-presence signal and uploads share the same
  cable; `CDCOnBoot=cdc` maps `Serial` to it.
- Pins settled in [Pins.h](../libraries/VacuumCore/src/hal/Pins.h):

| Signal | S3 GPIO |
| --- | --- |
| I2C SDA / SCL | 8 / 9 |
| Pump PWM | 6 |
| Vent valve | 5 |
| Battery ADC | 4 |
| BTN_UP / DOWN / OK / BACK | 1 / 2 / 10 / 11 |

The strapping pins (0, 3, 45, 46) and the USB pins (19/20) are all avoided, and the
existing serial protocol runs unchanged on the S3.

## 5. Task architecture

Replace the single-threaded `loop()` model with three FreeRTOS tasks created in
`App::begin()`. The sketch's `loop()` becomes `vTaskDelete(NULL)`.

| Task | Core | Priority | Owns | Communicates via |
| --- | --- | --- | --- | --- |
| **Plant** | 0 | 10 (highest on core 0) | `VacuumSystem`, `PressureSensor`, `Pump`, `VentValve`, `Battery`, the display framebuffer + `AppDisplay` flush, the `Buttons` scan, local UI render, snapshot publication | command queue (consumer), snapshot slot (publisher), button-event queue, log queue |
| **Serial link** | 0 | 6 | `SerialLink` grammar, the stream (replies + diagnostics), `logEnabled_` | command queue (producer), snapshot slot (reader), log queue (consumer) |
| **Berry VM** | 1 | 3 (alone on core 1) | Berry VM, `ProgramApi` | command queue (producer), snapshot slot (reader), program-overlay slot (writer), button-event queue (consumer) |

The USB-Serial-JTAG CDC (`hwcdc`) is a hardware peripheral with no FreeRTOS task of its
own, so the serial task is the only consumer of `Serial` and can be pinned to core 0
directly.

Core 0 carries the two bounded jobs: the plant tick (a 50 ms control step plus the
display and button servicing) and the serial task (at most a handful of parsed lines per
pass). Neither can hold the core for long, so they share it comfortably. Core 1 carries
the only unbounded job, the Berry VM.

### 5.1 Thread-safety rules

- Exactly one owner per piece of hardware. Nothing else touches I2C, LEDC, GPIO or the
  display buffer.
- All cross-task state moves through FreeRTOS queues, or through two small "latest value"
  slots (snapshot, program overlay) protected by a `portMUX` critical section.
- The serial stream has one writer: the serial task. Plant diagnostics go through a
  **log queue** to it, so replies and `[I]/[W]/[E]` records stay ordered and
  `set log off` stays race-free.

### 5.2 Command flow

- The serial task parses and validates every line. `info` / `status` / `help` are answered
  locally from the snapshot slot. `draw` / `flush` ops are validated locally and enqueued
  fire-and-forget (they cannot fail after validation), replying `ok drawn` / `ok flushed`
  immediately.
- Actuator commands that can fail (`start`, `stop`, `reset`, `set target`, `ambient`,
  `run`, `set params`) use a small **request/response mailbox**: the serial task enqueues
  with a reply token and blocks (short timeout) for the plant's `ok` / `err`. This
  preserves the current one-reply-per-command rule and the exact `err not_ready` semantics.
- The VM task writes commands fire-and-forget; `ProgramApi` reads are served from a local
  copy of the latest snapshot. Programs poll `vacuum.state()` instead of blocking on plant
  RPC.

### 5.3 Runaway-program protection

- The plant runs on core 0, so a CPU-bound Berry loop on core 1 cannot delay the control
  loop at all — the plant's 50 ms tick is immune to the VM by construction.
- The VM task updates a heartbeat each time it yields (`vacuum.delay` and the Berry
  execution-limit hook). If the plant sees the heartbeat go stale (~2 s), it sets a
  stop-request flag and, if needed, restarts the VM task. The exact Berry hook name
  (execution/instruction budget) is confirmed during implementation; Tasmota's Berry port
  is the reference.

## 6. The program model

### 6.1 Manifest and parameters

Every program may declare typed parameters in a comment header at the top of the file:

```berry
# @param target number 40.0 min=5 max=150 step=0.5 unit=kPa "Target absolute pressure"
# @param cycles int 10 min=1 max=100 "Pulse cycles"
# @param mode choice hold choices=hold|pulse "Run mode"
# @param beep bool false "Beep on fault"
```

| Type | Editor control | Extra manifest keys |
| --- | --- | --- |
| `number` | Up/Down steps, repeat = coarse (×10) | `min`, `max`, `step`, `unit` |
| `int` | Up/Down steps, repeat = coarse | `min`, `max` |
| `choice` | Up/Down cycles the options | `choices=a|b|c` |
| `bool` | Up/Down toggles | — |

The header runs from the first line to the first non-comment line. `ProgramParams` parses
it into `ProgramInfo.params[]`; the Berry `program.param(name)` accessor returns the typed
value at run time. `choice` and `bool` are the recommended types for anything that will be
tuned without a host, because they need no numeric stepping.

### 6.2 Store and persistence

`ProgramStore` keeps programs in LittleFS as `/programs/<name>.be`, exactly as the old
`f35a3bb` store did: writes are staged into a temp file and renamed on commit, names are
restricted to `[A-Za-z0-9_-]`, and the store is refreshed into `ProgramInfo[]` on boot and
after any change.

Per-program parameter values live in `/programs/.params/<name>` as `key=value` lines. They
are the **last run** values. The merge is deterministic:

- start from the manifest defaults,
- overlay persisted values (persisted wins),
- drop persisted keys the manifest no longer declares (program was re-uploaded),
- coerce and range-check every value against the manifest; an invalid persisted value
  falls back to its default.

### 6.3 The magic default program

- `default` is a reserved name. The store guarantees it exists: if `/programs/default.be`
  is missing on boot (deleted, or first boot), the firmware re-seeds the built-in source.
- At boot the device **loads and starts `default`** — no host required. This is the
  headless fallback and the home screen.
- The seeded `default` is passive: it draws the live readout (pressure, state, duty,
  battery) and never touches the pump or valve, so boot stays safe (pump off, valve open)
  by construction.
- An owner can replace `default` by uploading a program of that name; that program then
  becomes the boot behavior. `rm default` restores the built-in passive source, so the
  device can never be left without a home program.
- Everything else about `default` is ordinary: it appears in the browser (pinned first,
  marked `*`), can take parameters, and runs like any other program.

## 7. On-device UX

Four buttons, active-low with internal pull-ups, debounced into three event kinds by
`hal/Buttons`: **click** (< 500 ms), **long-press** (≥ 800 ms), and **repeat** (every
~200 ms while held after the long-press).

There are exactly three screens:

| Screen | When | Purpose |
| --- | --- | --- |
| **RUN** | a program is executing | the program's `ProgramScreen`, framed by device chrome |
| **BROWSER** | selector open, nothing running | pick a program to run or edit |
| **PARAMS** | editor open | step through and change one program's parameters |

There is no separate status page: the seeded `default` **is** the status screen.

### 7.1 Button grammar

Two gestures are global and belong to the device no matter what a program is doing:

- **Back click** — *go home*. Stop the running program (if any), vent, then load and
  start `default`.
- **Back long-press** — *open the selector*. Stop the running program (if any), vent,
  then open BROWSER with the cursor on `default`.

Everything else is context-dependent:

| Gesture | RUN (program executing) | BROWSER | PARAMS |
| --- | --- | --- | --- |
| Up / Down click | to the program if it owns input | move cursor (wraps) | step value by one step |
| Up / Down repeat | to the program if it owns input | fast scroll | coarse step (×10) |
| OK click | to the program if it owns input | **run the selected program** | next parameter (wraps) |
| OK long-press | to the program if it owns input | **edit the selected program's params** | **persist params and run** |

Notes:

- **Running is one press.** In BROWSER, OK click runs the selected program with its last
  used (or default) parameters.
- **Editing is deliberately clunkier.** Reaching PARAMS is an extra long-press, each
  parameter is one screen, and changes only take effect when you run.
- **Back can never be taken by a program.** A program that claims input gets Up, Down and
  OK; Back always escapes, so no program can trap the user.

### 7.2 Break-out paths

From any running program:

- **Vent and return to the default program** — **Back click**. The current program is
  stopped, the chamber vents, and `default` is loaded and started. One press gets back to
  the known-good home screen.
- **Vent and initialize the program selector** — **Back long-press**. The current program
  is stopped, the chamber vents, and BROWSER opens with nothing running and the cursor on
  `default`, ready to pick the next program.

Because every program change vents first, both paths leave the machine in the released
state before anything new can act.

### 7.3 Program browser, in detail

128x64 layout:

```
PROGRAMS 3/12
* default
  pulse
> hold
  cycle
OK run  OK^ edit  BACK home
```

- **Header** — title plus cursor index / total.
- **List** — programs sorted alphabetically with `default` pinned first. Each row is the
  name (truncated to fit). Markers: `*` on `default`, `>` on the cursor, a `R` suffix on
  the program currently running (only reachable while running if browsing is entered
  non-destructively; with the grammar above BROWSER is always entered post-vent, so `R`
  is the common case when returning from PARAMS of a running program).
- **Cursor** — the selected row is inverted (paper-on-ink), so the selection is readable at
  arm's length. The list scrolls with the cursor and wraps at both ends; Up/Down repeat
  fast-scrolls.
- **Footer** — hints for the three available actions.

Interactions:

- OK click runs the selected program and returns to RUN.
- OK long-press opens PARAMS for the selected program.
- Back click leaves the browser and homes to `default`.

### 7.4 Parameter editor, in detail

One parameter per screen, stepped with OK:

```
TARGET 2/3
Target absolute pressure
  40.0 kPa
min 5  max 150  step 0.5
OK next  OK^ run  BACK back
```

- **Header** — parameter name (uppercased) and index/total.
- **Description** — the manifest's `"…"` text, truncated to one line.
- **Value** — large (size 2) and inverted while it is the active field; the unit from
  `unit=` is appended.
- **Range** — `min`/`max`/`step`, or the choices for a `choice` param, or on/off for a
  `bool`.
- **Footer** — hints.

Interactions:

- Up/Down click steps the value; repeat steps by ×10 (numeric) or just cycles (choice,
  bool).
- OK click moves to the next parameter, wrapping to the first after the last.
- OK long-press persists the values to `/programs/.params/<name>` and starts the program.
- Back click discards edits and returns to BROWSER.

A program with no declared parameters skips PARAMS: OK long-press in BROWSER shows
"no params", and OK click runs it directly.

### 7.5 RUN screen

The device draws the chrome; the program draws the middle:

```
pulse             HOLD
cycle 3/10
  41.3 kPa
[███████████-----]
BACK stop  BACK^ browser
```

- **Header** — program name (device-drawn) plus the live run state.
- **Program area** — the program's `ProgramScreen`: up to five text lines
  (`vacuum.screen_line(row, text)`), an optional bar (`vacuum.screen_bar(fraction)`), and
  toasts (`vacuum.toast(text, ms)`).
- **Battery** — composited on every flush by `AppDisplay`, as today; programs cannot erase
  it.
- **Footer** — the two escape hints, always present, because Back is always the way out.

### 7.6 Input ownership

A program may claim button input by calling `vacuum.button_wait(...)` / `vacuum.button_down(...)`.
While claimed, Up, Down and OK are delivered to it as events; Back remains device-owned.
A program that never claims input leaves Up/Down/OK as no-ops, and its screen is purely a
readout. The seeded `default` is exactly such a readout.

### 7.7 Host-attached vs local

- **No host** → the local UI owns the panel (RUN / BROWSER / PARAMS as above).
- **Host attaches** → the panel becomes host-drawn exactly as today; local rendering
  suspends. A program launched over the wire still runs on the VM core, but its screen is
  not shown — the host draws the panel and polls `status`.
- **Host detaches** → the panel returns to the local UI, showing the RUN screen of whatever
  is running (or `default` after the home gesture).
- Buttons pressed while the host is attached are ignored, so the local UI never fights the
  host for the glass; the host remains authoritative while present.

## 8. Disconnected operation and parameterized programs

### 8.1 What "disconnected" means now

The link-loss rule changes from "always stop and vent" to:

- **A running program is the local owner.** Host loss does not stop it; the program keeps
  running. It sees `vacuum.host_present()` go false and may react, but the default is
  simply to carry on.
- **No program running** → stop and vent as today (safety default).
- `status` exposes `host=0|1`, `program=<name|->` and `running=0|1` so a reattached host
  can see what happened while it was away and take over (`run` / `stop` both work).

Safety stays intact regardless of the owner: the vent valve remains normally-open
fail-safe, and the fault latch plus sensor-loss trip still act with no owner in the loop.

### 8.2 Running is easy, editing is deliberate

The split is explicit in the button grammar:

- **Run with last parameters** = BROWSER → OK click. One press, nothing to retype.
- **Run with different parameters** = BROWSER → OK long → step through PARAMS → OK long.
  Multi-step by design.

Persisted "last run" values are what make disconnected use easy: tune a program once (or
over the wire), and every later disconnected run reuses those values without re-entering
them.

### 8.3 A disconnected walkthrough

1. Device boots unplugged → `default` starts and shows the live readout.
2. Back long-press → current program stops, chamber vents, BROWSER opens.
3. Up/Down to `pulse`, OK long-press → PARAMS.
4. OK to reach `cycles`, Up/Down (repeat for coarse) to set 5.
5. OK long-press → params persist, `pulse` runs with `cycles=5`.
6. Back click → `pulse` stops, chamber vents, `default` runs again.
7. Power-cycle → `default` runs again, because `default` is the boot program.

If the user wants `pulse`-with-5-cycles at every boot, they overwrite `default` with that
source (or set `default`'s own params), not a separate autostart knob.

### 8.4 Persistence and merge rules

- Persisted per-program values are written when a program is run from PARAMS or via
  `set params <name> …`. They survive reboot in LittleFS.
- On start, effective params = manifest defaults overlaid with persisted values, as in
  section 6.2. A re-uploaded program with a changed manifest loses obsolete keys and fills
  missing ones with defaults — never a crash, never a silent type mismatch.
- Parameters are **captured at start** and do not change mid-run. Changing them means
  breaking out and re-running, which is the deliberate "clunkier" path.

### 8.5 Edge cases

- **Zero-parameter program** — BROWSER → OK click runs it directly; there is nothing to
  edit.
- **Store has only `default`** — the browser lists one row; Back click and OK click both
  effectively keep the device home.
- **`default` deleted** — the store re-seeds the passive source on next refresh; the device
  is never without a boot program.
- **Host attached while a program runs** — the host can `stop`, `run` another program, or
  draw its own panel; the local buttons are ignored until it leaves.
- **Program consumes every button it can** — Back click and Back long always escape, so the
  user can always vent and get home or to the selector.

## 9. Snapshot extensibility for programs

`SystemSnapshot` stays plain, copyable data (it is the cross-task boundary) and grows
additively:

- `ProgramStatus` sub-struct: `running`, `programName`, `error` / last error,
  `hostAttached`.
- `ProgramInfo[]`: name, size, params summary, running/default flags (bounded, e.g. 24),
  published by `ProgramStore::refresh()`.
- `ProgramOverlay` value: program screen lines, bar fraction, toast + expiry.
- A fixed-capacity **program state table** a program can write and read:
  `program.state_set("label", 42.5)` / `program.state_get("label")`. These keys are
  appended to the `status` payload (`s0=label s0v=42.5 …`) and available to the program's
  own screen, so programs publish their own telemetry without any C++ change.
- The whole snapshot is exposed read-only to Berry as typed accessors (`vacuum.kpa()`,
  `vacuum.state()`, `vacuum.duty()`, …) served from the VM task's local copy.

## 10. Exposing the Display instance to programs

- Do **not** hand `Display` to the VM task. The instance stays on the plant task.
- Programs get the **ProgramScreen**:
  - `vacuum.screen_clear()`
  - `vacuum.screen_line(row, text)` (five rows)
  - `vacuum.screen_bar(fraction)`
  - `vacuum.toast(text, ms)`
- The plant task composites the ProgramScreen between the device-drawn header and footer;
  `AppDisplay` puts the battery on last. The overlay travels through the program-overlay
  slot as a small value struct, never as a pointer.
- Stretch goal, if arbitrary drawing is ever needed: route the existing `draw` op set
  through the same command mailbox the serial link uses, so the plant task serializes all
  drawing. The bar + lines + toast cover the on-device readout use case, so this is
  optional.

## 11. Serial protocol changes

New program commands (additive; existing commands keep their meaning):

| Command | Reply | Notes |
| --- | --- | --- |
| `list` | `ok programs=N` then one row per program, then `eof` | rows carry name, size, param count, `default`/`running` flags |
| `cat <name>` | paged source | as the old `cat` |
| `rm <name>` | `ok removed <name>` | `rm default` re-seeds the passive source |
| `run <name> [k=v …]` | `ok running <name>` / `err …` | stop current program → vent → start; named params validated against the manifest |
| `stop` | `ok stopped` | stop current program/run → vent → **idle**. Does not auto-start `default` while a host is attached, so the host is never surprised |
| `params <name>` | `ok params=<name> k=v …` | effective values (persisted over defaults) |
| `set params <name> k=v …` | `ok params=<name> k=v …` | persist without running |
| `prog begin <name>` / `prog append …` / `prog commit <size> <crc32>` / `prog abort` | staged, chunked upload | CRC-32, staged temp + rename on commit — the old `ProgramUploader` scheme over the line |

`status` gains `program=<name|->`, `running=0|1`, `host=0|1`, and the program state table
(`s0=key s0v=val …`). `help` lists the new commands.

Local Back click and serial `stop` differ on purpose: Back click always homes to `default`
(the device's own home), while serial `stop` leaves the device idle (the host is present
and decides what happens next).

## 12. Layer layout

Re-introduce the five-layer layout the project used before the serial simplification, and
extend [check-layers.ps1](../scripts/check-layers.ps1) accordingly — otherwise `app/`
absorbs a UI, a VM and a file store.

```
core/     Config.h, Snapshot.h (extended), Math.h, Command.h
hal/      Pins.h (S3), Display.h, AdafruitDisplay, Pump, VentValve, Battery,
          PressureSensor, Buttons, Storage (LittleFS wrapper)
services/ VacuumSystem (moved), SnapshotBuilder (extracted from App),
          ProgramStore, ProgramParams
hosts/    serial/SerialLink (moved), program/ProgramApi, program/BerryRuntime,
          program/NullRuntime
ui/       UiController, Renderer, pages/{Programs,Params,ProgramScreen}
app/      App (composition root + task creation), AppDisplay
```

[VacuumCore.h](../libraries/VacuumCore/src/VacuumCore.h) remains the single umbrella include
for the sketch.

## 13. Phasing

Phase 0 (S3 migration) is **complete**: FQBN
`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`, the pin map in
[Pins.h](../libraries/VacuumCore/src/hal/Pins.h), and host presence verified.

Each remaining phase keeps the build green (`powershell -File scripts/arduino.ps1 -Task build`).

| Phase | Content | Gate |
| --- | --- | --- |
| 1 | Task architecture: plant + serial tasks, command mailbox, snapshot slot, log queue | Protocol behavior parity on dual-core |
| 2 | `hal/Buttons`, `hal/Storage`, UI framework: BROWSER, PARAMS, RUN screens, `default` seeding (NullRuntime) | Button navigation works with no VM; host-drawn mode still works |
| 3 | `ProgramStore` + `ProgramParams` + manifest parsing + serial program commands (`list`/`cat`/`rm`/`run`/`params`/`set params`/upload) | Programs storable, browsable and runnable (NullRuntime: `run` → "runtime not built in") |
| 4 | Berry VM vendored + `ProgramApi`/`BerryRuntime` on core 1, ProgramScreen overlay, run from UI and CLI | `default` and `pulse` examples run on core 1 while plant + serial run on core 0 |
| 5 | Disconnected operation, magic-default boot, break-out semantics, program state table, watchdogs | Unplug USB mid-program → program continues; `status` shows `host`/`program`; Back gestures escape |
| 6 | Docs, version bump, cleanup | Docs match behavior |

## 14. Risks to verify during implementation

- **Boot now runs a program.** The seeded `default` is passive, but once an owner replaces
  it, boot behavior is owner-defined. The fail-safe valve and fault latch still hold; this
  is called out in the docs, not left implicit.
- **Exact Berry execution-limit/yield hook name** — confirm against Tasmota's Berry port;
  the runaway-program watchdog depends on it.
- **Button pin reachability** — the pins (1/2/10/11) are confirmed on the S3 Super Mini used
  for development, but re-check them if the device moves to a different S3 carrier.
- **Host presence was verified on the `hwcdc` CDC in Phase 0**, but re-check it on the
  production board if the USB wiring changes, since the display-mode switch and
  disconnected-operation rules rely on it.
- **Wire thread affinity** — confirm all I2C traffic stays on the plant task; Wire is not
  cross-task safe.
- **The UX is only a spec until it is on glass.** The browser/params/run flow needs a
  physical pass on the real board before the gestures are locked in.

## 15. Detailed specs

One spec per feature, under [docs/specs/](specs/):

| Spec | Feature |
| --- | --- |
| [01-task-architecture](specs/01-task-architecture.md) | Cores, tasks, IPC, watchdogs |
| [02-program-store-and-params](specs/02-program-store-and-params.md) | LittleFS store, manifest, params, persistence, `default` |
| [03-berry-runtime-and-api](specs/03-berry-runtime-and-api.md) | Berry VM, `ProgramApi`, Berry binding, input ownership |
| [04-on-device-ux](specs/04-on-device-ux.md) | Buttons, browser, params editor, run screen, break-out |
| [05-disconnected-operation](specs/05-disconnected-operation.md) | Link loss, magic default boot, takeover, break-out |
| [06-serial-protocol](specs/06-serial-protocol.md) | Program commands, upload framing, status/info keys |
| [07-snapshot-and-display](specs/07-snapshot-and-display.md) | Snapshot extensions, ProgramScreen, display ownership |
