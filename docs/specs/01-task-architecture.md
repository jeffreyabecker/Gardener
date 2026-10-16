# Spec 01 — Task architecture and threading

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5 and
constraint 1 ("programs run on a different core from the serial link").

Board is the ESP32-S3 (`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`), which has two
cores. There is deliberately **no IPC layer**: two mutually exclusive modes (stand-alone
and peripheral) ensure only one task controls and draws at a time, so the three tasks
share state directly.

## 1. Tasks

| Task | Core | Priority | Stack (start) | Function |
| --- | --- | --- | --- | --- |
| `plantTask` | 0 | 10 | 4 KiB | `App::plantTask(void*)` |
| `serialTask` | 0 | 6 | 4 KiB | `App::serialTask(void*)` |
| `vmTask` | 1 | 3 | 16 KiB | `App::vmTask(void*)` |

Priorities: `plantTask` is highest on core 0 so the 50 ms safety tick is never deferred;
`serialTask` sits above idle and below the plant; `vmTask` is alone on core 1.

## 2. Modes

Contention is resolved by two mutually exclusive modes, not by locks:

- **Stand-alone** — the VM task is running and has access to all resources: the running
  program commands `VacuumSystem` and draws/flushes `Display` directly.
- **Peripheral** — the VM task is stopped; the serial system has full control and does the
  same directly.

`set mode` (spec 06) switches modes; attaching the serial link does not. There is never a
moment when the VM and the serial task are both controlling or both drawing.

## 3. Shared mutable state, by design

- **`VacuumSystem`** — the controller (serial or VM) calls `setTarget`/`start`/`stop`/…
  directly, and `plantTask` runs `service()`. Its fields are 32-bit aligned, so reads and
  writes are atomic; a controller transition may be observed one tick late. Accepted.
- **The display** — the active controller (the VM in stand-alone, the serial task in
  peripheral) draws and flushes `Display` directly, including its `Wire` traffic. Only one
  mode is active, so only one controller ever touches it.
- **`Wire`** — the plant reads the sensor every tick, and the active controller flushes
  the display. Short, interleaved I2C transactions; accepted.
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
  refresh the snapshot in place             # spec 07
  check vm heartbeat; if stale > 2000 ms: request VM stop
  delay to next 50 ms tick
```

### 6.2 `serialTask`

```
loop:
  drain Serial into the line buffer and dispatch every complete line it received
  for each complete line: dispatch
    info/status/help/list/params → read the snapshot / store directly and reply
    set mode → stop the VM (peripheral) or start `default` (stand-alone)
    in peripheral mode:
      draw/flush → call the Display verbs directly
      start/stop/set target/… → call VacuumSystem directly
    in stand-alone mode:
      control and draw commands are refused (`use set mode peripheral`)
  on detach: if peripheral → vent + set mode stand-alone (spec 05 §3)
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

- `set mode peripheral` stops the VM and gives the serial system full control;
  `set mode standalone` returns control to the VM (runs `default`).
- Attaching the serial link does not change the mode.
- A `while true` program with no `vacuum.delay` does not stop `status` replies or the
  PID tick.
- A busy program drawing every loop cannot delay the control loop.
- Disconnecting in peripheral mode vents and returns to stand-alone (spec 05 §3).
