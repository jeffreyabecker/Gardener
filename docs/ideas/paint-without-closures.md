# How should a paint hold what it draws without a closure stack?

`Paint` is a `std::function<void(Canvas&)>`, so the drawing language is a language of closures:
each factory in [Drawing.h](../../libraries/Gfx/src/Drawing.h) returns one, `layout`/`row`/`column`
return one holding a vector of them, the pages wrap those in lambdas of their own, and the Berry
walk in [DrawTree.cpp](../../libraries/BerryHost/src/DrawTree.cpp) wraps every leaf in a closure
before any of it runs. The alignment idea in `alignment-in-layout.md` proposes `Sized` and
`aligned` on top of that — two more closure layers for every centred string. The question here is
whether `Paint` should stop being a `std::function` first, so that the alignment change lands on a
value instead of a stack.

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

## The options

**Keep `Paint` a `std::function` and only thin the closures.** Land `Size`/`Sized`/`aligned` to
delete `drawCentered` and the measured cells, pre-format `duration`, drop the per-draw `spans`
vector, and build the Berry tree with fewer `leaf()` layers. The type stays type-erased, every leaf
and every layout still allocates, and the alignment change still adds a `Sized` — one more
`std::function` — per aligned child. This is the fallback if the value `Paint` proves too much, not
the answer.

**Typed nodes with templates.** Each factory returns its own struct (`TextNode`, `BarNode` and so
on) and composition nests through the types, so `inverted(text(...))` is `Inverted<TextNode>` with
no erasure at all. It falls over on the one thing the tree always does: a layout's children are
heterogeneous, so `std::vector<Child>` cannot hold `Inverted<TextNode>` beside `LayoutNode<...>`
without an erasure somewhere — which moves the `std::function` to `Child` and saves nothing — and
the bodies move into the header, where every other function in `Layout.h` is a declaration. Ruled
out by the heterogeneous-children constraint.

**A virtual node hierarchy.** A `PaintNode` base with `virtual void draw(Canvas&)` and one subclass
per kind. It removes `std::function` and replaces it with a vtable and one heap object per node —
the same allocation count, more classes, more surface. The firmware rule is that an interface earns
its place at a foreign boundary, not as a second implementation that does not exist; every kind here
exists, but they are a finite set the code already lists. Ruled out: it trades one erasure for a
bespoke one without removing the allocations.

**A finite node vocabulary in a value `Paint`.** `Paint` becomes a small value carrying one node
struct per fixed kind, with one `std::function` case kept for draws the vocabulary cannot name.
Leaves are inline and cost nothing to build; the composite nodes — icon, inverted, underlay,
aligned, layout — own their children behind one shared pointer, which is also what breaks the
recursive type. The draw is a `switch` in the `.cpp`, the way the layout's helpers already are.
This is the preferred shape, below.

**The same vocabulary in a `std::variant`.** The same nodes, erased by `std::variant` and
`std::visit` instead of a hand-rolled tag. It is cheaper to write and heavier to compile: the tree
has never used `<variant>`, the dispatch becomes a template in the header, and a visitation table is
bought for a union the code can hold by hand. Set aside as the spelling to reach for if the
hand-rolled one ever costs more code than it saves.

## The preferred shape

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

## What has to be true for the preferred one

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

## Recommendation

Take the value `Paint` with the finite node vocabulary and the one callable seam, and land it
together with the alignment idea's `Size`, `Sized` and `aligned`, because the two are one change to
the one path that runs every frame. The fallback is the closure-thinning option if the value type
proves too much for the call sites that build paints from lambdas, and the suite and the panel are
the measurement.

Held moderately. The vocabulary and the arithmetic are already in the tree as value structs —
`shapes::*` and `alignedOffset` — so this extends a shape the code already has rather than inventing
one, and what it buys is the removal of the closure stack from the hot path rather than a new
abstraction. What is not bought is certainty that the callable seam stays small: if the pages keep
writing bespoke lambdas, the seam becomes the stack again, and the honest test is whether the
factories and the Berry walk are enough of the tree to carry the design.
