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
`HomeScene`, `BrowserScene` and every routine page — `RampedStatic`, `RampedInterval` and
`Interval` — is constructed with, and their chromes lay their children out from it.

A scene opened on top of another is given that scene's box. `ux.box()` answers the `pos` and `dim` of
the scene on top, which — because a scene's `init()` runs before its push — is the scene it is about
to cover. `SettingsEditScene` and `EditScene` take their box that way, so a list and the editor it opens
draw where the scene they were opened from draws.

## The map

Every scene, and where it lives:

| Scene | Defined in | Dispatch |
| --- | --- | --- |
| `HomeScene` — the root | `system/home.be` | `on` |
| `BrowserScene` | `system/browser.be` | `on` |
| `SettingsEditScene` | `system/settings/settings_edit.be` | `handle_page` |
| `EditScene` — the edit base | `system/settings/edit_scene.be` | `handle_page` |
| `BoolEditScene`, `ChoiceEditScene`, `RealEditScene`, `IntEditScene`, `PressureEditScene` | `system/settings/` | inherited |
| `DurationEditScene` | `system/settings/duration.be` | `handle_page` |
| `JingleEditScene` | `system/settings/jingle.be` | inherited |
| `Interval` | `routines/ramped_interval.be` | `on` |
| `RampedInterval` | `routines/ramped_interval.be` | `on` |
| `RampedStatic` | `routines/ramped_static.be` | `on` |
| `Scene` — the base | `system/scene.be` | — |

What each one answers:

| Scene | Gesture | What it does |
| --- | --- | --- |
| `HomeScene` | `ok:single` | start or resume the run |
| `HomeScene` | `cancel:single` | stop and vent |
| `HomeScene` | `forward:double` | stop and vent, then push the routine browser |
| `HomeScene` | `up:double` | open the home scene's settings list — the target, the unit, the jingle and the radio |

| `BrowserScene` | `up:single`, `down:single` | move the selection |
| `BrowserScene` | `forward:single` | open the selected routine's settings, then launch the routine from the set that list settles on |
| `BrowserScene` | `ok:single` | push the selected routine |
| `BrowserScene` | `back:single` | leave |
| `BrowserScene` | `cancel:single` | leave |
| `SettingsEditScene` | `up:single`, `down:single` | move the row |
| `SettingsEditScene` | `forward:single` | open the row's value scene, or nothing for a setting that has no editor |
| `SettingsEditScene` | `back:single` | close the list, handing back the set as it was opened |
| `SettingsEditScene` | `cancel:single` | close the list, handing back the set as it was opened |
| `SettingsEditScene` | `ok:single` | keep the set it was edited to, and close |
| `EditScene` | `up:single`, `down:single` | raise or lower the value |
| `EditScene` | `ok:single` | close, keeping the value |
| `EditScene` | `cancel:single` | close, handing back the value it opened with |
| `BoolEditScene`, `ChoiceEditScene`, `RealEditScene`, `IntEditScene`, `PressureEditScene` | the four `EditScene` gestures | as `EditScene` |
| `ChoiceEditScene` | any other gesture | offered to the choice that is showing, which answers it when it carries that gesture |
| `DurationEditScene` | `forward:single` | advance minutes→seconds|
| `DurationEditScene` | `back:single` | go back seconds→minutes|
| `DurationEditScene` | the other four `EditScene` gestures | as `EditScene` |
| `JingleEditScene` | the four `EditScene` gestures | as `EditScene` |

`ChoiceEditScene` wraps at the ends. Its choices are `ChoiceItem` records — a `ChoiceEditCfg`
normalizes whatever its declaration was written with into them — so Up/Down moves between their keys
and the editor prints each one's title, letting a setting store an identifier and show a name. Which
entry is showing is a `ChoiceRing`: the entries and the index of the one named, stepped with wrap,
rather than a value the editor has to compare against the list. A nullable choice's list carries an
entry whose key is nil and whose title is `--`, at the front — unless the declaration named that
entry itself, which keeps its title and its place. Every entry is then an entry like any other:
drawn and selected as the rest are, wrapped onto at both ends, and handed back when OK is pressed.
The entries are drawn through the `List` the editor holds, which is what puts the highlight on
the entry showing and scrolls the window when the list is longer than the box. A record may also
carry gestures of its own; the editor answers its four first and offers any other gesture to the
choice that is showing, handing it that choice, so an entry can do something the editor knows
nothing about.
`BoolEditScene` is the same ring with two entries — `OFF` and `ON`, with `--` in front of them when the
setting is nullable — drawn as a switch of chips rather than as a list, so Up and Down wrap there as
they do through a choice.
`JingleEditScene` auditions the jingle it opens on and each one it steps to, and goes quiet when
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
3. Every settings list is a `SettingsEditScene`: the home scene's own set and a routine's are opened
   and left the same way. A list that fits draws a centered block the size of its rows, with the
   heading dropped unless it was given a title; a longer list fills the scene's box and scrolls,
   keeping its heading.
4. `EditScene` and everything built on it answer the same gestures; only `DurationEditScene` adds a
   stage. The base frames what the kind measures: `content_size()` is the whole of the editor's
   content — its label row, its body and its hint row — and `draw_page()` draws a border a pixel
   outside that content, centred in the scene's own box with at least a pixel of paper between the
   frame and the edge of the box, then hands `draw_editor(pos, dim)` the content box inside the
   border. The base measures nothing and draws nothing of its own: every kind answers both, and
   draws the whole of its content in that box, chrome included. No label row, hint row or spacing is
   inherited, so a kind's look is its own.
5. A routine page's `ok` is its primary action, `cancel:single` pauses a run under way, and
   `cancel:double` leaves the page.
