# Spec 04 — On-device UX: screen ownership and buttons

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §7.

Programs own the screen. The device owns only the battery overlay and the two Back
gestures. There is no device-drawn UI layer; anything reusable is a Berry program.

## 1. Screen ownership

- A running program draws the entire panel with the `screen.draw_*` verbs and pushes the
  frame with `screen.flush()`. The device composites only the battery (`AppDisplay`) on
  every flush, so a program can never erase it.
- There are no device-drawn pages — no browser, params editor, or status page in C++.
  Those are Berry programs (spec 02 §4). A reusable list/menu abstraction, if one is
  wanted, is written in Berry, not firmware.

## 2. `hal/Buttons`

Four active-low buttons with internal pull-ups, pins from
[Pins.h](../../libraries/VacuumCore/src/hal/Pins.h) (UP=1, DOWN=2, OK=10, BACK=11).

```cpp
enum class ButtonId : uint8_t { Up = 0, Down = 1, Ok = 2, Back = 3 };
constexpr uint8_t kButtonCount = 4;  // sizes the per-button state arrays, not a button
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

## 3. Button routing

- While a program runs and no host is attached, Up, Down and OK events are delivered to
  the program's `buttons.on` callbacks (spec 03 §5).
- Back is delivered to the program **only when it claims it** with
  `buttons.on(buttons.BACK, fn)`;
  otherwise the device owns it:

| Gesture | Effect |
| --- | --- |
| Back click (unclaimed) | `programs.home()` — stop current, vent, run `default` |
| Back long-press (unclaimed) | `programs.run("browser")` — stop current, vent, run `browser` |

- While a host is attached, all buttons are ignored — the panel is host-drawn (§5).

## 4. Reserved programs

- `default` — the boot and home program. Seeded as a passive readout; an owner can
  replace it (spec 02 §4).
- `browser` — the program selector and params editor, also seeded. Reached by Back
  long-press.

They are ordinary stored programs to the store and runtime; only the names are reserved
by boot and the two Back gestures.

## 5. Display mode switch

- **No host** → the running program draws (local mode).
- **Host attaches** → the panel becomes host-drawn exactly as today (serial `draw`/
  `flush`); the program's drawing is suppressed and buttons are ignored.
- **Host detaches** → the running program draws again.

## 6. Acceptance criteria

- A program's frame reaches the glass with only the battery composited on top.
- Back click homes to `default` and Back long runs `browser`, from any program that has
  not claimed Back.
- Up/Down/OK reach the program's `buttons.on` callbacks and are never consumed by the
  device.
- Host attach/detach swaps between host-drawn and program-drawn without losing local
  state.
