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

| Scene | Defined in | Dispatch | Gesture | What it does |
| --- | --- | --- | --- | --- |
| `ReadoutScene` — the root | `modules/ui.be` | `on` | `up:chord` | push the system settings |
| | | | `ok:short` | start or resume the run |
| | | | `back:short`, `back:double` | stop and vent |
| | | | `up:double` | push the routine browser |
| | | | `down:double` | push the system settings |
| `BrowserScene` | `modules/ui.be` | `on` | `up:*`, `down:*` | move the selection |
| | | | `up:chord` | open the selected routine's settings |
| | | | `ok:*` | push the selected routine |
| | | | `back:*` | leave |
| `SystemScene` | `modules/ui.be` | `handle_page` | `up:*`, `down:*` | move the row |
| | | | `ok:*` | open the row |
| | | | `back:*` | leave |
| `SettingsScene` | `modules/settings_page.be` | `handle_page` | `up:*`, `down:*` | move the row |
| | | | `ok:*` | open the row |
| | | | `back:*` | close the list |
| `ValueScene` | `modules/settings_edit.be` | `handle_page` | `up:*`, `down:*` | raise or lower the value |
| | | | `ok:*` | close, keeping the value |
| | | | `back:*` | close, discarding it |
| `BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene` | `modules/settings_edit.be` | inherited | the four `ValueScene` gestures | as `ValueScene` |
| `DurationScene` | `modules/settings_edit.be` | `handle_page` | `ok:*` | advance minutes→seconds, then close keeping the value |
| | | inherited | the other three `ValueScene` gestures | as `ValueScene` |
| `UnitScene`, `JingleScene` | `modules/ui.be` | inherited | the four `ValueScene` gestures | as `ValueScene` |
| `Interval` | `routines/interval.be` | `on` | `up:chord` | open its settings |
| | | | `ok:*` | begin, while the run is ready or waiting |
| `RampedInterval` | `routines/ramped_interval.be` | `on` | `up:chord` | open its settings |
| | | | `ok:*` | start or resume the run |
| | | | `back:short` | pause the run |
| `RampedStatic` | `routines/ramped_static.be` | `on` | `up:chord` | open its settings |
| | | | `ok:*` | start or resume the run |
| | | | `back:short` | pause the run |
| `IntervalSettings`, `RampedIntervalSettings`, `RampedStaticSettings` | the three routine files | inherited | the four `SettingsScene` gestures | as `SettingsScene` |
| `Page` — the routine base | `modules/settings_page.be` | — | none | owns the settings view, not a gesture |
| `Scene` — the base | `modules/scene.be` | — | none | drains the queue and dispatches |

`key:*` is any press on that key. `chord` is the settings gesture, Up and Down pressed together, and
a scene registers it on the up key.

## Rules

1. A scene answers only what the table gives it; everything else is dropped.
2. The reserved gestures in [gestures.md](../gestures.md) are taken before the scene sees them, so
   no binding here may claim them.
3. `SettingsScene` and `SystemScene` answer the same four gestures; a row and a setting are opened
   and left the same way in both.
4. `ValueScene` and everything built on it answer the same four; only `DurationScene` adds a stage.
5. A routine page's `ok` is its primary action, and its `back` is its own — the two ramped routines
   pause on it, `Interval` has none.
