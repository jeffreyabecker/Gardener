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
| `BrowserScene` | `modules/browser.be` | `on` |
| `SettingsScene` | `system/settings/scenes/settings_scene.be` | `handle_page` |
| `ValueScene` — the value base | `system/settings/scenes/value_scene.be` | `handle_page` |
| `BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene` | `system/settings/scenes/` | inherited |
| `DurationScene` | `system/settings/scenes/duration_scene.be` | `handle_page` |
| `JingleScene` | `modules/ui.be` | inherited |
| `Interval` | `routines/ramped_interval.be` | `on` |
| `RampedInterval` | `routines/ramped_interval.be` | `on` |
| `RampedStatic` | `routines/ramped_static.be` | `on` |
| `Scene` — the base | `system/scene.be` | — |

What each one answers:

| Scene | Gesture | What it does |
| --- | --- | --- |
| `ReadoutScene` | `ok:single` | start or resume the run |
| `ReadoutScene` | `cancel:single` | stop and vent |
| `ReadoutScene` | `forward:double` | stop and vent, then push the routine browser |
| `ReadoutScene` | `up:double` | open the readout's settings list — the target, the unit, the jingle and the radio |

| `BrowserScene` | `up:single`, `down:single` | move the selection |
| `BrowserScene` | `forward:single` | open the selected routine's settings  |
| `BrowserScene` | `ok:single` | push the selected routine |
| `BrowserScene` | `back:single` | leave |
| `BrowserScene` | `cancel:single` | leave |
| `SettingsScene` | `up:single`, `down:single` | move the row |
| `SettingsScene` | `forward:single` | open the row's value scene, or nothing for a setting that has no editor |
| `SettingsScene` | `back:single` | close the list, handing back the set as it was opened |
| `SettingsScene` | `cancel:single` | close the list, handing back the set as it was opened |
| `SettingsScene` | `ok:single` | keep the set it was edited to, and close |
| `ValueScene` | `up:single`, `down:single` | raise or lower the value |
| `ValueScene` | `ok:single` | close, keeping the value |
| `ValueScene` | `cancel:single` | close, discarding it |
| `BoolScene`, `ChoiceScene`, `RealScene`, `IntScene`, `PressureScene` | the four `ValueScene` gestures | as `ValueScene` |
| `DurationScene` | `forward:single` | advance minutes→seconds|
| `DurationScene` | `back:single` | go back seconds→minutes|
| `DurationScene` | the other four `ValueScene` gestures | as `ValueScene` |
| `JingleScene` | the four `ValueScene` gestures | as `ValueScene` |

`ChoiceScene` wraps at the ends. A choice is a value, or a `[value, title]` pair: Up/Down moves
between values and the editor prints each one's title, so a setting can store an identifier and show
a name. `JingleScene` auditions the jingle it opens on and each one it steps to, and goes quiet when
it is cancelled.

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
| `Scene` | none | the base answers nothing |

## Rules

1. A scene answers only what the table gives it; everything else is dropped.
2. The reserved gestures in [gestures.md](../gestures.md) are taken before the scene sees them, so
   no binding here may claim them.
3. Every settings list is a `SettingsScene`: the readout's own set and a routine's are opened and
   left the same way.
4. `ValueScene` and everything built on it answer the same gestures; only `DurationScene` adds a
   stage.
5. A routine page's `ok` is its primary action, `cancel:single` pauses a run under way, and
   `cancel:double` leaves the page.
