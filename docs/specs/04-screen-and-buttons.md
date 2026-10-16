# Spec 04 — Screen and button ownership

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §7.

The device draws nothing and interprets almost nothing. A running program owns the panel
and owns the buttons; the firmware keeps two things for itself — the battery overlay it
composites into every frame, and one abort gesture.

## 1. Screen ownership

- A running program draws the entire panel with the `screen.draw_*` verbs and pushes with
  `screen.flush()`. The device composites only the battery (`Overlay`) on every flush,
  so a program can never erase it.
- There are no device-drawn pages — no browser, no params editor, no status page in C++.
  Those are Berry programs (spec 02 §4). A reusable list/menu abstraction, if one is
  wanted, is written in Berry, not firmware.
- Only the active controller draws: the program in stand-alone mode, the serial line in
  peripheral mode. The two modes are mutually exclusive, so there is nothing to arbitrate
  and no device-side drawing path at all (spec 01 §3).

## 2. Button ownership

- In stand-alone mode the button scan queues every event — Up, Down, OK and Back, all three
  kinds — and the running program drains them into its `buttons.on` callbacks (spec 03 §5).
  The device filters nothing, so Back is an ordinary button: its clicks, long-presses and
  repeats mean whatever the program says they mean.
- The one exception is holding Back for 5 s, which the firmware does for itself (spec 05 §4):

| Gesture | Effect |
| --- | --- |
| Back hold 5 s | abort: `programs.home()` — stop current, vent, run `default` |

- The threshold is a condition on how long the pin is held, tested by the service pass, not
  an event, so it cannot be swallowed: a program that registers `buttons.on(buttons.BACK, fn)`
  still gets that press's Back events, and is aborted anyway. A program that never comes up
  for air is aborted at the hook (spec 01 §5).
- The abort is the only action the device takes on its own initiative, and `default` is
  the only name the firmware uses — boot starts it and the abort returns to it (spec 02
  §4). No gesture opens the selector: `browser` is launched like any other program, with
  `programs.run("browser")` or `run browser` over the wire.
- In peripheral mode there is no program to route to: the VM is stopped, so button events
  are never delivered anywhere (spec 01 §2).

## 3. `hal/Buttons`

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
  void update(uint32_t nowMs);       // called by the service pass every tick
  bool isDown(ButtonId id) const;
  uint32_t heldMs(ButtonId id) const;
  bool popEvent(ButtonEvent& out);   // the running program drains this (spec 03 §5)
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

Events are queued (depth 16) so a press during a busy tick is not lost; `popEvent()` is the
routing side of §2.

## 4. Acceptance criteria

- A program's frame reaches the glass with only the battery composited on top.
- Up/Down/OK/Back reach the program's `buttons.on` callbacks, for every kind, and are
  never consumed by the device.
- Holding Back for 5 s aborts any program: it stops, the chamber vents, and `default`
  runs — whatever the program registered.
- In peripheral mode the buttons do nothing.
