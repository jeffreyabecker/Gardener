# Drawing without components

The backlog for [drawing-without-components.md](../plans/drawing-without-components.md). The
component model — a box an object carries, a tree of them, and a layout that places and draws the
tree — is replaced by functions: a page asks `cells()` for its boxes, `place()` for where a thing sits
in one, and draws with the `screen` module. The C++ classes that mirror the model go with it.

Every item leaves the tree working, with the host check passing and the pages drawing what they drew
before. The C++ and the image are independent until the last port, because nothing binds the C++
drawing to the interpreter.

## Epic 1 — the functions

- [ ] 1.1 — `screen.draw_bar(x, y, w, h, percent)`, over a free function in
  `libraries/Gfx/src/ui/`: the frame and the fill, the value held to its ends. Verified: a host case
  draws a bar at 0, 50 and 100 and reads the pixels back, and one on either side of the range draws
  the ends.
- [ ] 1.2 — `screen.draw_scrollbar(x, y, w, h, index, count, visible)`, over a free function beside
  it: the thumb sized and placed from the three numbers, clamped to the box, and nothing drawn for a
  count of none. Verified: the thumb at the top, the bottom and out of range.
- [ ] 1.3 — `screen.draw_duration(x, y, scale, seconds[, format[, font]])`, over the duration formatter
  that is already a free function, drawn like any other string. Verified: each unit's text, the dash
  for a count that is not known, and a frame on the glass.
- [ ] 1.4 — The C++ classes go: `Component`, `Text`, `Icon`, `ProgressBar`, `Scrollbar`, `Duration`,
  `Layout`, `RowLayout`, `ColumnLayout` and their `Types`, with the host cases that placed them. What
  is left is the three free functions and the formatter, and `Gfx.h` includes only what exists.
  Verified: `python tools/gfx/check.py` passes with the cases rewritten against the functions.

## Epic 2 — the distribution

- [ ] 2.1 — `layout.be` loses its class and keeps its rules as two functions: `cells()` answering a
  box per declared span, and `place()` answering a corner for a size and an alignment. Verified: the
  layout suite states the answers — the equal thirds, the rounded share, the declared pixels, the
  centred offset — and the check drives a page that lays its rows out with them.
- [ ] 2.2 — The scene's page and the band draw their rows with `cells()`: the label row, the body and
  the hint row of a value editor, and the four cells of the status band. Verified: `--scene home` and
  `--scene browser` draw the frames they drew before.

## Epic 3 — the pages

- [ ] 3.1 — The home scene and its chrome draw with the verbs: the title, the pressure display's
  reading, the timer and the run state, each measured with `screen.text_bounds` and placed with
  `place()`. `pressure_display.be` keeps its class and draws through the verbs. Verified: the home
  suite and `--scene home`.
- [ ] 3.2 — The routine chrome and both routine pages draw their rows and their bar with the verbs.
  Verified: the chrome suite, the routine suite and `--art`.
- [ ] 3.3 — The browser and the list: a list becomes a selected index and a loop that draws its
  visible rows, the selection inverted. `list.be` loses its classes. Verified: the list and browser
  suites.
- [ ] 3.4 — The settings list and the value editors draw their columns, their rows and their kinds'
  bodies with the verbs. `edit_scene.be`, `bool.be`, `choice.be`, `duration.be` and `settings_edit.be`
  lose the layouts they declared. Verified: the settings suite and the drive's settings walk.
- [ ] 3.5 — The component files leave the image: `component.be`, `text.be`, `icon.be`, `layout.be`,
  `progress_bar.be`, `scrollbar.be` and `duration.be` are deleted and `/system/order` no longer names
  them, so `--memory` lists neither them nor the objects they made. Verified: the full `--tests` run
  and `--memory`.

## Epic 4 — the record

- [ ] 4.1 — `docs/specs/ui/components.md` becomes the drawing verb list: the box a call takes, the
  three composed verbs, and the two distribution functions. Verified: the file names no class.
- [ ] 4.2 — `routine.md` and `scenes.md` state the page box and the drawing a page does, rather than
  the components it composes. Verified: neither names a component class.
