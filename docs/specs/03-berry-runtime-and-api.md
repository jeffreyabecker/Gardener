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
  void update() override;             // if stopRequested: be_stop / unwind
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
2. Bind the `vacuum` and `program` modules (one global `be_map`, native functions).
3. Set `program.name`, `program.args`, and the effective params.
4. Run the top-level call. `start()` returns as soon as the program begins; the VM task
   keeps it alive via `update()`.

Stop flow:

1. `stop()` sets `stopRequested_ = true`.
2. The program observes it at the next yield (`vacuum.delay`, `vacuum.button_wait`, or
   the execution-limit hook) and unwinds; `update()` notices and finalizes.
3. If the heartbeat shows the program is not yielding (spec 01 §5), the plant asks `App`
   to delete and recreate `vmTask`, which abandons the VM.

## 4. Native surface: `ProgramApi`

Thin, plain C++, no VM in the loop. The same behaviour is reachable from the serial
command path, the UI, and (later) tests.

```cpp
class ProgramApi {
 public:
  ProgramApi(VacuumSystem& system, Buttons& buttons,
             ProgramParams::Context& params);

  // --- plant (commands are enqueued; the one read is from the local snapshot) ---
  void setTarget(float kpa);
  void clearTarget();
  bool start();                      // false when snapshot says not ready
  void stop();                       // stop + vent
  void resetFault();
  bool setAmbient(float kpa);        // false when out of range
  void captureAmbient();
  PlantState state() const;          // the whole plant readout

  // --- input ---
  bool buttonDown(int index) const;        // 0=up 1=down 2=ok 3=back
  int buttonWait(int index, int timeoutMs); // 0=timeout 1=click 2=long 3=repeat

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
  const char* arg(int index) const;
  String args() const;
  bool param(const char* name, float& out) const;      // number/int
  bool param(const char* name, String& out) const;     // choice/bool/text
  void stateSet(const char* key, float value);
  void stateSet(const char* key, const char* value);
  bool stateGet(const char* key, float& out) const;
  bool stateGet(const char* key, String& out) const;

  // --- misc ---
  bool hostPresent() const;
  String fmt(float value, int decimals) const;
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
- Writes (`setTarget`, `clearTarget`, `start`, `stop`, `resetFault`, `setAmbient`,
  `captureAmbient`) enqueue a `Command` to the plant and return immediately. `start()`
  returns the local prediction from the snapshot and may disagree with the plant's
  authoritative decision; programs that care poll `state()`.
- `delay()` and `buttonWait()` yield: they call `serviceHook()` (which services the
  snapshot copy) and check `stopRequested()` every ~2 ms.
- `buttonWait` requires input ownership (next section).

## 5. Input ownership

- A program owns input from the moment it calls `buttonDown`, `buttonWait`, or an explicit
  `vacuum.input(true)`; it can release it with `vacuum.input(false)`.
- While owned, Up, Down and OK events flow from the plant's button queue to the VM.
  **Back is never delivered** — it always belongs to the device (break-out, spec 04 §6).
- If a program never owns input, Up/Down/OK are no-ops and the RUN screen is purely a
  readout.

## 6. Berry binding

Three modules, registered once per VM:

- `vacuum` — the machine (the table below).
- `program` — identity and params.
- `screen` — drawing (the `screen` table below).

| Berry call | Native | Notes |
| --- | --- | --- |
| `vacuum.target(kpa)` | `setTarget` | |
| `vacuum.target_off()` | `clearTarget` | |
| `vacuum.start()` | `start` | bool (local prediction) |
| `vacuum.stop()` | `stop` | stop + vent |
| `vacuum.reset()` | `resetFault` | clears latched fault, then vents |
| `vacuum.set_ambient(kpa)` | `setAmbient` | false when out of range |
| `vacuum.capture_ambient()` | `captureAmbient` | |
| `vacuum.state()` | `state` | plant readout as a Berry map |
| `vacuum.button_down(i)` | `buttonDown` | claims input |
| `vacuum.button_wait(i, ms)` | `buttonWait` | 0/1/2/3; claims input |
| `vacuum.input(on)` | — | explicit ownership |
| `vacuum.host_present()` | `hostPresent` | bool |
| `vacuum.log(text)` | `log` | `[I]` record |
| `vacuum.fmt(v, decimals)` | `fmt` | String |
| `vacuum.now_ms()` | `nowMs` | |
| `vacuum.delay(ms)` | `delay` | yields, aborts on stop |
| `program.name()` | `programName` | |
| `program.arg(n)` | `arg` | raw CLI arg |
| `program.args()` | `args` | |
| `program.param(name)` | `param` | typed manifest value |
| `program.state_set(k, v)` | `stateSet` | float or string |
| `program.state_get(k)` | `stateGet` | |

`vacuum.state()` returns a Berry map with keys `kpa`, `vac`, `pct`, `ambient`, `target`,
`target_active`, `duty`, `floor`, `state`, `sensor_ok`, `reading_valid`, `faulted`,
`fault`.

The `screen` module:

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

The argument order matches the host `draw` ops, so a program and a host draw the same
shapes the same way.

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
- A program that calls `vacuum.delay(10)` in a loop can be stopped by Back click within
  one yield interval.
- A program that never yields is stopped by the plant heartbeat and the VM restarts.
- `program.param("cycles")` returns the effective value (persisted over default).
- `screen.draw_text(…)` output appears on the RUN screen within one plant tick of
  `screen.flush()`.
