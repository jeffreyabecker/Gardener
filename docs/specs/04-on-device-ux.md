# Spec 04 — On-device UX: buttons, browser, params, run screen

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §7 and
constraints 2 (parameter UX) and 3 (buttons + browser).

## 1. `hal/Buttons`

Four active-low buttons with internal pull-ups, pins from
[Pins.h](../../libraries/VacuumCore/src/hal/Pins.h) (UP=1, DOWN=2, OK=10, BACK=11).

```cpp
enum class ButtonId : uint8_t { Up = 0, Down = 1, Ok = 2, Back = 3, Count = 4 };
enum class ButtonEventKind : uint8_t { Click, LongPress, Repeat };

struct ButtonEvent {
  ButtonId id;
  ButtonEventKind kind;
  uint32_t heldMs;
};

class Buttons {
 public:
  void begin();
  void update(uint32_t nowMs);       // called by plantTask every tick
  bool isDown(ButtonId id) const;
  uint32_t heldMs(ButtonId id) const;
  bool popEvent(ButtonEvent& out);   // plant consumes for the UI or forwards to the VM
 private:
  // debounce, long-press and repeat state per pin (port of the f35a3bb driver)
};
```

Timings:

| Event | Threshold |
| --- | --- |
| click | press shorter than `longPressMs` |
| long-press | held ≥ 800 ms |
| repeat | every 200 ms after the long-press, while held |

Events are queued (depth 16) so a press during a busy tick is not lost.

## 2. Screens and state machine

Three screens, one `ViewState` owned by `UiController`:

```cpp
enum class Screen : uint8_t { Run, Browser, Params };

struct ViewState {
  Screen screen = Screen::Run;
  size_t cursor = 0;          // browser: selected program; params: selected param
  size_t listTop = 0;         // browser: first visible row
};
```

`UiController` turns button events into intents, and the plant task applies them:

```cpp
enum class UiIntent : uint8_t { None, Home, OpenBrowser, RunSelected,
                                EditSelected, MoveCursor, AdjustValue, NextParam,
                                CommitRun };
```

## 3. Button grammar

Back is always device-owned. The two break-out gestures (spec 05 §4) are global:

- **Back click** → `Home`: stop program (if any), vent, load + start `default`.
- **Back long-press** → `OpenBrowser`: stop program (if any), vent, open the browser
  with the cursor on `default`.

| Gesture | RUN | BROWSER | PARAMS |
| --- | --- | --- | --- |
| Up / Down click | program input if owned; else no-op | `MoveCursor` (wraps) | `AdjustValue` (one step) |
| Up / Down repeat | program input if owned; else no-op | fast scroll | `AdjustValue` (×10) |
| OK click | program input if owned; else no-op | `RunSelected` | `NextParam` (wraps) |
| OK long-press | program input if owned; else no-op | `EditSelected` | `CommitRun` (persist + run) |

`Back` in BROWSER and PARAMS also homes to `default`; the long-press there re-initializes
the browser. Programs may claim Up/Down/OK via input ownership (spec 03 §5) but can never
claim Back.

## 4. Rendering

`Renderer` draws through `hal/Display`, never through `AppDisplay` directly (the battery
is composited by `App` on flush):

```cpp
class Renderer {
 public:
  explicit Renderer(Display& display);
  void header(const char* title, const char* right);
  void row(uint8_t row, const char* text, bool inverted);
  void value(const char* text);          // large (size 2), centered
  void hint(const char* text);           // small, left
  void footer(const char* text);
  void bar(float fraction);
};
```

The RUN screen composites: device header (program name + state) → the program's five
`ProgramOverlay` lines → optional bar → footer. `AppDisplay::flush()` puts the battery on
last, so a program can never erase it.

## 5. Browser layout

128x64, built-in font size 1:

```
PROGRAMS 3/12
* default
  pulse
> hold
  cycle
OK run  OK^ edit  BACK home
```

- Row 0: header, `PROGRAMS <cursor+1>/<count>`.
- Rows 1–5: five program names (truncated to 18 chars). `*` suffix on `default`, `R`
  suffix on the running program, cursor row inverted.
- Row 6: footer hints.
- Scrolling: the window slides so the cursor stays visible; wraps at both ends.
- Order: alphabetical with `default` pinned first (store does this, spec 02 §3).

## 6. Break-out paths (from a running program)

- **Vent and return to the default program** — Back click. Stop current program, vent,
  load + start `default`. One press to the known-good home screen.
- **Vent and initialize the program selector** — Back long-press. Stop current program,
  vent, open BROWSER with nothing running and the cursor on `default`.

Both leave the chamber released before anything new can act, because every program change
vents first.

## 7. Params editor layout

One parameter per screen:

```
TARGET 2/3
Target absolute pressure
  40.0 kPa
min 5  max 150  step 0.5
OK next  OK^ run  BACK back
```

- Row 0: header, parameter name (uppercased) + index/total.
- Row 1: description (truncated to one line).
- Rows 2–3: value, size 2, inverted while active, with `unit` appended.
- Row 4: range line — `min/max/step` for numbers, the choices for `choice`, on/off for
  `bool`.
- Row 5: footer hints.

Interactions: Up/Down step the value (repeat ×10 for numbers, cycles for choice/bool);
OK advances to the next parameter; OK long-press persists and runs; Back discards and
homes. A program with no declared params skips PARAMS (OK long-press in BROWSER shows
"no params", OK click runs directly).

## 8. Display mode switch

- No host → **Local mode**: the UI owns the glass.
- Host attaches → **Host mode**: the panel is host-drawn exactly as today; local
  rendering suspends. Buttons pressed while attached are ignored.
- Host detaches → back to **Local mode**, showing the RUN screen of whatever is running
  (or `default` after the home gesture).

## 9. Toasts

`vacuum.toast(text, ms)` and device toasts (`"running pulse"`, `"stopped"`, `"read
failed"`) render centered, size 2, over the current frame for their duration, then the
frame is redrawn.

## 10. Acceptance criteria

- From boot (default running), Back long → browser → Up/Down → OK runs a program in
  three button presses.
- Back click from any running program vents and lands on `default` within one plant tick.
- A program that claims Up/Down/OK still responds to Back click and Back long.
- Cursor wraps, fast-scrolls, and the window keeps the cursor visible at the list ends.
- Host attach swaps to host-drawn panel and back without losing local state.
