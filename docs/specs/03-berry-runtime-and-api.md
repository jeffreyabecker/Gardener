# Spec 03 — Berry runtime and program API

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5.3, §7.6 and
the Berry half of constraint 2.

## 1. Runtime abstraction

The rest of the firmware depends on an abstract runtime, never on Berry headers, so a
build without the VM still ships and the command router never sees Berry:

```cpp
class ProgramRuntime {
 public:
  virtual ~ProgramRuntime() = default;
  virtual const char* name() const = 0;               // "berry" | "none"
  virtual bool available() const = 0;                 // false for NullRuntime
  virtual bool start(const char* programName, const String& source,
                     const String& effectiveParams, String& error) = 0;
  virtual void stop(const char* reason) = 0;          // async; sets stop flag
  virtual bool running() const = 0;
  virtual const char* currentProgram() const = 0;
  virtual const char* lastError() const = 0;
  virtual void update() = 0;                          // called by vmTask
  virtual bool evaluate(const String& line, String& out) = 0;  // optional REPL
};
```

Two implementations:

- `NullRuntime` — `available() == false`; `start()` returns false with
  `"runtime not built in"`. Lets phases 2–3 land before the VM exists.
- `BerryRuntime` — the real thing, below.

## 2. Vendoring and compile

- Vendor upstream Berry (the Tasmota port is the reference) as a third-party library, not
  a hand fork. Keep it out of `libraries/VacuumCore` if it ships headers that would leak
  into the layer check.
- Compile options: single-instance VM, no `be_file` I/O (no filesystem access from
  Berry), conservative default heap. Confirm and document the exact execution-limit /
  yield hook name (Tasmota uses a per-call instruction budget); it backs the heartbeat in
  spec 01 §5.

## 3. `BerryRuntime`

```cpp
class BerryRuntime : public ProgramRuntime {
 public:
  bool start(const char* programName, const String& source,
             const String& effectiveParams, String& error) override;
  void stop(const char* reason) override;
  bool running() const override;
  void update() override;             // drain button events → callbacks; on stop: unwind
  ...
 private:
  bvm* vm_ = nullptr;
  ProgramApi& api_;
  volatile bool stopRequested_ = false;
  volatile uint32_t heartbeat_ = 0;
  String currentProgram_;
  String lastError_;
};
```

Start flow:

1. Compile `source` with `be_loadbuffer` (or equivalent). Compile error → `error` set,
   `lastError_` set, return false.
2. Bind the `vacuum`, `program`, and `screen` modules (one global `be_map`, native
   functions).
3. Set `program.name` and the effective params.
4. Run the top-level call. `start()` returns as soon as the program begins; the VM task
   keeps it alive via `update()`.

Stop flow:

1. `stop()` sets `stopRequested_ = true`.
2. The program observes it at the next yield (`vacuum.delay` or the execution-limit hook)
   and unwinds; `update()` notices and finalizes.
3. If the heartbeat shows the program is not yielding (spec 01 §5), the plant asks `App`
   to delete and recreate `vmTask`, which abandons the VM.

## 4. Native surface: `ProgramApi`

Thin, plain C++, no VM in the loop. The same behaviour is reachable from the serial
command path, the UI, and (later) tests.

```cpp
class ProgramApi {
 public:
  ProgramApi(VacuumSystem& system, ProgramParams::Context& params);

  // --- plant (commands are enqueued; the one read is from the local snapshot) ---
  void setTarget(float kpa);
  void clearTarget();
  bool start();                      // false when snapshot says not ready
  void stop();                       // stop + vent
  void resetFault();
  PlantState state() const;          // the whole plant readout

  // --- drawing (spec 07) ---
  void drawClear();
  void drawPixel(int16_t x, int16_t y);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
  void drawHLine(int16_t x, int16_t y, int16_t w);
  void drawVLine(int16_t x, int16_t y, int16_t h);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
  void drawFillRect(int16_t x, int16_t y, int16_t w, int16_t h);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r);
  void drawFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r);
  void drawCircle(int16_t x, int16_t y, int16_t r);
  void drawFillCircle(int16_t x, int16_t y, int16_t r);
  void drawEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh);
  void drawFillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2);
  void drawFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                        int16_t y2);
  void drawRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle);
  void drawFillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle);
  void drawText(int16_t x, int16_t y, uint8_t size, bool inverted, const char* text);
  void flush();

  // --- identity / params / state ---
  const char* programName() const;
  bool param(const char* name, float& out) const;      // number/int
  bool param(const char* name, String& out) const;     // choice/bool/text
  void stateSet(const char* key, float value);
  void stateSet(const char* key, const char* value);
  bool stateGet(const char* key, float& out) const;
  bool stateGet(const char* key, String& out) const;

  // --- misc ---
  bool hostPresent() const;
  void log(const char* message);
  uint32_t nowMs() const;
  void delay(uint32_t ms);           // yields; aborts on stopRequested

  void setServiceHook(void (*hook)());
  bool stopRequested() const;
};
```

Rules:

- `state()` (and `hostPresent()`, `param()`, `stateGet()`) reads the VM task's local
  snapshot copy plus the effective param set captured at start. It never blocks.
- Writes (`setTarget`, `clearTarget`, `start`, `stop`, `resetFault`) enqueue a `Command`
  to the plant and return immediately. `start()` returns the local prediction from the
  snapshot and may disagree with the plant's authoritative decision; programs that care
  poll `state()`.
- `delay()` yields: it calls `serviceHook()` (which services the snapshot copy) and checks
  `stopRequested()` every ~2 ms.

## 5. Button callbacks

Buttons are exposed as callbacks, not polling: registering a callback is owning input
(full API in §6.4). While a program runs, the plant forwards Up, Down, OK — and Back when
claimed — to the VM's button queue (spec 01 §3.5); `BerryRuntime::update()` drains it and
invokes the matching callbacks.

Back is special: it is device-owned (break-out, spec 05 §4) **unless the program claims
it** by registering `buttons.on(buttons.BACK, fn)`. Claiming Back takes the break-out
gestures; releasing it (`buttons.off(buttons.BACK)` / `buttons.clear()`) returns them to
the device. A program that claims Back and stops yielding is still stopped and vented by
the runaway watchdog (spec 01 §5).

## 6. Berry binding

Four modules, registered once per VM. Argument order matches the host `draw` ops where
the surfaces overlap.

### 6.1 `vacuum` — the machine

| Berry call | Native | Notes |
| --- | --- | --- |
| `vacuum.target(kpa)` | `setTarget` | |
| `vacuum.target_off()` | `clearTarget` | |
| `vacuum.start()` | `start` | bool (local prediction) |
| `vacuum.stop()` | `stop` | stop + vent |
| `vacuum.reset()` | `resetFault` | clears latched fault, then vents |
| `vacuum.state()` | `state` | plant readout as a Berry map |
| `vacuum.host_present()` | `hostPresent` | bool |
| `vacuum.log(text)` | `log` | `[I]` record |
| `vacuum.now_ms()` | `nowMs` | |
| `vacuum.delay(ms)` | `delay` | yields, aborts on stop |

`vacuum.state()` returns a Berry map with keys `kpa`, `vac`, `pct`, `ambient`, `target`,
`target_active`, `duty`, `floor`, `state`, `sensor_ok`, `reading_valid`, `faulted`,
`fault`. Programs format numbers with Berry's built-in `string.format` and `str`; the
firmware exposes no formatter.

### 6.2 `program` — identity and params

| Berry call | Native | Notes |
| --- | --- | --- |
| `program.name()` | `programName` | |
| `program.param(name)` | `param` | typed manifest value |
| `program.state_set(k, v)` | `stateSet` | float or string |
| `program.state_get(k)` | `stateGet` | |

### 6.3 `screen` — drawing

| Berry call | Native | Notes |
| --- | --- | --- |
| `screen.draw_clear()` | `drawClear` | |
| `screen.draw_pixel(x, y)` | `drawPixel` | |
| `screen.draw_line(x0,y0,x1,y1)` | `drawLine` | |
| `screen.draw_hline(x,y,w)` | `drawHLine` | |
| `screen.draw_vline(x,y,h)` | `drawVLine` | |
| `screen.draw_rect(x,y,w,h)` | `drawRect` | |
| `screen.draw_fillrect(x,y,w,h)` | `drawFillRect` | |
| `screen.draw_roundrect(x,y,w,h,r)` | `drawRoundRect` | |
| `screen.draw_fillroundrect(x,y,w,h,r)` | `drawFillRoundRect` | |
| `screen.draw_circle(x,y,r)` | `drawCircle` | |
| `screen.draw_fillcircle(x,y,r)` | `drawFillCircle` | |
| `screen.draw_ellipse(x,y,rw,rh)` | `drawEllipse` | |
| `screen.draw_fillellipse(x,y,rw,rh)` | `drawFillEllipse` | |
| `screen.draw_triangle(x0,y0,x1,y1,x2,y2)` | `drawTriangle` | |
| `screen.draw_filltriangle(x0,y0,x1,y1,x2,y2)` | `drawFillTriangle` | |
| `screen.draw_rotatedrect(cx,cy,w,h,angle)` | `drawRotatedRect` | |
| `screen.draw_fillrotatedrect(cx,cy,w,h,angle)` | `drawFillRotatedRect` | |
| `screen.draw_text(x,y,size,inverted,str)` | `drawText` | |
| `screen.flush()` | `flush` | frame boundary, battery composited |

### 6.4 `buttons` — button callbacks

| Berry call | Notes |
| --- | --- |
The module exposes named constants for the button ids: `buttons.UP` (0), `buttons.DOWN`
(1), `buttons.OK` (2), `buttons.BACK` (3).

| Berry call | Notes |
| --- | --- |
| `buttons.on(button, fn)` | register `fn(kind)`; kind is `"click"`, `"long"`, `"repeat"` |
| `buttons.off(button)` | remove the callback for `button` |
| `buttons.clear()` | remove all callbacks |

`button` is one of the named constants. `buttons.BACK` is device-owned unless claimed
with `buttons.on(buttons.BACK, fn)` (§5).

## 7. Error and stop semantics

- A runtime error (compile failure, Berry exception) sets `lastError()`, publishes it in
  the snapshot (`program.error`), stops the program, and leaves the plant vented via the
  normal stop path.
- `stop()` is idempotent and safe to call when nothing is running.
- `evaluate()` is the optional REPL (`eval <expr>`); it runs on the VM task and returns
  the printed result, capped at ~200 bytes.

## 8. Acceptance criteria

- `hold` and `pulse` example programs run on core 1; `status` keeps answering on core 0.
- `vacuum.state()` returns the plant readout (kpa, state, duty, …) as a Berry map within
  one plant tick.
- `buttons.on(buttons.OK, fn)` fires `fn("click")` when OK is clicked while the program runs.
- A program that calls `vacuum.delay(10)` in a loop can be stopped by Back click within
  one yield interval.
- A program that never yields is stopped by the plant heartbeat and the VM restarts.
- `program.param("cycles")` returns the effective value (persisted over default).
- `screen.draw_text(…)` output appears on the RUN screen within one plant tick of
  `screen.flush()`.
