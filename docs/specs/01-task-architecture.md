# Spec 01 — Task architecture and threading

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5 and
constraint 1 ("programs run on a different core from the serial link").

Board is the ESP32-S3 (`esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`), which has
two cores. The split is:

- **Core 0** — the two bounded jobs: the plant task and the serial link task.
- **Core 1** — the one unbounded job: the Berry VM task.

## 1. Tasks

Created in `App::begin()`. The Arduino `loop()` becomes `vTaskDelete(NULL)`; all work
moves into these tasks.

| Task | Core | Priority | Stack (start) | Function |
| --- | --- | --- | --- | --- |
| `plantTask` | 0 | 10 | 4 KiB | `App::plantTask(void*)` |
| `serialTask` | 0 | 6 | 4 KiB | `App::serialTask(void*)` |
| `vmTask` | 1 | 3 | 16 KiB | `App::vmTask(void*)` |

Stack sizes are starting points; the VM stack is the one most likely to need tuning
(Berry recurses on its call stack).

Priorities: `plantTask` is highest on core 0 so a serial flood can never defer the
50 ms control tick; `serialTask` sits above idle and below the plant. `vmTask` is alone
on core 1, so its priority only matters relative to core 1's idle task.

## 2. Ownership matrix

Every piece of state has exactly one owner task. Cross-task access happens only through
the primitives in §3.

| Module / state | Owner | Other tasks may |
| --- | --- | --- |
| `VacuumSystem`, `PressureSensor`, `Pump`, `VentValve`, `Battery` | plant | read the snapshot |
| `Wire` / I2C (OLED + MPRLS) | plant | never |
| display framebuffer + `AppDisplay` | plant | never |
| `Buttons` scan | plant | consume button events |
| `ProgramStore` (LittleFS) | plant | enqueue store commands |
| snapshot | plant (publisher) | copy out |
| program overlay | VM (writer) | plant (reader, compositor) |
| `Serial` stream | serial | plant sends log records |
| Berry VM | VM | plant sends stop requests |

## 3. IPC primitives

All cross-task state is one of these; there is no other shared mutable state.

### 3.1 Command mailbox

`core/Command.h` defines plain, copyable, Arduino-free data:

```cpp
enum class CommandKind : uint8_t {
  Draw, Flush,                 // validated locally by serial; fire-and-forget
  SetTarget, ClearTarget, Start, Stop, Vent, ResetFault,
  SetAmbient, CaptureAmbient,
  RunProgram, StopProgram,
  ListPrograms, CatProgram, RmProgram, SetProgramParams,
  UploadBegin, UploadAppend, UploadCommit, UploadAbort,
};

enum class DrawOp : uint8_t {
  Clear, Pixel, Line, HLine, VLine, Rect, FillRect, RoundRect, FillRoundRect,
  Circle, FillCircle, Ellipse, FillEllipse, Triangle, FillTriangle,
  RotatedRect, FillRotatedRect, Text,
};

struct Command {
  CommandKind kind = CommandKind::Flush;
  DrawOp drawOp = DrawOp::Clear;   // valid when kind == Draw
  int16_t a[6] = {};               // draw coords, rotation, etc.
  float f[4] = {};                 // target, ambient, params
  char text[64] = {};              // draw text, program name, param blob, upload chunk
  uint32_t token = 0;              // request id for serial request/response
};
```

- `QueueHandle_t` created with `xQueueCreate(32, sizeof(Command))`.
- Writers: `serialTask` and `vmTask` (`xQueueSend`). Reader: `plantTask`
  (`xQueueReceive`, timeout 0 or a short block).
- Multi-writer is safe because FreeRTOS queues serialize producers.
- Queue full is a normal event under load: the producer drops the command and logs a
  `[W]` record. Plant-critical commands from the VM must not rely on unbounded
  buffering; programs that enqueue faster than the plant drains see their writes dropped
  and can poll `vacuum.state()` to resync.

### 3.2 Snapshot slot

Latest-value, one publisher (plant), many readers (serial, VM). Double buffer under a
`portMUX`:

```cpp
class SnapshotSlot {
 public:
  void publish(const SystemSnapshot& s);
  void copyOut(SystemSnapshot& out) const;
 private:
  SystemSnapshot slots_[2];
  uint8_t live_ = 0;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};
```

`publish()` copies into the non-live buffer under the lock, flips `live_`, releases.
`copyOut()` copies the live buffer out under the lock. Both are short memcpys (~1.5 KiB),
so the critical section is a few microseconds.

### 3.3 Program overlay slot

Same double-buffer pattern, but writer is `vmTask` and reader is `plantTask`. Carries
`ProgramOverlay` (spec 07): five text lines, a bar fraction, and a toast.

### 3.4 Log queue

`QueueHandle_t` of `LogRecord { char level; char text[96]; }`, depth 32. Plant and VM
push; `serialTask` drains and writes the `[I]/[W]/[E]` lines. This keeps the serial
stream single-writer and makes `set log off` race-free (the serial task owns the flag and
drops queued records when it is off).

### 3.5 Button event queue

`QueueHandle_t` of `ButtonEvent { ButtonId id; ButtonEventKind kind; uint32_t heldMs; }`,
depth 16. `plantTask` pushes after each `Buttons::update()`. When no program owns input,
`plantTask` consumes events itself for the UI; when a program owns input, `plantTask`
forwards them to the VM's queue (spec 03 §4).

### 3.6 Request/response (serial, single-flight)

The serial task handles one command at a time, so one in-flight reply slot is enough:

```cpp
struct CommandReply { uint32_t token; char body[96]; };
SemaphoreHandle_t replyReady_;   // binary semaphore
CommandReply pending_;
```

Flow for a command that can fail (`start`, `stop`, `reset`, `set target`, `ambient`,
`run`, `set params`, store ops): serial assigns `token`, enqueues the `Command`, blocks on
`replyReady_` (timeout 250 ms), then writes `pending_.body` as one line. The plant fills
`pending_` and gives the semaphore. Timeout → the serial task replies `err timeout` and
continues; the plant still executes the command, and a later `status` reveals the truth.

`Draw`/`Flush` skip this path: serial validates them locally (§5.2 of the plan), replies
`ok drawn`/`ok flushed` immediately, and enqueues fire-and-forget.

## 4. Task bodies

### 4.1 `plantTask` — period 50 ms

```
loop:
  now = millis()
  sensor.update(now); pump.update(now); battery.update(now); buttons.update(now)
  drain command mailbox (all pending commands, in order)
  system.service(now)
  publish button events (queue / UI consumption)
  if display mode is Local: ui.update(now, snapshot)
  else if a flush command was drained: apply buffered draw ops, overlay.flush()
  refresh snapshot; snapshotSlot.publish(snapshot)
  check vm heartbeat; if stale > 2000 ms: request VM stop
  delay to next 50 ms tick
```

Ordering rule (from the plan): commands are drained against the machine as the last
control tick left it, so a reply describes something that actually happened.

### 4.2 `serialTask`

```
loop:
  drain log queue → write [level] lines (if logEnabled)
  poll Serial into line buffer (8 lines per pass, as today)
  for each complete line: dispatch
    info/status/help/draw/flush → answer locally / enqueue
    other commands → request/response via command mailbox
  detect host attach/detach; flip display mode; on detach:
    if no program running → enqueue Stop (vent)
    else → nothing (program is the local owner; spec 05)
```

### 4.3 `vmTask`

```
loop:
  if not running: block on start notification
  programApi.tick(): if stopRequested → stop program, publish overlay, notify plant
  heartbeat update on every yield
```

## 5. Runaway-program protection

- Core separation is the primary protection: a CPU-bound Berry loop on core 1 cannot
  delay the plant on core 0 at all.
- The VM increments a `volatile uint32_t` heartbeat on every yield (`vacuum.delay`,
  `vacuum.button_wait`, and the Berry execution-limit hook). `plantTask` samples it each
  tick; if unchanged for > 2000 ms it sets the VM's stop-request flag. A VM that still
  does not yield after a further 1000 ms has its task restarted by `App` (delete + create
  with a "vm restarted" log record).

## 6. Acceptance criteria

- Serial protocol behaves identically to today on the S3 while a Berry program runs on
  core 1 (`start`/`status`/`stop`/`draw`/`flush` all reply correctly).
- A `while true` program with no `vacuum.delay` does not stop `status` replies or the
  PID tick.
- A queued flood of draw commands never delays the control loop.
- All state crosses tasks only through the primitives in §3 (enforceable by review and by
  keeping `App` the only file that creates tasks and queues).
