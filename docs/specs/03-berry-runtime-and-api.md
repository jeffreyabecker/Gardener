# Spec 03 — Berry runtime and program API

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §5.3, §7.6 and
the Berry half of constraint 2.

## 1. Runtime

The firmware depends on Berry directly; there is no runtime abstraction and no
"build without the VM" variant. `BerryRuntime` is the single concrete runtime, owned by
`vmTask` (spec 01 §1).

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
class BerryRuntime {
 public:
  BerryRuntime(ProgramApi& api, Buttons& buttons, ProgramStore& store);

  bool start(const char* programName, const String& source,
             const String& effectiveParams, String& error);
  void stop(const char* reason);      // async; sets stop flag
  bool running() const;
  const char* currentProgram() const;
  const char* lastError() const;
  void update();                      // drain button events → callbacks; on stop: unwind
  bool evaluate(const String& line, String& out);  // optional REPL

 private:
  bvm* vm_ = nullptr;
  ProgramApi& api_;
  Buttons& buttons_;
  ProgramStore& store_;
  volatile bool stopRequested_ = false;
  volatile uint32_t heartbeat_ = 0;
  String currentProgram_;
  String lastError_;
};
```

Start flow:

1. Compile `source` with `be_loadbuffer` (or equivalent). Compile error → `error` set,
   `lastError_` set, return false.
2. Bind the `vacuum`, `program`, `screen`, `buttons`, and `programs` modules (one global
   `be_map`, native functions).
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

Thin, plain C++, no VM in the loop. Every method is a direct call — no queue, no request,
no ack. The Berry bindings in §6 are a thin veneer over it.

```cpp
class ProgramApi {
 public:
  ProgramApi(VacuumSystem& system, Display& display, Buttons& buttons,
             SystemSnapshot& snapshot, ProgramStore& store,
             ProgramParams::Params& params);

  // --- plant (direct calls into VacuumSystem) ---
  void setTarget(float kpa) { system_.setTarget(kpa); }
  void clearTarget()         { system_.clearTarget(); }
  bool start()               { return system_.start(); }
  void stop()                { system_.stop(); }
  void resetFault()          { system_.resetFault(); }
  PlantState state() const   { return system_.state(); }

  // --- programs (store and snapshot, direct) ---
  uint8_t programCount() const;                   // snapshot_.programCount
  const ProgramInfo* programAt(uint8_t i) const;  // snapshot_.programs[i], or null
  bool programParams(const char* name, ProgramParams::Param* out,
                     uint8_t max, uint8_t& count);  // store read + manifest parse
  bool setProgramParam(const char* name, const char* key,
                       const char* value);          // store writes the .params file

  // --- drawing (direct framebuffer writes; no flush — the plant flushes) ---
  void drawClear() { display_.clear(); }
  void drawPixel(int16_t x, int16_t y) { display_.pixel(x, y); }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    display_.line(x0, y0, x1, y1);
  }
  // ... remaining Display verbs pass straight through (hline, vline, rect, fillrect,
  // roundrect, fillroundrect, circle, fillcircle, ellipse, fillellipse, triangle,
  // filltriangle, rotatedrect, fillrotatedrect) ...
  void drawText(int16_t x, int16_t y, uint8_t size, bool inverted, const char* text) {
    display_.text(x, y, text, size, inverted);
  }

  // --- identity / params / state ---
  const char* programName() const;
  bool param(const char* name, float& out) const;      // number/int
  bool param(const char* name, String& out) const;     // choice/bool/text
  void stateSet(const char* key, float value);         // writes snapshot_.state[] directly
  void stateSet(const char* key, const char* value);
  bool stateGet(const char* key, float& out) const;    // reads snapshot_.state[]
  bool stateGet(const char* key, String& out) const;

  // --- misc ---
  bool hostPresent() const { return snapshot_.program.hostAttached; }
  void log(const char* message);
  uint32_t nowMs() const { return millis(); }
  void delay(uint32_t ms);           // yields; aborts on stopRequested

  void setServiceHook(void (*hook)());
  bool stopRequested() const;

 private:
  VacuumSystem& system_;
  Display& display_;
  Buttons& buttons_;
  SystemSnapshot& snapshot_;
  ProgramStore& store_;
  ProgramParams::Params& params_;
  String programName_;
};
```

Rules:

- Everything is a direct call. `state()`, `hostPresent()`, `param()`, `stateGet()`,
  `programCount()`, `programAt()` read the plant's `snapshot_` and the captured `params_`
  and never block.
- `programParams()` and `setProgramParam()` touch `ProgramStore` (LittleFS) directly; this
  is the one shared-filesystem access, accepted in spec 01 §3.
- `stateSet()` writes `snapshot_.state[]` directly; the plant reads it into `status`.
- `programs.run` / `programs.stop` / `programs.home` are program-lifecycle operations
  owned by `BerryRuntime`, not `ProgramApi` (see §3).
- `delay()` yields: it calls `serviceHook()` and checks `stopRequested()` every ~2 ms.

## 5. Button callbacks

Buttons are exposed as callbacks, not polling: registering a callback is owning input
(full API in §6.4). While a program runs, the plant forwards Up, Down, OK — and Back when
claimed — to the VM's button queue (spec 01 §3); `BerryRuntime::update()` drains it and
invokes the matching callbacks.

Back is special: it is device-owned (break-out, spec 05 §4) **unless the program claims
it** by registering `buttons.on(buttons.BACK, fn)`. Claiming Back takes the break-out
gestures; releasing it (`buttons.off(buttons.BACK)` / `buttons.clear()`) returns them to
the device.

Even when claimed, **holding Back for 5 s is always a device hard escape**: the plant
stops the program, vents, and runs `default`. A program cannot consume or override it. A
program that claims Back and stops yielding is likewise stopped and vented by the runaway
watchdog (spec 01 §5).

## 6. Berry binding

Five modules, registered once per VM. Argument order matches the host `draw` ops where
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

There is no `screen.flush()` — the plant pushes the frame each tick (spec 07 §3).

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
with `buttons.on(buttons.BACK, fn)`; holding Back for 5 s is always a device hard escape
(§5).

### 6.5 `programs` — store and launcher

| Berry call | Native | Notes |
| --- | --- | --- |
| `programs.count()` | `programCount` | reads the snapshot |
| `programs.name(i)` | `programAt` | `.name`, or nil |
| `programs.info(i)` | `programAt` | map of name/size/param_count/is_default/running |
| `programs.run(name)` | `BerryRuntime` | stop current → vent → start |
| `programs.run(name, params)` | `BerryRuntime` | params map overrides |
| `programs.stop()` | `BerryRuntime` | stop current → vent → idle |
| `programs.home()` | `BerryRuntime` | stop current → vent → start `default` |
| `programs.params(name)` | `programParams` | list of param maps; reads the store |
| `programs.set_param(name, k, v)` | `setProgramParam` | persist one value |

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
- `screen.draw_text(…)` output appears on the glass within one plant tick (the plant
  flushes).
