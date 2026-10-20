# Queues and shared state

Paths are relative to the repo root. The rule this whole conversion is built on: **dirty reads are
fine; a lock appears only when a value gains writers on more than one task.** The design keeps one
writer per object, so no mutex, no semaphore and no seqlock exist. The only IPC is three queues,
and they exist to move a **write** to its owner — not to guard a read.

## Ownership matrix

Every object has one writing task. Everything else reads the live object directly.

| Object | Owner (writer) | Readers (dirty) | Notes |
|---|---|---|---|
| `VacuumSystem` | Control | Interface (`vacuum get`, BLE publish), Panel (VM `state()`, overlay) | Scalars; a multi-field `vacuum get` may mix two ticks — accepted. |
| `Pump` | Control | Interface (`vacuum get` duty) | Read via `vacuum.pump()`. |
| `PressureSensor` | Control | Interface (`vacuum get` sensor=) | `present()` / `kpa()` scalars. |
| `Battery` | Control | Interface (`device get`), Panel (overlay, VM `state()`) | `percent()` / `volts()` scalars. |
| `BuzzerPlayer` / `Buzzer` | Control | Interface (`buzzer get`) | `sounding()` / `frequencyHz()` / `remainingMs()` read dirty. |
| `Buttons` (debounce) | Control | Panel reads one published scalar (`okHeldMs`) | The `settle()`/`consumePress()` state is control-private. |
| `Ownership` | Interface | Panel (page choice), BLE (`owned()`) | `owner_` / `stale_` are bytes; dirty read is fine. |
| `Settings` | Interface | Panel (VM `device.*`, `screen.unit()`), BLE (`unit`) | Scalars written on `device set` / VM `device.set_*`. |
| `Storage` (LittleFS) | Interface | nobody else | LittleFS is not re-entrant, so even **reads** stay on the interface task. |
| `Display` / `Overlay` | Panel | nobody else | Private. |
| `Routines` (the VM) | Panel | nobody else | Private. |
| `logEnabled` (in `Settings`) | Interface | `device get log` (via Settings) | Persisted; read at boot to choose the `Logger`; a `set` applies on the next reset. |
| `Serial` stream | everyone who logs | — | Replies from Interface; log lines from any task; interleaving accepted. |

Two control-owned scalars cross to the panel task and are worth spelling out:

- `okHeldMs` — control writes `heldMs(Button::Ok)` each tick; the panel's default-hold watch reads it
  dirty. The `Buttons` debounce object itself is never touched by the panel task.
- `stopSeq` — control increments a `volatile uint32_t` each time the Back-hold fires; the panel's
  `Routines::takeStopped()` compares it against its own last-seen value and answers true once per
  increment. This replaces `Routines::stopped_` (a bool two tasks would otherwise both write) with a
  single-writer monotonic counter.

## Queue payloads

Copied by value through FreeRTOS queues. `String` members are used because the project convention
is `String`, not fixed char buffers; the items are rare and small. `MachineCommand` and
`ButtonEvent` live in `app/Tasks.h`; the store types live in `app/StoreBus.h`.

```cpp
// Writes to VacuumSystem / BuzzerPlayer, applied on the control task.
struct MachineCommand {
  enum class Op : uint8_t {
    Start, Stop, ResetFault, SetTarget, ClearTarget, Reboot, Tone, Play, Off,
  };
  Op op;
  float value = 0.0f;      // SetTarget: kPa; Tone: Hz
  uint32_t durationMs = 0; // Tone
  String text;             // Play: the raw note sequence
};

// The store bus (see "Store access" below): the one request the VM task sends to the
// interface task for flash-backed reads and writes.
struct StoreResult {
  bool ok = false;
  String value;
};

struct StoreRequest {
  enum class Op : uint8_t { Get, Set, SetUnit, SetTarget, SetBluetooth };
  Op op;
  String routine;                // Get/Set: the routine's registered name, already resolved
  String key;                    // Get/Set
  String value;                  // Set
  uint8_t unit = 0;              // SetUnit: DisplayUnit
  float targetKpa = 0.0f;        // SetTarget
  bool bluetooth = false;        // SetBluetooth
  StoreResult* result = nullptr; // Get/Set: the client's slot, filled by the server
  TaskHandle_t waiter = nullptr; // Get/Set: the task to notify when the result is ready
};

// One classified press, control -> panel.
struct ButtonEvent {
  Button button;
  Press press;
};
```

## The three queues

| Queue | Sender → Receiver | Depth | Blocking |
|---|---|---|---|
| machine commands | Interface, Panel → Control | 8 | `xQueueSend`, non-blocking; overflow drops the newest command with a `[W]` log line. |
| store | Panel → Interface | 4 | `xQueueSend`, non-blocking; same overflow rule. |
| button events | Control → Panel | 8 | `xQueueSend`, non-blocking; overflow logs, matching today's `pressOverflow_`. |

Overflow is effectively unreachable: serial commands are paced by the host reading replies, BLE
writes by the connection, VM commands by a 10 ms poll, and the consumers drain every tick. The
non-blocking send is the whole policy — no producer ever stalls behind a full queue.

## Store access

The one place a Berry routine reads or writes flash is `store.*`, and it cannot run on the panel task
because LittleFS is not re-entrant. So store access is the single request/response in the system, and
it gets its own primitive rather than raw queue sends: a `StoreClient` on the panel side and a
`StoreServer` on the interface side, sharing one queue of `StoreRequest`s. The same bus carries the
void `device.set_*` settings writes, so the VM has exactly one way to reach flash.

All four `store.get`/`store.set` forms round-trip, because their Berry return value is the file's
answer (`get` returns the stored text or the fallback; `set` returns whether the file took the
write). Blocking the VM task for a LittleFS op is no worse than today, where the same read/write ran
inline and stalled the whole loop.

`StoreClient` — panel side, in `app/StoreBus.h`:

```cpp
class StoreClient {
 public:
  explicit StoreClient(QueueHandle_t queue);

  bool get(const char* routine, const char* key, String& out);         // blocks
  bool set(const char* routine, const char* key, const String& value); // blocks
  void setUnit(DisplayUnit unit);      // one-way
  void setTarget(float kpa);           // one-way
  void setBluetooth(bool enabled);     // one-way

 private:
  QueueHandle_t queue_;
  StoreResult result_;  // one slot; the VM task is sequential and never has two in flight
};
```

`get` fills a `StoreRequest`, posts it, and blocks on a task notification. The result slot belongs to
the client and is only read after the notification, so nothing locks it:

```cpp
bool StoreClient::get(const char* routine, const char* key, String& out) {
  StoreRequest request;
  request.op = StoreRequest::Op::Get;
  request.routine = routine;
  request.key = key;
  request.result = &result_;
  request.waiter = xTaskGetCurrentTaskHandle();
  if (xQueueSend(queue_, &request, 0) != pdTRUE) return false;  // full: refuse, like "not stored"
  xTaskNotifyWait(0, 0, nullptr, portMAX_DELAY);
  if (!result_.ok) return false;
  out = result_.value;
  return true;
}
```

`set` is the same shape with `Op::Set`, returning `result_.ok`. The one-way calls set `result` and
`waiter` to null and post without waiting.

Task notifications are level-persistent, so a server that answers before the client reaches
`xTaskNotifyWait` still wakes it: there is no lost wakeup, and each notify pairs with exactly one
wait.

`StoreServer` — interface side, same header:

```cpp
class StoreServer {
 public:
  StoreServer(QueueHandle_t queue, Settings& settings, Storage& storage, Logger& logger);
  void drain();  // called each interface-loop pass
 private:
  void handle(const StoreRequest& request);
  QueueHandle_t queue_;
  Settings& settings_;
  Storage& storage_;
  Logger& logger_;
};

void StoreServer::drain() {
  StoreRequest request;
  while (xQueueReceive(queue_, &request, 0) == pdTRUE) handle(request);
}
```

`handle` is the only place the VM's flash traffic runs. `Get`/`Set` build the path
`/routines/<name>.ini`, then read/parse/write through `Storage` and `Ini` exactly as
`Routines::storeGet`/`storeSet` do today, fill `result`, and `xTaskNotifyGive(waiter)`. The three
settings ops call `settings_.setX(...)` and return without answering:

```cpp
void StoreServer::handle(const StoreRequest& request) {
  bool ok = false;
  String out;
  switch (request.op) {
    case StoreRequest::Op::Get: {
      String path = "/routines/" + request.routine + ".ini";
      String text;
      if (storage_.read(path.c_str(), text)) {
        Ini values;
        values.parse(text);
        if (values.has("", request.key.c_str())) {
          out = values.get("", request.key.c_str(), "");
          ok = true;
        }
      }
      break;
    }
    case StoreRequest::Op::Set: {
      String path = "/routines/" + request.routine + ".ini";
      String text;
      Ini values;
      if (storage_.read(path.c_str(), text)) values.parse(text);
      // A repeat value is not worth a flash write, as today.
      if (values.has("", request.key.c_str()) &&
          values.get("", request.key.c_str(), "") == request.value) {
        ok = true;
      } else if (values.set("", request.key.c_str(), request.value)) {
        ok = storage_.write(path.c_str(), values.text());
      }
      break;
    }
    case StoreRequest::Op::SetUnit:       settings_.setUnit(static_cast<DisplayUnit>(request.unit)); return;
    case StoreRequest::Op::SetTarget:     settings_.setTarget(request.targetKpa); return;
    case StoreRequest::Op::SetBluetooth:  settings_.setBluetooth(request.bluetooth); return;
  }
  if (request.result) {
    request.result->ok = ok;
    request.result->value = out;
    xTaskNotifyGive(request.waiter);
  }
}
```

### What the client does not know

The server never learns which routine is active. `Routines::storeGet(key, out)` resolves the active
routine from its own registry and calls `client_.get(entries_[active_].name.c_str(), key, out)`; the
named form passes the name straight through. The `registered` check in `Routines::storePath` stays on
the panel where the registry lives — the server trusts the name it is given and just builds the
path. The `active_ < 0` early-return also stays in `Routines`, so a `store.get` with no active
routine answers `false` without touching the bus.

Wiring: `buildAppContext()` constructs the queue and both ends. The panel task holds the
`StoreClient` — handed to `Routines` for `store.*`, and to `berry::install` (as a new `gStore`
global) for the `device.set_*` bindings. The interface task holds the `StoreServer` and calls
`drain()` each pass.
