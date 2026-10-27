# Scenes — the gestures they bind

This is the contract for what each scene in the image answers. The gesture vocabulary, the presses
the classifier produces and the gestures the device reserves are in [gestures.md](../gestures.md);
how a scene is registered and what the stack does with one is in [routine.md](./routine.md); the
calls a scene makes are in [bindings.md](./bindings.md). This file is only the map: scene, gesture,
what it does.

## How a gesture reaches a scene

The stack drains the gesture queue once per beat and hands the top scene one gesture string —
`ok:single`, `back:3`, `up:double` — which is `ev` in `handle(ev)`.

- A scene answers in `handle(ev)`: it compares the string it is given with the gestures it knows and
  returns true for the one it took. The match is **whole or not at all**, so there is no `*`, no
  bare key and no list of presses — each of those matches one part of a gesture without the other,
  which no scene may do.
- A gesture no scene answers is dropped. A scene never sees the device's reserved gestures, which
  are taken before the queue.
- `Scene.handle(ev)` answers nothing, so a scene that knows no keys overrides nothing.

## The box a scene draws in

Every scene draws in the page. It carries a box — `pos` and `dim`, as a component does — and `ux`
asks the scene for it with `default_box()` and stamps it on the scene as it goes on the stack, before
its `activate()` runs. `Scene.default_box()` answers `point(0, 0)` and the page's `bounds`, the panel
less the band, and no scene overrides it, so a scene is never a window inside another one. A page
composed in a `Layout` hands the same box on to it. `Scene.draw()` fills that box with paper and then
calls `draw_page()`, and a scene opened on top of another covers the page and clears it; the scene
below keeps its state and redraws when it is back on top.

That box is the one `HomeScene`, `BrowserScene`, every routine page — `RampedStatic`,
`RampedInterval` and `Interval` — and every settings list and value editor lays its content out in.

## The map

Every scene, and where it lives:

| Scene | Defined in |
| --- | --- |
| `HomeScene` — the root | `system/home.be` |
| `BrowserScene` | `system/browser.be` |
| `SettingsEditScene` | `system/settings/settings_edit.be` |
| `EditScene` — the value editor every kind extends, and the one a number is edited in | `system/settings/edit_scene.be` |
| `BoolEditScene`, `ChoiceEditScene`, `PressureEditScene` | `system/settings/` |
| `DurationEditScene` | `system/settings/duration.be` |
| `JingleEditScene` | `system/settings/jingle.be` |
| `Interval` | `routines/3_interval.be` |
| `RampedInterval` | `routines/2_ramped_interval.be` |
| `RampedStatic` | `routines/5_ramped_static.be` |
| `Scene` — the base | `system/scene.be` |

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
| `BoolEditScene`, `ChoiceEditScene`, `PressureEditScene` | the four `EditScene` gestures | as `EditScene` |
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
A setting with no values at all — the jingle row on an image that ships no library — is still a row:
its editor opens on no entry, draws none, steps nowhere, and OK hands back nothing.
`BoolEditScene` is the same ring with two entries — `OFF` and `ON`, with `--` in front of them when the
setting is nullable — drawn as a switch rather than as a list, so Up and Down wrap there as they do
through a choice. The switch is the editor's body: one chip per entry, an equal share of the body's
width, every chip at one face so the segments read as one control, and the entry showing inverted.
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
4. `EditScene` is the whole of a value editor, and everything built on it answers the same
   gestures; only `DurationEditScene` adds a stage. The scene holds the value it was opened on, the
   increment a keypress takes, the bounds it is held between and whether the kind holds whole
   numbers, and Up/Down step it within them; OK keeps what it holds and Cancel hands back what it
   opened with. A plain number is no kind at all: `NumericEditCfg` builds an `EditScene` with the
   setting's increment, bounds and `whole` flag, and the editor coerces, steps and prints the number
   that flag names.
   The scene draws its column on the page it was given: `draw_page()` builds `editor_column()` — an
   eight-pixel label row, two pixels of paper, the body, two pixels of paper and an eight-pixel hint
   row, each declared in pixels and its label, body and hint centred by the column's alignment — and
   places it from the page's own top left, the page's whole width and as tall as those rows. A kind
   answers only its own body: `body()` and `body_h()`, with `measure()` for the value's ink where its
   look needs it. A choice hands the column the entries as one block the width they measure, centred
   in the body row.
5. A routine page's `ok` is its primary action, `cancel:single` pauses a run under way, and
   `cancel:double` leaves the page.
