# The row list

The steps of [the row list plan](../plans/row-list.md), in the order they land. Each item is one
branch, one commit and one diff, and leaves the tree building with the suite green. Branches are
`backlog/row-list/main`, `backlog/row-list/epic-<n>/main` and `backlog/row-list/epic-<n>/task-<m>`,
merged back with `--no-ff` as each item lands.

The plan's Berry half — the native `RowList` class, the `row_list` node and the browser — is a later
backlog and is not here.

The numbers every item is checked against are the plan's: the row cell is the row height the caller
gives — 8 px for the settings list and the choice editor's shortest line of the face — the bar is
`gfx::drawScrollbar()`'s, two pixels wide and inset two at both ends in a `gfx::ScrollbarColumn`
(4 px) column; the page's band is 52 px tall, the overlay's 12 below it.

## Epic 1 — the component

- [ ] **Borrow a paint.** Add `gfx::borrowed(const PaintNode&)` to `Drawing.h`/`Drawing.cpp`: a node
      that draws one it does not own and leaves it alive, the ownership counterpart of
      `borrowedText`. A case draws the borrowed node twice and reads
      `inverted(borrowed(paint))` as the cell's ink and paper counted together.
- [ ] **The model.** Add `RowList` in `panel/page/RowList.h`/`.cpp` — a plain class a scene holds,
      not a paint node: `Row { String value; gfx::PaintNodeRef paint; }`, the row height, a selected
      value and a wrap flag; `nudge`, `selected`, `current` and `draw(canvas, along)`, which draws
      the window into the band it is handed. Cases in `tools/unit/cases/row_list.cpp`: the ring in
      both modes, a clamp at either end, a value no row holds, an empty list and a one-row list.
- [ ] **The drawing.** Cases for what `draw()` puts on the glass: a seven-row list in a four-row band
      shows rows 0–3 and no bar; the fifth value selected shows rows 1–4 with only that row inverted;
      a three-row list takes the full width and draws no bar; a list that outgrows the band gives its
      rows up four pixels and inks the bar's two-pixel column; a five-pixel band draws one clipped
      row; the rows sit where the caller's justification puts them.

## Epic 2 — the scenes on it

- [ ] **Move `SettingsScene` onto it.** The rows are built in the constructor and rebuilt when an
      editor hands a value back, with the key the keys are on as the selection; `move()` calls
      `nudge()`, `selected()` answers `current()`, and `drawPage()` calls `list_->draw(band)` under
      the heading. The window arithmetic goes. The existing selection and editor cases stay green,
      and a case opens an editor, changes the value and reads it in the rebuilt row.
- [ ] **Move `ChoiceScene` onto it.** The items become rows of key and title; `nudge()` wraps around
      `RowList::nudge()` with the nullable ends kept — Up at the first clears, Down from nothing
      enters the first; `index()` and `text()` read `current()`; `pageContents()` answers a
      `gfx::callable` that calls `list_.draw(canvas, gfx::Justify::Center)` while an entry is
      showing, and draws the `--` row otherwise; a key no entry names shows the first. `windowOf()`,
      `BarWidth` and `BarInset` go. Cases cover the ring, the nullable Up, `--` with nothing
      selected, and the missing key.
- [ ] **Drive the device's own list through it.** `SystemSettingsScene` builds a `SettingsScene` over
      `SystemSettings::rows()`, so it inherits the model with the item above; a case opens it, edits
      a row and reads the rebuilt row and the set `SystemSettings::apply()` is handed. No change to
      `SystemSettings` is expected; a hook it turns out to need lands here.
