# On-device control with Berry

Status: plan, not yet implemented.

Read section 2 as the requirements and the rest as the smallest mechanism that answers them.
Where a mechanism is not settled the plan says so and says what would settle it. One requirement
is like that — constraint 1, isolation from a runaway program — and section 5 is where it is
argued rather than assumed.

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

1. **A runaway program must not delay or corrupt control.** A program is unbounded work and the
   control tick is not, so a program that never returns must not be able to hold them together.
   This is the one requirement whose mechanism is not settled: section 5.
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
| Execution model | The one loop the device already has, a program included, with the VM on a slice | The loop runs the plant and the serial line today. Constraint 1 may need only cooperation, and that is measurable before it is architectural — section 5. |
| Plant ownership | Sensor, pump, valve, battery and buttons keep **one** owner, and whoever is controlling — a program or the serial line — commands it | The control loop is the safety-critical code, and exactly one writer to the pump and valve is what makes it safe. |
| Display to programs | Programs draw with the same display verbs as the host — the same `Display`, the same overlay, held by whoever is controlling | Only one mode is active, so only one controller ever draws; the overlay adds the battery to every flush, so neither the host nor a program has to know about it. |
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

## 5. Execution model

The device runs one loop, as it does today: the plant tick, the serial line, and the program.
Adding the VM does not have to change that, so this section is the requirement first and the
smallest mechanism that answers it second.

### 5.1 Modes

There is no IPC layer and no lock, because two mutually exclusive modes mean exactly one
controller:

- **Stand-alone** — the VM runs; the running program commands `VacuumSystem` and
  draws/flushes `Display` directly.
- **Peripheral** — the VM is stopped; the serial system does the same directly.

`set mode` (spec 06) switches; attaching the serial link does not. What both of them reach
(spec 01 §3): `VacuumSystem`, the display, the snapshot, the button event ring, and
`ProgramStore`. Nothing else.

### 5.2 What a command is

- Serial: parse the line, then call the target directly — `info`/`status` read the
  snapshot, `set mode` flips modes, and in peripheral mode `draw`/`flush` call `Display`
  and `start`/`stop`/`set target`/… call `VacuumSystem`. In stand-alone mode those
  control/draw commands are refused.
- Program: `vacuum.*` / `screen.*` / `programs.*` call `VacuumSystem`, `Display`, the
  snapshot, and `ProgramStore` directly. A call is fire-and-forget; the authoritative
  result is the next `status` / `vacuum.state()`.

### 5.3 Runaway programs

**What has to be true.** The control tick keeps `CONTROL_PERIOD_MS`. Everything the device
guarantees about safety is counted in ticks, so a program that never returns must not be able to
stretch one.

**The smallest mechanism.** The VM runs on a slice in the loop: a bounded number of VM
instructions, then a yield, then a tick, then the program resumes. Berry's execution-limit hook
is where that budget lives — `vacuum.delay(ms)` is the deliberate way for a program to yield, and
the hook is what makes an unwilling one yield anyway. The budget is an instruction count rather
than a clock, because a counter is cheap to check and needs no timer.

**What would force something else.** If the vendored Berry port has no execution-limit hook, or a
budget cannot hold the tick, the program needs somewhere else to run — a second task, or a second
core. That is the fallback, and it is not free: section 5.1 changes from ownership to
synchronization, and a watchdog plus a task restart path come with it. None of that is worth
building before a measurement asks for it, and the measurement is simple.

**The measurement.** Run `default` and a deliberately hostile program — `while true`, no
`vacuum.delay` — and watch the tick. If the period holds, the loop is the answer and this section
is finished.

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
it into the typed `Param` set handed to the program; the Berry `program.param(name)`
accessor returns the typed value at run time. `choice` and `bool` are the recommended types
for anything that will be tuned without a host, because they need no numeric stepping.

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
- Everything else about `default` is ordinary: the store pins it first in the program
  list, it can take parameters, and it runs like any other program.

## 7. On-device UX

Programs own the screen; the device owns only the battery and the Back-hold abort.

### 7.1 Screen ownership

- A running program draws the entire panel with the `screen.draw_*` verbs and pushes with
  `screen.flush()`. The device composites only the battery (`Overlay`) on every flush,
  so a program can never erase it.
- There are no device-drawn pages. The browser, the params editor and the home shell are
  all Berry programs. A reusable list/menu abstraction, if one is wanted, is written in
  Berry — it is not firmware.

### 7.2 Buttons

- `hal/Buttons` debounces the four buttons into click / long-press / repeat events.
- While a program runs and no host is attached, every event — Up, Down, OK and Back, all
  kinds — is delivered to the program's `buttons.on` callbacks (spec 03 §5). Nothing is
  claimed, consumed or withheld by the device; buttons mean whatever the program says they
  mean.

### 7.3 The hard-coded abort

**Holding Back for 5 s is the device's only button behaviour, and the firmware hard-codes
it**: stop the current program, vent, then run `default`. Every program change vents first, so
the chamber is left released before the next program acts.

The override is a hold condition, not an event, so it can neither be claimed nor
suppressed: the program still receives the Back events of that press, and it is aborted
anyway.

### 7.4 Reserved programs

- `default` — the boot and home program (seeded as a passive readout; an owner can
  replace it).
- `browser` — the program selector and params editor (also seeded).

They are ordinary stored programs, and the runtime reserves nothing. Only `default` is
named by the device — boot starts it and the abort returns to it. `browser` is reached the
way any program is: `programs.run("browser")` from a running program, or `run browser` over
the wire.

### 7.5 Modes

- **Stand-alone** — the VM runs; the running program draws and commands.
- **Peripheral** — the VM is stopped; the serial system draws and commands.

Modes are switched by `set mode` (spec 06), not by attaching the serial link. Attaching
does not change the mode; disconnecting in peripheral mode vents and switches to
stand-alone (spec 05 §3).

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

The split is explicit in the `browser` program:

- **Run with last parameters** = select the program, OK click. One press, nothing to
  retype.
- **Run with different parameters** = open its params, step through them, then run.
  Multi-step by design.

Persisted "last run" values are what make disconnected use easy: tune a program once (or
over the wire), and every later disconnected run reuses those values without re-entering
them.

### 8.3 A disconnected walkthrough

1. Device boots unplugged → `default` starts and shows the live readout.
2. Open the selector from `default` (`programs.run("browser")`) → the current program
   stops, the chamber vents, and `browser` runs.
3. Up/Down to `pulse`, OK long-press → the `browser` opens its params.
4. OK to reach `cycles`, Up/Down (repeat for coarse) to set 5.
5. OK long-press → params persist, `pulse` runs with `cycles=5`.
6. Back hold 5 s → `pulse` stops, chamber vents, `default` runs again.
7. Power-cycle → `default` runs again, because `default` is the boot program.

If the user wants `pulse`-with-5-cycles at every boot, they overwrite `default` with that
source (or set `default`'s own params), not a separate autostart knob.

### 8.4 Persistence and merge rules

- Persisted per-program values are written when a program is run from the `browser`
  program's params editor or via `set params <name> …`. They survive reboot in LittleFS.
- On start, effective params = manifest defaults overlaid with persisted values, as in
  section 6.2. A re-uploaded program with a changed manifest loses obsolete keys and fills
  missing ones with defaults — never a crash, never a silent type mismatch.
- Parameters are **captured at start** and do not change mid-run. Changing them means
  breaking out and re-running, which is the deliberate "clunkier" path.

### 8.5 Edge cases

- **Zero-parameter program** — select it and OK click runs it directly; there is nothing
  to edit.
- **Store has only `default`** — the browser lists one row; OK click runs `default` again
  and effectively keeps the device home.
- **`default` deleted** — the store re-seeds the passive source on next refresh; the device
  is never without a boot program.
- **Host attached while a program runs** — the host can `stop`, `run` another program, or
  draw its own panel; the local buttons are ignored until it leaves.
- **Program consumes every button it can** — Back hold 5 s still aborts, so the user can
  always vent and get home.

## 9. Snapshot extensibility for programs

`SystemSnapshot` stays plain, copyable data and grows additively:

- `ProgramStatus` sub-struct: `running`, `programName`, `error` / last error,
  `hostAttached`.
- **No program list.** The snapshot does not carry the store: `ProgramStore` owns the list
  and `count()`/`at()` answer `list` and the `programs.*` verbs on demand (spec 02 §7), so
  there is nothing to keep in sync.
- A fixed-capacity **program state table** a program can write and read:
  `program.state_set("label", 42.5)` / `program.state_get("label")`. These keys are
  appended to the `status` payload (`s0=label s0v=42.5 …`) and available to the program's
  own screen, so programs publish their own telemetry without any C++ change.
- The whole snapshot is exposed read-only to Berry as `vacuum.state()`, a Berry map of the
  plant readout built when a program asks for it.

## 10. Exposing the Display to programs

- The active controller holds a `Display&` (the `Overlay` instance) and draws and
  flushes directly: a program in stand-alone mode, the serial line in peripheral mode. The
  modes are mutually exclusive, so exactly one of them ever touches it.
- Programs draw with the same **drawing verbs** the host uses: each `screen.draw_<op>(…)`
  call is a direct `Display` call, and `screen.flush()` pushes the frame.
- The op set is the one already on the wire — clear, pixel, line, hline, vline, rect,
  fillrect, roundrect, fillroundrect, circle, fillcircle, ellipse, fillellipse, triangle,
  filltriangle, rotatedrect, fillrotatedrect, text, plus `flush`.
- `Overlay` composites the battery on every flush, so the battery (like the splash)
  remains the device's and cannot be erased.

## 11. Serial protocol changes

New program commands (additive; existing commands keep their meaning):

| Command | Reply | Notes |
| --- | --- | --- |
| `list` | `ok programs=N` then one row per program, then `eof` | rows carry name, size, param count, `default`/`running` flags, read from the store (spec 02 §7) |
| `cat <name>` | paged source | as the old `cat` |
| `rm <name>` | `ok removed <name>` | `rm default` re-seeds the passive source |
| `run <name> [k=v …]` | `ok running <name>` / `err …` | stop current program → vent → start; named params validated against the manifest |
| `stop` | `ok stopped` | stop current program/run → vent → **idle**. Does not auto-start `default` while a host is attached, so the host is never surprised |
| `params <name>` | `ok params=<name> k=v …` | effective values (persisted over defaults) |
| `set params <name> k=v …` | `ok params=<name> k=v …` | persist without running |
| `prog <name>` (source to the `set term` marker) | `ok stored <name> size=<n>` | one shot, not chunked or hex encoded: the source follows the first newline and runs to the custom terminator, staged temp + rename on commit |

`status` gains `program=<name|->`, `running=0|1`, `host=0|1`, and the program state table
(`s0=key s0v=val …`). `help` lists the new commands.

The abort and serial `stop` differ on purpose: Back hold 5 s always homes to `default`
(the device's own home), while serial `stop` leaves the device idle (the host is present
and decides what happens next).

## 12. Layer layout

The three layers stay, and this plan adds two devices to `hal/` where the rest of the device
lives: `Buttons` and `Storage` (the LittleFS wrapper). The VM, the `ProgramApi` binding,
`ProgramStore` and `ProgramParams` go in `app/` beside the serial interface, which is where the
way in already is. [check-layers.ps1](../scripts/check-layers.ps1) is not touched: the rule is
still `app/` → `hal/` → `core/`, and nothing here points the other way.

That is a refusal to pre-split, and it is a change of position. This plan used to propose
re-introducing the five-layer `core/hal/services/hosts/app` layout on the argument that
otherwise `app/` absorbs a UI, a VM and a file store. It will, by the end of this plan. But five
layers for one instrument, invented before anything in `app/` had a second consumer, is the shape
the project has already paid for once: what actually moved `Overlay` out of `app/` was a second
`app/` file needing it, and what split `SerialLink` was a file too long to read, not a plan. If
`app/` becomes two problems, that is the commit that splits it — pointed at a consumer, the way
those two were.

## 13. Phasing

Phase 0 (S3 migration) is **complete**: FQBN
`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`, the pin map in
[Pins.h](../libraries/VacuumCore/src/hal/Pins.h), and host presence verified.

Each remaining phase keeps the build green (`powershell -File scripts/arduino.ps1 -Task build`).

| Phase | Content | Gate |
| --- | --- | --- |
| 1 | Execution model: the program runs on a slice in the loop that already exists, budget from the Berry hook | A `while true` program with no `vacuum.delay` cannot stretch the control tick (§5.3) |
| 2 | `hal/Buttons`, `hal/Storage`, `ProgramStore` seeding of `default` and `browser` | Buttons scanned and stored programs listed via serial |
| 3 | `ProgramStore` + `ProgramParams` + manifest parsing + serial program commands (`list`/`cat`/`rm`/`params`/`set params`/upload) | Programs storable and listable over the wire; `run` waits for the VM |
| 4 | Vendor Berry + `BerryRuntime`/`ProgramApi`, `vacuum`/`screen`/`buttons`/`program` modules, `run` command, seeded `default` and `browser` | `default` and `browser` run and draw, and the tick still holds with a program running |
| 5 | Disconnected operation, reserved-program boot, abort semantics, program state table, runaway handling | Unplug USB mid-program → program continues; `status` shows `host`/`program`; Back hold 5 s aborts home |
| 6 | Docs, version bump, cleanup | Docs match behavior |

## 14. Risks to verify during implementation

- **Boot now runs a program.** The seeded `default` is passive, but once an owner replaces
  it, boot behavior is owner-defined. The fail-safe valve and fault latch still hold; this
  is called out in the docs, not left implicit.
- **Whether the vendored Berry port has an execution-limit hook.** This is the hinge for §5.3:
  with it, a runaway program is a budget in the loop that already exists; without it, the program
  needs a context of its own, which is the fallback and a piece of work in its own right. Confirm
  against Tasmota's Berry port before phase 4.
- **Button pin reachability** — the pins (1/2/10/11) are confirmed on the S3 Super Mini used
  for development, but re-check them if the device moves to a different S3 carrier.
- **Host presence was verified on the `hwcdc` CDC in Phase 0**, but re-check it on the
  production board if the USB wiring changes, since the display-mode switch and
  disconnected-operation rules rely on it.
- **`Wire` if §5.3's fallback is ever taken.** Today one loop reads the sensor and flushes the
  display, so nothing interleaves and there is nothing to serialize. A second context would put
  two readers on one bus, and that would be the first thing to fix rather than a risk to carry.
- **The UX is only a spec until it is on glass.** The browser/params/run flow needs a
  physical pass on the real board before the interaction is locked in.
- **Nothing on the device reaches `browser`.** With no selector gesture, the way in is
  `programs.run("browser")` from a program or `run browser` over the wire, so the seeded
  `default` has to offer one. Confirm it does — otherwise the stock device ships a selector
  it cannot open.

## 15. Detailed specs

One spec per feature, under [docs/specs/](specs/):

| Spec | Feature |
| --- | --- |
| [01-task-architecture](specs/01-task-architecture.md) | Cores, tasks, IPC, watchdogs — **the one spec §5 no longer agrees with** |
| [02-program-store-and-params](specs/02-program-store-and-params.md) | LittleFS store, manifest, params, persistence, `default` |
| [03-berry-runtime-and-api](specs/03-berry-runtime-and-api.md) | Berry VM, `ProgramApi`, Berry binding, `programs` module, button callbacks |
| [04-screen-and-buttons](specs/04-screen-and-buttons.md) | Screen and button ownership, the abort override |
| [05-disconnected-operation](specs/05-disconnected-operation.md) | Link loss, magic default boot, takeover, abort |
| [06-serial-protocol](specs/06-serial-protocol.md) | Program commands, upload framing, status/info keys |

Spec 01 is the exception. It specifies three FreeRTOS tasks, a heartbeat and a VM restart — the
mechanism §5.3 now defers until a measurement asks for it — so it is the one spec that has to be
rewritten before phase 1 starts. The rest are unaffected by that delay: they are about what the
program model does, not about where it runs.
| [07-snapshot-and-display](specs/07-snapshot-and-display.md) | Snapshot extensions, program draw access, display ownership |
