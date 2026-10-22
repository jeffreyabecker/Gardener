# Scenes — the gestures they bind

This is the contract for what each scene in the image answers. The gesture vocabulary, the presses
the classifier produces and the gestures the device reserves are in [gestures.md](../gestures.md);
how a scene is registered and what the stack does with one is in [routine.md](./routine.md); the
calls a scene makes are in [bindings.md](./bindings.md). This file is only the map: scene, gesture,
what it does.

## How a gesture reaches a scene

The stack drains the gesture queue once per beat and hands the top scene one gesture string —
`ok:single`, `back:3`, `up:double` — which is `ev` in `handle(ev)` and `handle_page(ev)`.

- A scene matches a gesture **whole or not at all**. It compares the string it is given with the
  gestures it knows, and the first match whose action returns true wins.
- The declarative form is `on(gesture, fn)`: one whole gesture string per registration. There is no
  `*`, no bare key and no list of presses — each of those matches one part of a gesture without the
  other, which no scene may do.
- A gesture no binding matches is dropped. A scene never sees the device's reserved gestures, which
  are taken before the queue.

## The map

Every scene, and where it lives:

| Scene | Defined in | Dispatch |
| --- | --- | --- |
| `ReadoutScene` — the root | `modules/ui.be` | `on` |
| `BrowserScene` | `modules/ui.be` | `on` |
| `SystemScene` | `modules/ui.be` | `handle_page` |
| `SettingsScene` | `modules/settings_page.be` | `handle_page` |
| `ValueScene` — the value base | `modules/settings_edit.be` | `handle_page` |
| `BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene` | `modules/settings_edit.be` | inherited |
| `DurationScene` | `modules/settings_edit.be` | `handle_page` |
| `UnitScene`, `JingleScene` | `modules/ui.be` | inherited |
| `Interval` | `routines/interval.be` | `on` |
| `RampedInterval` | `routines/ramped_interval.be` | `on` |
| `RampedStatic` | `routines/ramped_static.be` | `on` |
| `IntervalSettings`, `RampedIntervalSettings`, `RampedStaticSettings` | the three routine files | inherited |
| `Page` — the routine base | `modules/settings_page.be` | — |
| `Scene` — the base | `modules/scene.be` | — |

What each one answers:

| Scene | Gesture | What it does |
| --- | --- | --- |
| `ReadoutScene` | `ok:single` | start or resume the run |
| `ReadoutScene` | `cancel:single` | stop and vent |
| `ReadoutScene` | `up:double` | push the routine browser |
| `ReadoutScene` | `down:double` | push the system settings |
| `BrowserScene` | `up:single`, `down:single` | move the selection |
| `BrowserScene` | `forward:single` | open the selected routine's settings  |
| `BrowserScene` | `ok:single` | push the selected routine |
| `BrowserScene` | `back:single` | leave |
| `SystemScene` | `up:single`, `down:single` | move the row |
| `SystemScene` | `ok:single` | open the row |
| `SystemScene` | `back:single` | leave |
| `SettingsScene` | `up:single`, `down:single` | move the row |
| `SettingsScene` | `ok:single` | open the row |
| `SettingsScene` | `back:single` | close the list |
| `ValueScene` | `up:single`, `down:single` | raise or lower the value |
| `ValueScene` | `ok:single` | close, keeping the value |
| `ValueScene` | `cancel:single` | close, discarding it |
| `BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene` | the four `ValueScene` gestures | as `ValueScene` |
| `DurationScene` | `forward:single` | advance minutes→seconds|
| `DurationScene` | `back:single` | go back seconds→minutes|
| `DurationScene` | the other four `ValueScene` gestures | as `ValueScene` |
| `UnitScene`, `JingleScene` | the four `ValueScene` gestures | as `ValueScene` |
| `IntervalSettings`, `RampedIntervalSettings`, `RampedStaticSettings` | the four `SettingsScene` gestures | as `SettingsScene` |

| `Interval` | `up:double` | stop and vent, reset the run, open its settings |
| `Interval` | `ok:single` | begin, while the run is ready, paused or waiting |
| `Interval` | `cancel:single` | stop and vent, pause the run |
| `Interval` | `cancel:double` | stop and vent, pop off the stack |
| `RampedInterval` | `up:double` | stop and vent, reset the run, open its settings |
| `RampedInterval` | `ok:single` | begin, while the run is ready, paused or waiting |
| `RampedInterval` | `cancel:single` | stop and vent, pause the run |
| `RampedInterval` | `cancel:double` | stop and vent, pop off the stack |
| `RampedStatic` | `up:double` | stop and vent, reset the run, open its settings |
| `RampedStatic` | `ok:single` | begin, while the run is ready, paused or waiting |
| `RampedStatic` | `cancel:single` | stop and vent, pause the run |
| `RampedStatic` | `cancel:double` | stop and vent, pop off the stack |
| `Page`, `Scene` | none | the bases answer nothing |

## Rules

1. A scene answers only what the table gives it; everything else is dropped.
2. The reserved gestures in [gestures.md](../gestures.md) are taken before the scene sees them, so
   no binding here may claim them.
3. `SettingsScene` and `SystemScene` answer the same gestures; a row and a setting are opened and
   left the same way in both.
4. `ValueScene` and everything built on it answer the same gestures; only `DurationScene` adds a
   stage.
5. A routine page's `ok` is its primary action, and its `back` is its own — the two ramped routines
   pause on it, `Interval` has none.
