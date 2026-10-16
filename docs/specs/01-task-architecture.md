# Spec 01 — Task architecture and threading

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5 and
constraint 1 ("programs run on a different core from the serial link").

Board is the ESP32-S3 (`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`), which has two
cores. There is deliberately **no IPC layer**: the system has at most one controller and
one drawer at a time, so the three tasks share state directly.

## 1. Tasks

| Task | Core | Priority | Stack (start) | Function |
| --- | --- | --- | --- | --- |
| `plantTask` | 0 | 10 | 4 KiB | `App::plantTask(void*)` |
| `serialTask` | 0 | 6 | 4 KiB | `App::serialTask(void*)` |
| `vmTask` | 1 | 3 | 16 KiB | `App::vmTask(void*)` |

Priorities: `plantTask` is highest on core 0 so the 50 ms safety tick is never deferred;
`serialTask` sits above idle and below the plant; `vmTask` is alone on core 1.

## 2. Single-owner rules

Three facts hold at all times, and they are what make direct sharing safe:

- **One controller.** Either the host (serial link) or the running program commands the
  plant — never both. Takeover (spec 05) stops one before the other acts.
- **One drawer.** Either the host (when attached) or the running program (when not) draws
  the panel — never both (spec 04 §5).
- **The plant owns I2C.** Only `plantTask` talks to `Wire`: sensor reads and the display
  `flush()`. Drawing into the framebuffer is plain RAM and is done by the active drawer.

## 3. Shared mutable state, by design

- **`VacuumSystem`** — the controller (serial or VM) calls `setTarget`/`start`/`stop`/…
  directly, and `plantTask` runs `service()`. Its fields are 32-bit aligned, so reads and
  writes are atomic; a controller transition may be observed one tick late. Accepted.
- **The framebuffer** — the active drawer writes it (RAM); the plant reads it when it
  flushes. A torn frame is possible and self-corrects on the next flush. Accepted.
- **The snapshot** — `SystemSnapshot` (spec 07) is one object the plant rewrites each
  tick; `serialTask` and `vmTask` read its fields directly for `status`,
  `vacuum.state()`, and `programs.*`. A read may span two adjacent ticks. Accepted for a
  readout.
- **Buttons** — `Buttons` keeps a small event ring; the plant pushes and the VM pops while
  a program runs (spec 03 §5). Single producer, single consumer.
- **Logs** — any task writes a `[I]/[W]/[E]` record to `Serial` under a `portMUX`, so
  records do not interleave at the byte level.
- **`ProgramStore` (LittleFS)** — normally touched only by the plant (refresh, upload).
  The VM reads it for `programs.params(name)` (spec 03 §6.5); a read concurrent with an
  upload is rare and non-safety-critical — it may return stale data or fail, and the
  program retries.

## 4. What is never shared

- `Wire` / I2C — plant only.
- The pump, valve, and battery driver update paths — plant only.

## 5. Runaway-program protection

- Core separation is the primary protection: a CPU-bound Berry loop on core 1 cannot
  delay the plant on core 0 at all.
- The VM increments a `volatile uint32_t` heartbeat on every yield (`vacuum.delay` and the
  Berry execution-limit hook). `plantTask` samples it each tick; if unchanged for
  > 2000 ms it sets the VM's stop-request flag. A VM that still does not yield after a
  further 1000 ms has its task restarted by `App` (delete + create, with a "vm restarted"
  log record).

## 6. Task bodies

### 6.1 `plantTask` — period 50 ms

```
loop:
  now = millis()
  sensor.update(now); pump.update(now); battery.update(now); buttons.update(now)
  system.service(now)                       # the PID step and fault detection
  appDisplay.flush()                        # I2C: battery composite + push to glass
  refresh the snapshot in place             # spec 07
  check vm heartbeat; if stale > 2000 ms: request VM stop
  delay to next 50 ms tick
```

### 6.2 `serialTask`

```
loop:
  poll Serial into the line buffer (8 lines per pass, as today)
  for each complete line: dispatch
    info/status/help → read the snapshot directly and reply
    draw <op> …      → call the Display verb directly (framebuffer)
    flush            → reply ok flushed (the plant pushes within one tick)
    other commands   → call VacuumSystem directly
  detect host attach/detach; flip display mode (spec 05)
```

### 6.3 `vmTask`

```
loop:
  if not running: block on start notification
  run the Berry VM until it yields or stops:
    program code calls vacuum.* / screen.* / buttons.* / programs.* directly
  if stopRequested: stop program, notify plant
  heartbeat update on every yield
```

## 7. Acceptance criteria

- Serial protocol behaves identically to today on the S3 while a Berry program runs on
  core 1 (`start`/`status`/`stop`/`draw`/`flush` all reply correctly).
- A `while true` program with no `vacuum.delay` does not stop `status` replies or the
  PID tick.
- A busy program drawing every loop cannot delay the control loop or block I2C.
- `Wire` is only ever touched from `plantTask`; the framebuffer is only written by the
  active drawer.
