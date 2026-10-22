# Scenes — the gestures they bind

This is the contract for what each scene in the image answers. The gesture vocabulary, the presses
the classifier produces and the gestures the device reserves are in [gestures.md](../gestures.md);
how a scene is registered and what the stack does with one is in [routine.md](./routine.md); the
calls a scene makes are in [bindings.md](./bindings.md). This file is only the map: scene, gesture,
what it does.

## How a gesture reaches a scene

The stack drains the gesture queue once per beat and hands the top scene one gesture string —
`ok:short`, `back:long`, `up:short+down:short` — which is `ev` in `handle(ev)` and `handle_page(ev)`.

- **Declarative.** `on(key, presses, fn)` matches the gesture's **first part** only: the part before
  a `+` if there is one. `key` is the button token; `presses` is one press token, a list of them, or
  `nil` for any press on that key. The first registration whose action returns true wins.
- **Hand-rolled.** A scene that defines `handle_page(ev)` compares the string itself. In the shipped
  scenes it does so by key — `string.startswith(ev, "up:")` — which is the same match as `on(key,
  nil, fn)`.
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
| `ReadoutScene` | `ok:short` | start or resume the run |
| `ReadoutScene` | `cancel:short` | stop and vent |
| `ReadoutScene` | `up:double` | push the routine browser |
| `ReadoutScene` | `down:double` | push the system settings |
| `BrowserScene` | `up:short`, `down:short` | move the selection |
| `BrowserScene` | `ok:double` | open the selected routine's settings & push the selected scene with those settings on return |
| `BrowserScene` | `ok:short` | push the selected routine |
| `BrowserScene` | `back:short` | leave |
| `SystemScene` | `up:short`, `down:short` | move the row |
| `SystemScene` | `ok:short` | open the row |
| `SystemScene` | `back:short` | leave |
| `SettingsScene` | `up:short`, `down:short` | move the row |
| `SettingsScene` | `ok:short` | open the row |
| `SettingsScene` | `back:short` | close the list |
| `ValueScene` | `up:short`, `down:short` | raise or lower the value |
| `ValueScene` | `ok:short` | close, keeping the value |
| `ValueScene` | `cancel:short` | close, discarding it |
| `BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene` | the four `ValueScene` gestures | as `ValueScene` |
| `DurationScene` | `forward:short` | advance minutes→seconds|
| `DurationScene` | `back:short` | go back seconds→minutes|
| `DurationScene` | the other four `ValueScene` gestures | as `ValueScene` |
| `UnitScene`, `JingleScene` | the four `ValueScene` gestures | as `ValueScene` |
| `IntervalSettings`, `RampedIntervalSettings`, `RampedStaticSettings` | the four `SettingsScene` gestures | as `SettingsScene` |

| `Interval` | `up:double` | stop and vent, reset the run, open its settings |
| `Interval` | `ok:short` | begin, while the run is ready, paused or waiting |
| `Interval` | `cancel:short` | stop and vent, pause the run |
| `Interval` | `cancel:double` | stop and vent, pop off the stack |
| `RampedInterval` | `up:double` | stop and vent, reset the run, open its settings |
| `RampedInterval` | `ok:short` | begin, while the run is ready, paused or waiting |
| `RampedInterval` | `cancel:short` | stop and vent, pause the run |
| `RampedInterval` | `cancel:double` | stop and vent, pop off the stack |
| `RampedStatic` | `up:double` | stop and vent, reset the run, open its settings |
| `RampedStatic` | `ok:short` | begin, while the run is ready, paused or waiting |
| `RampedStatic` | `cancel:short` | stop and vent, pause the run |
| `RampedStatic` | `cancel:double` | stop and vent, pop off the stack |
| `Page`, `Scene` | none | the bases answer nothing |

`key:*` and `*:chord` are not a valid gesture identifiers. 

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
