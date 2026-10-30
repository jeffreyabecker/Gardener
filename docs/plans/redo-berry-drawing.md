# Making the Berry drawing vocabulary the gfx paint vocabulary

## What is wrong now

A page draws by answering a `draw` record tree from `draw_page()`. The tree is built one node at a
time by the `screen` module ([Screen.cpp](../../ode/eggplan-vacuum/libraries/Gardener/src/routines/bindings/Screen.cpp))
and walked into paints by the interpreter ([DrawTree.cpp](../../ode/eggplan-vacuum/libraries/Gardener/src/routines/DrawTree.cpp)).
The paints it walks into are the gfx vocabulary ([Drawing.h](../../ode/eggplan-vacuum/libraries/Gfx/src/layout/Drawing.h),
[Layout.h](../../ode/eggplan-vacuum/libraries/Gfx/src/layout/Layout.h), [Paint.h](../../ode/eggplan-vacuum/libraries/Gfx/src/layout/Paint.h)).

The two lists have drifted. Against today's tree:

- **Eight factories a page cannot reach.** `glyph` (one built-in glyph by code), `icon(box, paint)`
  (a glyph drawn by a paint of its own), `blend`, `underlay`, `aligned`, `sized`, the `Direction`
  argument of `layout`, and `measureText`'s C++ name (the module calls it `measure`).
- **Names that do not match.** `bar` is `progressBar`, `cell` is `resolveSpan`, `align` is
  `alignedOffset`, and the walk's kind is `bar` where the factory says `progressBar`.
- **One kind with no factory behind it.** `centred` is a node the walk knows and gfx does not: the
  C++ scenes spell the same picture as `icon(box, aligned(sized(text(whole, ...), measureText(...))))`
  ([ChoiceScene.cpp:23](../../ode/eggplan-vacuum/libraries/Gardener/src/panel/page/ChoiceScene.cpp)).
  So the module invents a node the vocabulary cannot name, and the walk carries a second meaning for
  a picture the C++ side builds out of four.

The cost is a reader who cannot move between a page and a scene, a walk that has to be taught every
new shape separately, and `DrawTree` being the only consumer of half of `Drawing.h`.

## The shape of the change

One rule, stated once in the module's comment: **a call in `screen` is a factory in gfx, with the
same name in snake_case, the box first and the remaining arguments in the C++ order; a kind in the
tree is that factory with the fields of its arguments.** Nothing else.

### The parity table

| gfx | `screen` | tree kind |
| --- | --- | --- |
| `text` / `borrowedText(Box, value, face, scale)` | `screen.text(x, y, w, h, value[, font[, scale]])` | `text` |
| `glyph(Box, code, scale)` | `screen.glyph(x, y, w, h, code[, scale])` | `glyph` |
| `icon(Box, bits)` | `screen.icon(x, y, w, h, bitmap)` | `icon`, bitmap in `glyph` |
| `icon(Box, paint)` | `screen.icon(x, y, w, h, draw)` | `icon`, paint in `child` |
| `progressBar(Box, value)` | `screen.progress_bar(x, y, w, h, percent)` | `progress_bar` |
| `scrollbar(Box, index, count, visible, hollow)` | `screen.scrollbar(x, y, w, h, index, count, visible[, hollow])` | `scrollbar` |
| `duration(Box, format, seconds, face, known)` | `screen.duration(x, y, w, h, format, seconds[, font[, known]])` | `duration` |
| `blend(Blend, child)` | `screen.blend(rule, child[, level])` | `blend` |
| `underlay(child)` | `screen.underlay(draw)` | `underlay` |
| `inverted(child)` | `screen.inverted(draw)` | `inverted` |
| `layout(Cell, Children, Justification, Direction)` | `screen.layout(w, h, children[, justification[, direction]])` | `layout` |
| `row(heightPct, Children, Justification)` | `screen.row(h, children[, justification])` | `row` |
| `column(widthPct, Children, Justification)` | `screen.column(w, children[, justification])` | `column` |
| `aligned(Sized, horizontal, vertical)` | `screen.aligned(sized[, horizontal[, vertical]])` | `aligned` |
| `sized(paint, ink)` | `screen.sized(draw, ink)` | `sized` |
| `durationText(format, seconds, known, out, n)` | `screen.duration_text(format, seconds[, known])` | — |
| `resolveSpan(total, declared)` | `screen.resolve_span(total, declared)` | — |
| `alignedOffset(leftover, where)` | `screen.aligned_offset(leftover, where)` | — |
| `shapes::measureText(font, value, scale)` | `screen.measure_text(font, value, scale)` | — |
| `Canvas::width()` / `height()` | `screen.width()` / `height()` | — |
| `panel::page::pressureReadout(box, state, sizeHint, face, scale, secondaryScale)` | `screen.pressure_readout(x, y, w, h, size_hint[, font[, scale[, secondary_scale]]])` | `pressure_readout` |

`centred` leaves the module and the walk: the three pages that use it spell out the composition
their C++ counterparts write.

### The record and its fields

`draw` gains the fields the new kinds use and keeps one field per argument:

- `kind`, `x`, `y`, `w`, `h`, `value`, `font`, `scale` — as today;
- `code`, `glyph`, `child`, `children`, `ink`, `rule`, `level`, `direction`, `horizontal`,
  `vertical`, `justification`;
- the panel's `size_hint`, `secondary_scale`, and the bar's `percent`, the scrollbar's
  `index`/`count`/`visible`/`hollow`, the clock's `format`/`seconds`/`known`.

`alignment` is renamed `justification`, the name of the type it spells; no page names it today.

### The walk

`DrawTree` grows `sizedOf()` (a `gfx::Sized` from a `sized` node: `ink` read as w/h and `child`
walked), `directionOf()` (`nil` → `Declared`, else `down`/`across`) and `blendOf()` (rule and level
into `gfx::Blend`). `paintOf` gains `glyph`, `aligned`, `blend` and `underlay`, learns that an
`icon` node whose `child` is set is the paint form, loses `centred`, and renames `bar`. `sized`
answers an empty paint when it reaches the walk as a child, because `gfx::sized` answers a value and
not a node — a page that puts one in a child list draws nothing, as the C++ caller cannot compile
one.

### What deliberately stays as it is

- **`pressure_readout`** is `panel::page::pressureReadout`, not gfx: the panel's own factory, named
  and argumented like its C++ signature already.
- **`bounds` answers both a `Cell` and a `Size`.** C++ has two types of the same shape; Berry is
  untyped and a second class with the same two fields would be a trap. The record's comment says
  which side is which: nil is the whole side in a child's slot, pixels in `measure`.
- **`icon` dispatches on its argument's type** where C++ overloads: a string is the bitmap, an
  instance is the paint. The record says which field each lands in, so the walk never looks at types.
- **`screen.text` is `borrowedText`.** Every string a page hands over belongs to the interpreter and
  outlives the draw; an owning copy would buy nothing.

## Alternatives, and why not

- **Keep `centred` as a kind.** It is the shortest spelling of a common picture, but it is a second
  meaning for a picture the C++ vocabulary already composes, and it is exactly the drift this change
  exists to remove. The pages spell it out instead, as the scenes do.
- **Mirror `gfx::Blend` as a chain** (`invert()`, `dim(level)`, `then(other)`, `apply(value)`). A
  chain is reachable from two stages up, and nothing in the tree uses one: `inverted` is the only
  blend any caller builds. One stage per node, with `then` deferred.
- **Add `callable` and a `canvas` module with the shapes.** `gfx::callable` takes a C++ closure and
  draws through it; reaching it from Berry needs a canvas handle across the boundary and the whole
  shapes vocabulary behind it (`Line`, `Rect`, `Circle`, `Ellipse`, `Triangle`, `MonoBitmap`,
  `Text`, `drawGlyph`). No page needs it, and it is a canvas API rather than a tree vocabulary. A
  parking-lot line takes it.
- **Add the `Builder` form of `layout`.** The children computed as the layout draws, for the rows a
  list has room for. The browser page computes its window itself and hands a flat list; the C++
  builder is used nowhere. Deferred with `callable`, a parking-lot line.
- **Additive only: add the missing calls and keep the old names.** Cheapest in the diff, but it
  leaves two names for one factory and the walk with a second vocabulary, which is the state this
  plan is undoing.

## Entry points it has to hold on

- **The walk**, for every kind: a node it does not understand must still draw nothing rather than
  raise, because it runs after the page's own call returned and there is no frame to unwind to.
- **The three pages that name a leaf**: [home.be](../../ode/eggplan-vacuum/firmware/data/system/home.be),
  [1_ramped_interval_chrome.be](../../ode/eggplan-vacuum/firmware/data/routines/1_ramped_interval_chrome.be),
  [5_ramped_static.be](../../ode/eggplan-vacuum/firmware/data/routines/5_ramped_static.be).
- **The suites** that pin the module and the walk: [screen.test.be](../../ode/eggplan-vacuum/tools/berry/tests/screen.test.be)
  and [ink.test.be](../../ode/eggplan-vacuum/tools/berry/tests/ink.test.be).
- **The image's load order**: the module and its records are installed before any page compiles, so
  a renamed record is a rename in one place; a page that named the old one would fail to load,
  which is how the rename is caught.

## How it is known to have worked

- `python tools/berry/check.py` compiles and loads the image, drives every page and runs every
  suite; its last line reports the counts and the exit code is 0 only when all of it passed. This
  is the one check: `tools/unit` does not compile the interpreter or the walk.
- The parity table above, read against `Drawing.h`, `Layout.h` and `Canvas.h` with nothing on either
  side unaccounted for: every factory has exactly one row, and no row names a call that is not a
  factory.
- `screen.test.be` asserts the new kinds' fields, so a call that stops answering its factory's
  arguments is a suite failure rather than a blank panel.

## Steps

The detailed, one-commit-each list is the backlog this plan is broken into; it is written first,
before any code, and it holds the fine order. At the coarse level the plan is four epics:

1. **The names.** `progress_bar`, `resolve_span`, `aligned_offset`, `measure_text`, the `bar` kind,
   and `alignment` → `justification`, across the module, the walk, the pages and the suites.
2. **The missing leaves.** `glyph`, `sized`, `aligned` and `icon(box, paint)` in the module, the
   walk and the suite — the four a page needs before `centred` can be spelled out.
3. **The missing composites.** `blend` and `underlay` in the module, the walk and the suite.
4. **`centred` leaves**, the three pages re-spelled, the module comment stating the rule, and the
   parking-lot line for `callable`, the `Builder` form and the shapes vocabulary.

## Notes

- `libraries/BerryHost` is being folded into `libraries/Gardener/src/routines` in the working tree
  (staged moves, uncommitted). This plan is written against the moved paths and must land on top of
  them, not beside them.
- The `draw` record's field list is the union of the kinds, and the walk reads a field a kind does
  not set as the answer a missing field has. A new kind adds its fields to the record and its
  branch to the walk, in the same commit.
