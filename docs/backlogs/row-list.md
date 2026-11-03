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

## Epic 3 — the Berry bindings

- [ ] **The class a page uses.** `bindings/RowList.cpp` declares the global `RowList` with `.p` for the
      comobj that owns the model and `.rows` for the page's pairs; `init(row_height, rows, selected,
      wrap)` walks each pair's draw through a new `paintFromTree()` export, resolves the selected
      value and builds the model, and `nudge`, `getSelectedValue` and `draw()` answer through it.
      `draw()` answers a `row_list` record with the instance in a new `list` field, and `DrawTree`
      answers it with a callable that calls `list->draw(canvas)`. `installRowListClass()` runs in
      `BerryInterpreter` with the modules. `python tools/berry/check.py` loads the image.
- [ ] **Pin the class.** `tools/berry/tests/list.test.be` is the wrapper's suite, auto-globbed like the
      rest: `RowList(...)`, `nudge` and `getSelectedValue` agree with the model, a `nil` selection
      answers nil, a row that is not a pair or a wrap word that is neither `"wrap"` nor `"clamp"`
      raises, and a pushed page that draws the list inks the selected row's whole cell.
- [ ] **Move the browser onto it.** `BrowserScene.set()` builds `[value, screen.row(...)]` pairs and
      constructs `RowList(8, pairs, key, "wrap")`; `handle()` nudges, `selected_row()` reads
      `getSelectedValue()`, and `draw_page()` answers `self.list.draw()`. `browser_chrome()`,
      `step()`, `first()` and `VISIBLE` go. `browser.test.be` reads the page through the wrapper and
      its window case moves to the list's suite; `chromes.test.be` loses the browser case. The
      suites and `--scene browser` pass.
