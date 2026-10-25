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

## The box a scene draws in

Every scene carries a box — `pos` and `dim`, as a component does. `Scene.draw()` fills that box with
paper and then calls `draw_page()`, so a scene leaves whatever the scene below it drew outside the
box alone rather than clearing the panel.

A scene the stack is rooted on is the page: `Position.top_left()` and `Size.page()`. That is the box
`ReadoutScene`, `BrowserScene` and every routine page — `RampedStatic`, `RampedInterval` and
`Interval` — is constructed with, and their chromes lay their children out from it.

A scene opened on top of another is given that scene's box. `ux.box()` answers the `pos` and `dim` of
the scene on top, which — because a scene's `init()` runs before its push — is the scene it is about
to cover. `SettingsScene` and `ValueScene` take their box that way, so a list and the editor it opens
draw where the scene they were opened from draws.

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
| `ChoiceScene` | any other gesture | offered to the choice that is showing, which answers it when it carries that gesture |
| `DurationScene` | `forward:single` | advance minutes→seconds|
| `DurationScene` | `back:single` | go back seconds→minutes|
| `DurationScene` | the other four `ValueScene` gestures | as `ValueScene` |
| `JingleScene` | the four `ValueScene` gestures | as `ValueScene` |

`ChoiceScene` wraps at the ends. Its choices are `ChoiceItem` records — a `ChoiceSettingCfg`
normalizes whatever its declaration was written with into them — so Up/Down moves between their keys
and the editor prints each one's title, letting a setting store an identifier and show a name. A
record may also carry gestures of its own; the editor answers its four first and offers any other
gesture to the choice that is showing, handing it that choice, so an entry can do something the
editor knows nothing about.
`JingleScene` auditions the jingle it opens on and each one it steps to, and goes quiet when
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
   left the same way. A list that fits draws a centered block the size of its rows, with the heading
   dropped; a longer list fills the scene's box and scrolls, keeping its heading.
4. `ValueScene` and everything built on it answer the same gestures; only `DurationScene` adds a
   stage. The base frames what the kind measures: `content_size()` is the whole of the editor's
   content — its label row, its body and its hint row — and `draw_page()` draws a border a pixel
   outside that content, centred in the scene's own box with at least a pixel of paper between the
   frame and the edge of the box, then hands `draw_editor(pos, dim)` the content box inside the
   border. The base measures nothing and draws nothing of its own: every kind answers both, and
   draws the whole of its content in that box, chrome included. No label row, hint row or spacing is
   inherited, so a kind's look is its own.
5. A routine page's `ok` is its primary action, `cancel:single` pauses a run under way, and
   `cancel:double` leaves the page.
