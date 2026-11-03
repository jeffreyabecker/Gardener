# A row list for the pages

A list component for the panel, written in C++ and exposed to the image as a Berry class. It is built
from a row height, an ordered list of `value:paint` entries, the selected value and a wrap mode, and
it answers `nudge(direction)` and `getSelectedValue()`. It owns the window a band shows, the inverted
row and the scrollbar, and draws them into the canvas a scene hands it. It is not a scene: a scene
keeps the gesture and the values, and the list keeps the arithmetic and the drawing.

## What is wrong now, and where

The browser writes the arithmetic itself. [`browser_chrome()`](../../firmware/data/system/browser.be)
takes the rows, the selected index, the window's first row and the visible count, and composes the
column, the inversion and the bar; `BrowserScene.step()` wraps the index, `first()` derives the
window, and `VISIBLE = 6` is the row count. The row height is a local `8` inside the chrome, the bar
is given two pixels whether or not it draws, and it is drawn solid and full height.

[`SettingsScene::move()` and `drawPage()`](../../libraries/Gardener/src/panel/settings/SettingsScene.cpp)
write the same arithmetic again in C++: `Row = 8`, the visible count from the band's height,
`first = selected - visible + 1`, the selected row inverted, and `gfx::drawScrollbar()`, which gives
the rows up `gfx::ScrollbarColumn` (four pixels) and draws a hollow bar two pixels wide, inset two
pixels at the top and bottom. The rows, the window and the inversion agree in the two; the bar does
not.

The choice editor writes a third. [`ChoiceScene::pageContents()`](../../libraries/Gardener/src/panel/settings/ChoiceScene.cpp)
computes its own `Window` from the band's height and the face's shortest line, inverts the entry
showing, centres the block, and builds the bar from its own `BarWidth` and `BarInset`. Its numbers
agree with the band's — four columns given up, `Box(2, 2, 2, height - 4)`, hollow — and its window is
the same rule; what differs is the centred block and the `--` row a nullable setting shows while it
holds nothing.

So the tree has three implementations of one behaviour, and the only thing keeping them together is
that nobody has changed one since. The browser's suite reads through the copy it has —
`browser.selected`, `browser.first()`, `BrowserScene.VISIBLE` — because there is no boundary to read
it through.

## The shape of the change

One model in C++, reached from a page through a wrapper class. The settings list, the choice editor
and the browser then use the one arithmetic, and a row's picture stays the caller's: a settings line
rebuilt from the value it holds, or a page's own `screen` record walked from the image.

### The model

`libraries/Gardener/src/panel/page/RowList.h` and `.cpp` hold `RowList`, beside `Overlay` and
`PressureReadout`. It knows no `bvm` and no `screen` record: a row is a value and the paint that
draws it, and the list owns both.

```cpp
class RowList {
 public:
  // One row: the key a scene selects it by and the paint that draws it.
  struct Row {
    String value;
    gfx::PaintNodeRef paint;
  };

  RowList(int16_t rowHeight, std::vector<Row> rows, const String& selected, bool wrap);
  bool nudge(int direction);
  const String& selected() const;
  int16_t current() const;                       // the row's index, or -1

  void draw(gfx::Canvas& canvas, gfx::Justify along = gfx::Justify::Start) const;
};
```

The rules it holds:

1. A row is selected by its value, compared with `==`; the first row holding it is the current one.
   Values are unique, which is the caller's rule and not checked.
2. A selected value no row holds leaves the list with no current row. The window then starts at the
   first row, and `nudge` down selects the first row and up the last.
3. `nudge` moves one row by the sign of `direction`, `-1` up and `1` down. It answers whether the
   current row changed: false on an empty list, a direction of 0, a one-row list, and an end when the
   list clamps.
4. `wrap` steps from the last row to the first and the first to the last; without it the selection
   stops at the end.
5. `visible = height / rowHeight`, floored, at least one when the band has a pixel and capped at the
   number of rows.
6. The window's first row is 0 when the current row is in the first `visible`, otherwise
   `current - visible + 1`. It is derived on every draw; nothing remembers a scroll.
7. Only the current row is drawn through `gfx::inverted`, and it fills the row's cell.
8. The bar is drawn only when the rows outnumber the window, with
   `gfx::drawScrollbar(canvas, first, count, visible)` after the rows: the rows give up
   `gfx::ScrollbarColumn`, and the bar is hollow, two pixels wide and inset two at both ends.
9. A list shorter than its band leaves the spare rows where the caller's justification puts them:
   the browser and the settings list start at the top, the choice editor centres its block.
10. `draw()` asks the canvas for its size and draws into it: there is no box to pass and no tree to
    return. The list is not a paint node; a scene holds it and calls it.

A held paint is drawn as often as the band is: `PaintNode::draw()` is const and leaves the node where
it was, so `std::vector<gfx::PaintNodeRef>` is a fine thing for the model to own. The one operation
that cannot use a held node is the highlight, because `gfx::inverted()` takes its child by value and
owns it. The current row is drawn as
`gfx::inverted(gfx::callable([this, at](gfx::Canvas& c) { rows_[at].paint->draw(c); }))->draw(cell)`,
and every other row as `rows_[at].paint->draw(cell)`. `callable()` lends the held paint to the
inverting wrapper for the one draw that needs it, so nothing is added to Gfx and the picture keeps
its one spelling.

### The class a page uses

`libraries/Gardener/src/bindings/RowList.cpp` declares the global class `RowList`, the way
`Settings.cpp` declares a kind: a `bnfuncinfo` table with `.p` for the comobj that owns the model,
`.rows` for the page's pairs, and the methods below. `installRowListClass()` is called by
`BerryInterpreter` with the other installers in
[BerryInterpreter.cpp](../../libraries/Gardener/src/routines/BerryInterpreter.cpp).

| call | what it does |
| --- | --- |
| `RowList(row_height, rows, selected, wrap)` | unpacks the four, walks each pair's draw into a paint or builds its title row through `textRow`, resolves `selected`, builds the model in the comobj, keeps `rows` on the instance |
| `nudge(direction)` | calls the model and answers its bool |
| `getSelectedValue()` | the selected value, or `nil` when no row is current |
| `draw()` | a `draw` record naming the instance, which the walk draws by calling the list's `draw()` |

`rows` is the ordered list of `[value, draw]` pairs a page builds — the browser hands
`[routine_key, screen.row(nil, cells)]`. A pair whose draw is a string is a title row instead, built
by `RowList::textRow(value, title)`: the pair's third element names a bitmap printed in the columns
before the title, the fourth the alignment's word — `"start"`, `"center"` or `"end"`. A bitmap that
names no mark leaves the title alone, which is the helper's own fallback. The value is a string,
because the keys the pages select by are strings; a row that is not a pair, a value or a title that
is not a string, a `row_height` below one, a wrap word that is neither `"wrap"` nor `"clamp"`, and an
alignment word the vocabulary does not hold are refused at the call.

The instance keeps the page's pairs in `.rows` for as long as it lives, so the strings a row's paint
borrows stay where the interpreter put them. The walk that turns one pair's draw into a paint is the
page's own, so [`DrawTree`](../../libraries/Gardener/src/bindings/DrawTree.cpp) exports
`paintFromTree(bvm*, node, SystemState)` beside `drawTree()`. A row is a picture taken when the list
is built, which is what a registry row and a setting line are; a row that must move between beats — a
clock, a readout — is not one, because the state it would read is the copy the call took. The draw
record carries the instance, not a copy of anything.

### The node the page draws

A page has no canvas to hand a component, so its `draw()` answers a `draw` record with
`kind = "row_list"` and the instance in a new `list` field. `self.list.draw()` is the browser's whole
page when the tree gives the node the page.

[`DrawTree`](../../libraries/Gardener/src/bindings/DrawTree.cpp) learns the kind: it reads the handle
and returns `gfx::callable([list](gfx::Canvas& canvas) { list->draw(canvas); })`. The node is drawn by
the same method a C++ scene calls outright — the one paint is the callable the walk makes, not the
list. A missing handle draws nothing rather than raising.

The kind is a panel factory, like `pressure_readout`: the vocabulary's rule is that a `screen` call
answers a gfx factory, and this node answers the panel's own list drawing. It is the only place in
the change that the drawing vocabulary grows.

### The settings list on top of it

`SettingsScene` keeps its heading, its values and its editors, and gives up the arithmetic: the
`selected_` index becomes a `RowList` built from the visible rows, `move()` calls `nudge()`,
`selected()` answers `current()`, and `drawPage()` draws the band under the heading by calling
`list_->draw(band)`. `open()` reads `current()`.

The list is built in the constructor and again when an editor hands a value back — the one moment a
row's text moves — with the key of the row the keys are on as its selection, so the row survives the
rebuild. A rebuild is a vector of rows and their paints, a few Strings either way, and it costs less
than the row text the draw writes on every beat today.

### The choice editor on top of it

`ChoiceScene` keeps its items, its nullable rule and its `--` row, and gives up `windowOf()`,
`BarWidth`, `BarInset` and the row loop. The items become rows of key and title; `nudge()` keeps the
nullable ends around `RowList::nudge()` — Up at the first clears a nullable value, Down from nothing
enters the first; `index()` and `text()` read `current()`; and `pageContents()` answers
`gfx::callable([this](gfx::Canvas& canvas) { list_.draw(canvas, gfx::Justify::Center); … })` while an
entry is showing, and draws the `--` row in that same call when none is. A key no entry names selects
the first entry, as the editor does today.

### The browser on top of it

`BrowserScene` keeps its gestures, its activation, `choose()`, `open_selected()` and
`settings_closed()`. `set(rows)` builds the pairs and constructs `RowList(8, pairs, key, "wrap")`,
where `key` is the first row's value on the first build and the previous `getSelectedValue()` on a
rebuild. `selected_row()` finds the registry row whose value is `getSelectedValue()`. `handle()`
answers up and down with `self.list.nudge(-1)` and `self.list.nudge(1)`. `draw_page()` answers
`self.list.draw()`. `browser_chrome()`, `step()`, `first()` and `VISIBLE` are deleted, along with the
chromes case that drove the chrome.

## Alternatives, and why not

- **A Berry class that composes the `screen` tree.** It needs no C++ and no walk change, and it is
  the smaller diff. It leaves the arithmetic in two languages, which is the state this change exists
  to leave, and it cannot be the model the settings list uses.
- **The list as a `PaintNode`.** A node is a picture the tree owns for one draw; the list is state a
  scene keeps across beats — its rows, its selection and its wrap. A node would either move that
  state into the tree or hold a handle to it, which is the callable the walk already makes and
  nothing more.
- **`band()` answering a paint.** The scene would place the list's paints in its own tree, which is
  passing the list's internals up the tree; `draw(canvas)` keeps them in the component.
- **A row supplier instead of held paints.** The model would own the numbers and the caller each
  row's picture, rebuilt as the band reaches it. It is a second way for a row to exist, and it makes
  the settings list rebuild every row's text on every beat to keep its supplier honest, when the
  pictures only change when an editor returns.
- **An `inverted(const PaintNode&)` overload.** One name with two ownership rules is a trap: an
  lvalue `PaintNodeRef` would silently stop being consumed. `callable()` lends the paint for one draw
  without a second rule.
- **A wrapper that answers the record tree from the model's numbers.** The wrapper would compute
  cells and pick nodes, which is the layout the model is for, on the wrong side of the boundary.
- **A `screen.list(...)` call a page makes itself.** A `screen` call answers a `draw` record, and a
  page has no canvas to draw into; the record the wrapper answers is the way in.
- **The selection as an index at the boundary.** The browser re-reads the registry when it appears,
  and an index would stand for a different routine after a reordering; the routine key is what the
  scene uses, and it is what lets a rebuilt list keep its row.
- **A remembered scroll offset.** The window is a function of the current row and the band in both
  lists today, and an offset is a second piece of state to repair when either moves.
- **The browser's flush, solid bar.** It is two pixels at the right edge of the rows and full height;
  the band's is hollow and inset in a four-pixel column. One picture is the point and the band's is
  the older one, so the browser's rows lose four pixels instead of two while the bar shows and gain
  two when it does not.

## Entry points it has to hold on

- **The installer.** `installRowListClass()` runs with the modules in `BerryInterpreter`, so the
  class is a global before any image file compiles. No `/system/order` entry is involved: nothing in
  `firmware/data` gains a file.
- **The one-node walk.** `DrawTree` exports `paintFromTree()` beside `drawTree()`, and its comment
  gains the rule the wrapper depends on: a paint's borrowed text points into the node it was walked
  from, so whoever keeps the paint keeps the node — which is why the instance keeps `.rows`.
- **The walk.** `row_list` is a new kind in `DrawTree`, and the `list` field is declared in the
  `screen` module's record beside the kinds that already read it. The kind answers a callable that
  calls the list's own `draw()`, and a node with no handle draws nothing.
- **The two checks.** `tools/unit/check.py` compiles Gfx and Gardener and excludes the bindings, so
  it checks the model; `tools/berry/check.py` compiles the interpreter, the walk and the image, so it
  checks the wrapper, the node and the pages. Neither is a substitute for the other.
- **The page's band.** `ux.overlay_start()` is the page's height — 52 on the panel, where the overlay
  takes the last 12 of 64. A page with a heading hands the list the band less the heading.
- **The browser's gestures.** `handle()` still answers up, down, ok, forward, back and cancel, and
  `activate()` still vents and re-reads the registry. Only the arithmetic inside up and down moves.
- **The settings list's cases.** `gfx_scenes.cpp` reads the list's selection and drives its editors;
  the adoption must leave every one of them green.

## How it is known to have worked

`tools/unit/cases/row_list.cpp` is the model's suite, and its cases are the rules above with numbers:
a seven-row list in a four-row band draws rows 0–3; the same list with the fifth value selected draws
rows 1–4, `current` 4 and only that row inverted; a clamped end answers false and stays, while a
wrapping one answers true with the first row current; no matching value inverts nothing and nudges
down to the first row and up to the last; a three-row list in a four-row band draws no bar and takes
the full width; a five-pixel band draws one row; a one-row list answers false either way. The cases
build their rows from `gfx::rectangle` and `gfx::text`, and
`display.inkCount()` reads the inverted row as the cell it is; the settings-list cases already in the
suite prove the adoption.

`tools/berry/tests/list.test.be` is the wrapper's suite, auto-globbed like the rest: a page's
`RowList(...)`, `nudge` and `getSelectedValue` agree with the model, a `nil` selection answers nil, a
refused row, value or wrap word raises, and a pushed page drawing `self.list.draw(...)` inks a
selected row's whole cell and none of its neighbours. The browser's suite still proves the page: the
rows are the registry's, only the selected row is inverted, OK launches the row it stands on, Forward
opens the list and settles into a launch, and Back leaves. Its window case moves to the list's suite,
where a fabricated list can be longer than the image's.

`python tools/berry/check.py` ends with every file loaded, every page driven and exit code 0, and
`python tools/berry/check.py --scene browser` draws the browser for a look at the glass; the picture
is for a person and nothing in a run reads it back. On the device, `python scripts/arduino.py --task
upload`, the panel, and the `[E]` lines on the console are the last word.
