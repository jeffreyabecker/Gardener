# How should a paint hold what it draws without a closure stack?

`Paint` is a `std::function<void(Canvas&)>`, so the drawing language is built from nested
closures — one per factory, per layout child and per Berry node, plus a fresh span vector on
every layout draw. The shape vocabulary underneath is already plain value structs; the
composition above it is not.

## The shape

One box type comes first, because it is what lets the C++ pages drop their sizing lambdas. The
Berry walk already knows it as `NodeBox`: a pixel box whose sides may be the whole of the canvas it
is handed. Promote that to a `Box` beside `Rect`, with `Rect` converting to one with no whole
sides, so the existing `Rect`-taking calls read unchanged. Then the node vocabulary is one struct
per factory:

```cpp
struct TextNode { Box box; const char* value; const Font* face; uint8_t scale; };
struct GlyphNode { Box box; const char* glyph; };
struct BarNode { Box box; float value; };
struct ScrollbarNode { Box box; int16_t index, count, visible; bool hollow; };
struct DurationNode { Box box; DurationFormat format; int32_t seconds; const Font* face;
                      bool known; };  // formatted once into a fixed buffer, no std::string
struct IconNode { Box box; Paint glyph; };
struct InvertedNode { Paint child; };
struct UnderlayNode { Paint child; };
struct AlignedNode { Sized sized; Justify horizontal; Justify vertical; };
struct LayoutNode { Cell cell; Justification justification; Direction direction;
                    std::vector<Child> children; };
```

`Paint` holds a tag and the leaf structs inline, and points at the composite nodes through one
`std::shared_ptr`, which is what breaks the recursion — a `Paint` is a value, a composite owns
`Paint` children. `operator()(Canvas&)` switches on the tag, so the dispatch stays in
[Drawing.cpp](../../libraries/Gfx/src/Drawing.cpp) and [Layout.cpp](../../libraries/Gfx/src/Layout.cpp),
not in the header. The one remaining erasure is the callable case, kept for draws the vocabulary
cannot name; more on that below.

`LayoutNode` runs as a two-pass walk. The first pass sums the spans to learn the leftover and the
start offset; the second recomputes each span and places the child. The arithmetic is unchanged —
`resolveSpan`, `alignedOffset` and `stacksDown` stay exactly as they are — but the per-draw
`std::vector<int16_t> spans` goes, because nothing between the two passes needs to remember more
than the running total.

The Berry boundary then becomes a straight builder. `paintOf` reads a node and constructs the
matching `Paint` value; `leaf()` and its `Make` lambdas go, `boxOf` answers a `gfx::Box`, and
`childrenOf` pushes value paints into the layout's vector. The walk's string-lifetime note still
holds: a node keeps `const char*`, not a copy, and the interpreter's strings outlive the walk.

### Where each paint goes

The leaves of [Drawing.h](../../libraries/Gfx/src/Drawing.h) land as inline nodes: `text(Rect, ...)`
is `TextNode`, `icon(Rect, const char*)` is `GlyphNode`, `progressBar` is `BarNode`, `scrollbar` is
`ScrollbarNode`, `duration` is `DurationNode`. The composites land behind the shared pointer:
`icon(Rect, Paint)` is `IconNode`, `inverted` is `InvertedNode`, `underlay` is `UnderlayNode`,
`aligned` is `AlignedNode`, and `layout`/`row`/`column` are `LayoutNode`. `drawCentered` goes, as
the alignment idea already decided, and `drawScrollbar` stays the immediate helper it is.

The pages collapse rather than translate. `EditorScene::pageComponents` becomes a column of three
children whose title and hint are `AlignedNode(text(...))`, so the two `this` captures go.
`BoolScene::chip`, `DurationScene::field` and `NumberScene::numberBand` become `AlignedNode` and
`BarNode`; the measured cells, the `centred()` helpers and `centredText` go with them, exactly as
the alignment idea describes. `NumberScene`'s bar sizes itself with a whole-cell `Box` instead of a
lambda around `progressBar`. `ChoiceScene::rowOf` becomes `InvertedNode(AlignedNode(text(...)))` or
`AlignedNode(text(...))`, and `SettingsScene`'s `rowText` becomes a `TextNode` with a whole-width
box.

Three draws stay in the callable case, and that is the design's seam, not an oversight. The
pressure readout in [PressureReadout.cpp](../../libraries/Gardener/src/panel/page/PressureReadout.cpp)
captures a copy of `SystemState` and formats it as it draws; the overlay in
[Overlay.cpp](../../libraries/Gardener/src/panel/glass/Overlay.cpp) is a stateful callable object
held by `Ux`, not a factory; and `ChoiceScene::pageContents` builds its rows only once it knows the
canvas height. Each could earn a node, but a node kind exists for a draw the vocabulary needs, and
one page wanting its own behaviour is what the callable case is for. The unit tests' `fill` and
`spill` lambdas go through the same case, unchanged. `MarkScene` already draws a shape directly and
needs nothing.

## How the sized logic fits

The alignment idea's three pieces slot into the vocabulary without a single closure:

- `Size` sits beside `Rect` and replaces `shapes::TextBox`; `measureText` answers it.
- `Sized { Size ink; Paint paint; }` is the pair `aligned` takes, with the paint now a value. The
  measuring `text(value, face, scale)` answers it, and `sized(paint, size)` covers a child whose
  size the caller already knows.
- `aligned(Sized, Justify horizontal, Justify vertical)` returns `AlignedNode`. Its draw is
  `alignedOffset` on each axis and then the paint run on a view narrowed to the ink.

The size stays on `Sized`, not on `Paint`, for the same reason the alignment idea refused the ink
field: only the aligned child is ever asked its size, and every other node would have to invent an
answer that nothing checks. The value `Paint` makes the refusal cheaper to reverse, because a future
"how big is this paint" is a switch over the finite kinds plus an honest unknown for the callable
case — it no longer has to touch every paint and every node kind to be added.

## What has to be true

- `Box` exists beside `Rect`, and the leaf factories take it; the implicit conversion from `Rect`
  leaves every call site as it is.
- `Paint` becomes the value type, and its draw switch lives in `Drawing.cpp` and `Layout.cpp`; the
  callable case is the only remaining erasure.
- The alignment idea's `Size`, `Sized`, the measuring `text` and `aligned` land in the same change,
  because `AlignedNode` is one of the nodes and `drawCentered` and the measured cells are removed on
  the way — the two ideas are one change to the same path.
- `arrange` is rewritten as the two-pass walk, so the per-draw `spans` vector goes.
- `DrawTree` builds nodes directly; its `leaf()` template and the per-kind `Make` lambdas go, and
  the string-lifetime note in the walk still holds because nodes keep pointers, not copies.
- The overlay, the pressure readout and the choice list keep the callable case.
- The `gfx_layout` and `gfx_drawing` cases pass through the new factories unchanged, which is the
  observation that says this is a re-representation rather than a behaviour change.

## What is still unknown

- Whether the callable case stays `std::function` or becomes a raw function pointer with a context,
  trading safety for a little size. Three sites argue for keeping the `std::function`.
- Whether the inline leaves sit in a plain struct — simpler, a little larger than a `std::function`
  — or a hand-rolled union — smaller, with destructor and copy machinery of its own. The plain
  struct is the start; the union is what to reach for when a leaf grows.
- Whether `Box` whole sides replace the `Rect` parameters or sit beside them, and whether
  `shapes::TextBox`'s move to `gfx::Size` is this change's or the alignment change's.
- Whether the overlay should be re-expressed as `UnderlayNode(LayoutNode(...))` of four cell nodes
  and drop its callable. Nothing but the overlay, the readout and the choice list needs the seam, so
  the question is only whether three uses are enough to keep it — they are.
- Whether `AlignedNode` belongs in `Layout.h`, where `alignedOffset` lives, or in `Drawing.h`, where
  the wraps live — the same open question the alignment idea left.
- Whether the grown `Paint` matters anywhere it is stored across beats. `Ux::overlay_` is the only
  long-lived one, and it is one paint, not a stack.

The vocabulary and the arithmetic are already in the tree as value structs — `shapes::*` and
`alignedOffset` — so this extends a shape the code already has rather than inventing one, and what
it buys is the removal of the closure stack from the one path that runs every frame.

`Paint` is a `std::function<void(Canvas&)>`, so the drawing language is a language of closures:
each factory in [Drawing.h](../../libraries/Gfx/src/Drawing.h) returns one, `layout`/`row`/`column`
return one holding a vector of them, the pages wrap those in lambdas of their own, and the Berry
walk in [DrawTree.cpp](../../libraries/BerryHost/src/DrawTree.cpp) wraps every leaf in a closure
before any of it runs. The alignment idea in `alignment-in-layout.md` proposes `Sized` and
`aligned` on top of that — two more closure layers for every centred string.

The idea is to stop holding a drawing in a callable. `Paint` becomes a small value carrying a finite
vocabulary of node structs — one per factory, the leaves inline and the composites behind one shared
pointer — with a single callable case kept for the few draws the vocabulary cannot name. The size
logic the alignment idea proposes slots into the same vocabulary as `Size`, `Sized` and an `Aligned`
node, so the alignment change lands on a value instead of a stack.

## What the stack costs today

- The drawing factories in [Drawing.cpp](../../libraries/Gfx/src/Drawing.cpp) each answer a lambda:
  `text`, `icon` in both forms, `inverted`, `underlay`, `progressBar`, `scrollbar` and `duration`.
  `duration` captures a `std::string`; the rest capture a `Rect` and their arguments. A
  `std::function` is a type-erased callable whose captured state goes to the heap once it outgrows
  the small buffer, and on this toolchain that buffer is a few words — a captured `std::vector`, a
  captured `std::string` or a captured copy of `SystemState` is an allocation.
- `arrange` in [Layout.cpp](../../libraries/Gfx/src/Layout.cpp) returns a lambda that holds a
  `std::vector<Child>`, and each `Child` holds a `Paint`. Building a layout with N children is the
  vector plus a closure for every child paint that does not fit the buffer. Every draw of that
  layout also allocates a fresh `std::vector<int16_t> spans`, so a list page allocates twice a
  frame.
- The pages add their own layers. `EditorScene::pageComponents` captures `this` twice, once for the
  title and once for the hint. `BoolScene::chip` and `DurationScene::field` measure a label, wrap it
  in a lambda that builds `inverted(text(...))`, and centre that through a one-child column.
  `NumberScene::numberBand` wraps `progressBar` in a lambda only to size it to the cell it lands in.
  `ChoiceScene::pageContents` and `SettingsScene::drawPage` build paints per row as they draw.
- The Berry boundary turns every node into a closure. `leaf()` in `DrawTree.cpp` is one closure
  around a `Make` lambda, so a `text` node is two `std::function`s before it is drawn, and
  `childrenOf` collects them into a vector the layout closure then copies.
- The alignment idea, built on this, would make each centred string `aligned(Sized{ink, text(...)})`:
  three nested closures where one value would do.

The shape vocabulary already shows the way out. Every leaf draw in
[Shapes.h](../../libraries/Gfx/src/Shapes.h) is a value struct that carries its arguments and runs
through `operator()(Canvas&)` — `Line`, `Rect`, `Text`, `MonoBitmap`. The closures live one layer
up, in the composition: the factories, the layout and the pages.
