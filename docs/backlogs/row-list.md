# The row list

The steps of [the row list plan](../plans/row-list.md), in the order they land. Each item is one
branch, one commit and one diff, and leaves the tree building with the suite green. Branches are
`backlog/row-list/main`, `backlog/row-list/epic-<n>/main` and `backlog/row-list/epic-<n>/task-<m>`,
merged back with `--no-ff` as each item lands.

Epic 1 is the C++ model and its cases, epic 2 the settings and choice scenes on it, and epic 3 the
class the image reaches it through and the browser on that class.

The numbers every item is checked against are the plan's: the row cell is the row height the caller
gives — 8 px for the settings list and the choice editor's shortest line of the face — the bar is
`gfx::drawScrollbar()`'s, two pixels wide and inset two at both ends in a `gfx::ScrollbarColumn`
(4 px) column; the page's band is 52 px tall, the overlay's 12 below it.

## Epic 1 — the component

- [x] **The model.** Add `RowList` in `shapes/RowList.h`/`.cpp` — a plain class a scene holds,
      not a paint node: `Row { String value; PaintNodeRef paint; }`, the row height, a selected
      value and a wrap flag; `nudge`, `selected`, `current` and `draw(canvas, along)`, which draws
      the window into the band it is handed. Cases in `tools/unit/cases/row_list.cpp`: the ring in
      both modes, a clamp at either end, a value no row holds, an empty list and a one-row list.
- [x] **The drawing.** Cases for what `draw()` puts on the glass: a seven-row list in a four-row band
      shows rows 0–3; the fifth value selected shows rows 1–4 with only that row inverted;
      a three-row list takes the full width and draws no bar; a list that outgrows the band gives its
      rows up four pixels and inks the bar's two-pixel column; a five-pixel band draws one clipped
      row; the rows sit where the caller's justification puts them.
- [x] **The row builders.** `RowList::textRow(value, title, horizontal)` and
      `textRow(value, title, bitmap, horizontal)` build the two rows a scene draws: a title placed
      across the cell, and a mark in its own columns before it, centred down the row, with a string
      that names no bitmap falling back to the title alone. Cases in
      `tools/unit/cases/row_list.cpp` read the value, the title's alignment and the mark's columns.
- [x] **Move it into the drawing library.** `RowList` moves from the panel to
      `libraries/Gfx/src/shapes/` as `gfx::RowList`, beside the layout it draws with, so the scenes
      and the binding reach one model without Gardener owning it; `Gfx.h` includes it, and the two
      scenes and the case name it `gfx::RowList`.

## Epic 2 — the scenes on it

- [x] **Move `SettingsScene` onto it.** The rows are built in the constructor and rebuilt when an
      editor hands a value back, with the key the keys are on as the selection; `move()` calls
      `nudge()`, `selected()` answers `current()`, and `drawPage()` calls `list_->draw(band)` under
      the heading. The window arithmetic goes. The existing selection and editor cases stay green,
      and a case opens an editor, changes the value, reads it in the set the list holds and checks
      the rebuilt row draws the cell it drew before the edit, a row printing its label and not its
      value.
- [x] **Move `ChoiceScene` onto it.** The items become rows of key and title; `nudge()` wraps around
      `RowList::nudge()` with the nullable ends kept — Down at the first clears, Up from nothing
      enters the first; `index()` and `text()` read `current()`; `pageContents()` answers a
      `gfx::callable` that calls `list_.draw(canvas, gfx::Justify::Center)` while an entry is
      showing, and draws the `--` row otherwise; a key no entry names shows the first. `windowOf()`,
      `BarWidth` and `BarInset` go. Cases cover the ring, the nullable Up, `--` with nothing
      selected, and the missing key.
- [x] **Hand the choice scene its rows.** `ChoiceScene` takes the `gfx::RowList::Row`s a row built —
      `textRow(key, title)` — instead of the `ChoiceItem`s, keeps only their keys, and re-seats its
      list with the new `RowList::select()` when a nullable value clears or a cancel puts the opened
      key back. `text()` goes, since the title is the row's paint, and the pitch is the face's as the
      scene is built.
- [x] **Drive the device's own list through it.** `SystemSettingsScene` builds a `SettingsScene` over
      `SystemSettings::rows()`, so it inherits the model with the item above; a case opens it, edits
      a row and reads the value in the set the list holds and the set `SystemSettings::apply()` is
      handed. No change to `SystemSettings` is expected; a hook it turns out to need lands here.

## Epic 3 — the Berry bindings

- [x] **The class a page uses.** `bindings/RowList.cpp` declares the global `RowList` with `.p` for the
      comobj that owns the model and `.rows` for the page's pairs; `init(row_height, rows, selected,
      wrap)` resolves the selected value and builds the model, and `nudge`, `getSelectedValue` and
      `draw()` answer through it. A pair's draw is walked through a new `paintFromTree()` export, or,
      when it is a string, is the title of a row built by `RowList::textRow(value, title)`, with an
      optional third element naming a bitmap printed in the columns before it and an optional fourth
      naming the alignment as `"start"`, `"center"` or `"end"`; an icon that names no bitmap leaves
      the title alone, as the helper does. `draw()` answers a `row_list` record with the instance in
      a new `list` field, and `DrawTree` answers it with a callable that calls `list->draw(canvas)`.
      `installRowListClass()` runs in `BerryInterpreter` with the modules.
      `python tools/berry/check.py` loads the image.
- [x] **Pin the class.** `tools/berry/tests/list.test.be` is the wrapper's suite, auto-globbed like the
      rest: `RowList(...)`, `nudge` and `getSelectedValue` agree with the model, a `nil` selection
      answers nil, a row that is not a pair, a title that is not a string, a wrap word that is
      neither `"wrap"` nor `"clamp"`, and an alignment word that names no justification raise, and a
      pushed page that draws the list inks the selected row's whole cell. A `[key, "Title"]` pair and
      a `[key, "Title", icon]` pair draw the title and the mark the helpers build.
- [x] **Move the browser onto it.** `BrowserScene.set()` builds `[value, screen.row(...)]` pairs and
      constructs `RowList(8, pairs, key, "wrap")`; `handle()` nudges, `selected_row()` reads
      `getSelectedValue()`, and `draw_page()` answers `self.list.draw()`. `browser_chrome()`,
      `step()`, `first()` and `VISIBLE` go. `browser.test.be` reads the page through the wrapper and
      its window case moves to the list's suite; `chromes.test.be` loses the browser case. The
      suites and `--scene browser` pass.
