# How should a child be placed inside the box it is given?

`drawCentred` measures a string and draws it in the middle of the canvas it is handed, and it is the
only thing in the tree that places something smaller than the cell it lands in. A layout can only be
made to do it by hand: a `Cell` declares a share of the parent, never the child's own size, and a
`Justification` places the run of cells rather than one child in its share, so the page measures the
string first, declares a cell of the ink and wraps it in a one-child column. Three pages do exactly
that. The question is what one function would replace both spellings, if it had to work on any child
rather than on a string, and had to name both axes rather than only the middle.

## The two spellings, and what each costs

`drawCentred` in [Drawing.cpp](../../libraries/Gfx/src/Drawing.cpp) is a call and not a `Paint`, so
it cannot be composed: `inverted` and `underlay` take a `Paint`, and nothing can take a call.
`DurationScene` writes `centredText` as an adapter whose only job is to make the call into a paint.
It also measures inside the call, so a centred string on a page that redraws is measured once a
frame — two or three times a frame in `PressureReadout`.

The hand-written spelling, in `BoolScene::chip`, `DurationScene::field` and `NumberScene`'s
`numberBand`, does answer a `Paint`, so `inverted` wraps it. It works on one axis only: the column
centres the label along the stacking axis, `justification.across` centres it on the other, and with
two children the `along` placement centres the run of them rather than each child. It also spells
the ink box twice, once as a `Cell` and once as the rect the text is drawn in.

Both compute the same thing, `(width - ink.width) / 2`. `alignedOffset` in
[Layout.cpp](../../libraries/Gfx/src/Layout.cpp) already computes it and puts an odd pixel at the
end the same way, so a function built on it reproduces the pixels `drawCentred` makes for ink that
fits.

## The constraint that shapes everything: a paint has no size

`Paint` is a `std::function<void(Canvas&)>` — it can be run, and cannot be asked how big it is. Most
of the vocabulary fills the box it is given: `progressBar`, `scrollbar`, a nested layout, a
hand-written draw. For those there is no leftover to divide, so "aligned" means nothing. Only two
things know their own ink without being told: a string, through `shapes::measureText`, and a
hex-named glyph, whose width and height are the first two bytes of the bitmap it names.

So the function cannot take any child. It has to take a child whose size somebody knows, and the
real design question is where that size comes from.

## The options

**The size as arguments, no new type.** `Paint aligned(shapes::TextBox ink, Paint child, Justify
horizontal, Justify vertical)`, drawing on the canvas it is handed as `inverted` and `underlay` do
rather than filling a box of a bigger canvas as the factories do. It removes the one-child-column
trick and the `Cell` arithmetic and nothing else. What it costs is that the ink box is stated by the
caller and can disagree with the paint, and that the call site is hardly shorter than the form it
replaces.

**A size beside the paint.** `struct Sized { shapes::TextBox ink; Paint paint; };` with measuring
twins — `Sized text(const char* value, const Font* face, uint8_t scale)` beside the `Paint text(Rect
box, …)` that exists, and an escape `Sized sized(Paint child, shapes::TextBox ink)` for a child
whose size the caller states. Then `aligned(Sized child, Justify horizontal, Justify vertical)`, a
page writes `aligned(text("4m", nullptr, 2))`, and `inverted(aligned(…))` is the chip. It buys one
source of truth for the ink box and one measurement per construction instead of one per frame. It
costs a second name for a thing that draws, and an overload of `text` told apart only by its first
argument.

**Alignment on the child in a layout.** A `Justify` pair on `Child`, beside its `Cell`. It is the
shape that reads most like a browser's `align-self`, but the child still has to declare a pixel cell
to be smaller than the cell it lands in, so the caller's measurement stays, and the pair only means
anything for children that declared a size.

**A paint that answers its own size.** A `Drawn` of size and callable in place of `Paint`. Ruled out
by the constraint above: most draws fill their box and would have to invent a size, which the
alignment would then act on as if it were true. It would also put a second `std::function` in every
node.

**Alignment on `text` alone.** Two `Justify` arguments on the existing `text` — the narrow version,
and the one the Berry boundary would take most cleanly. It does not generalise, which is half of
the question, so it is not the answer; it is what the boundary can have if the general function
proves too much.

## How the two axes are named

`Justify` already has the three values the arithmetic needs, and spells the middle `Center`; nothing
new is required. The choice is how a pair of them is passed.

`Justification` is close at hand but wrong here: `along` and `across` mean the stacking axis and the
other one, and placement inside a box has no stacking axis. A new record `Placement{horizontal,
vertical}` reads well in the header and is filled field by field at the call site — the verbosity
the `centred()` helpers in `BoolScene` and `DurationScene` already are. Two parameters read in the
order the position does, as in a `Rect`, and leave the parameter names to carry the meaning:

```
aligned(text("4m", nullptr, 2), Justify::Center, Justify::End)
```

Nine combinations then cost no table of names. The cost is that a transposed pair compiles, and is
only seen on the panel.

## What has to be true for the preferred one

- `Sized` needs an ink box, and `shapes::TextBox` is that box. Either `Drawing.h` includes
  `Text.h`, or the pair of numbers moves down beside `Rect`. Which of the two is not settled.
- `text` gains the measuring twin: one call that answers the ink box beside the paint, measuring
  exactly as `measureText` already does and drawing nothing.
- The three hand-written cells go, and with them the `centred()` helpers, which are there to spell
  the justification a layout cannot omit, and `DurationScene::centredText`, which is there only to
  turn the call into a paint.
- The Berry side is the larger half. `screen.centred` in
  [Screen.cpp](../../libraries/BerryHost/src/Screen.cpp) and the `"centred"` kind in
  [DrawTree.cpp](../../libraries/BerryHost/src/DrawTree.cpp) are one node to fold, and six `.be`
  lines under `firmware/data/` call it. `screen.measure` already answers an ink box, so a
  `screen.aligned(draw, along, across)` node would need no new measuring — but no `.be` page aligns
  anything that is not a string, so nothing asks for one yet.
- `drawCentred` leaves rather than sitting beside the new function. Its nine calls in four pages
  then read as the new one does, and the case "a string is centred on its canvas" in
  `tools/unit/cases/gfx_drawing.cpp` should pass unchanged on the new path, because the arithmetic
  is the same — that is the observation that says the general function is a superset rather than a
  rewrite.
- Ink wider than its box changes behaviour. `alignedOffset` answers zero for a negative leftover, so
  a too-wide string starts at the left edge and is clipped at the right, where `drawCentred` starts
  it off the left edge and clips both ends.

## What is still unknown

- Whether the type is earned. Every aligned child in the tree today is a string, so `Sized` is a bet
  on a page nobody has written — an icon in a cell wider than the glyph, a fixed block of some
  other kind. The cheaper shape costs nothing if no such page arrives.
- Whether `icon` should have a twin. Nothing centres one; `sized(icon(rect, glyph), ink)` covers it
  until something does, and measuring a hex string at construction is work nothing has asked for.
- Whether `aligned` belongs in `Layout.h`, where the alignment vocabulary and `alignedOffset` live,
  or in `Drawing.h` beside `inverted` and `underlay`, where the wraps live. It divides nothing, and
  it is a wrap.
- Whether the Berry node should keep a kind named for the placement, or the two names fold into
  `text` and the kind disappears.

## Recommendation

Take the size beside the paint: `Sized` in `Drawing.h`, a measuring `text` overload and a `sized`
escape, and `aligned(Sized, Justify horizontal, Justify vertical)` in `Layout.h` defaulting to
`Center` on both axes so that the common case is one word. Delete `drawCentred` and the three
hand-written cells, and fold `screen.centred` into the `text` node's two alignment names rather than
inventing an `aligned` node nothing calls — the narrow shape the C++ side sets aside, but the one
the `.be` pages, which centre only strings, actually ask for.

Held moderately. The axes and the arithmetic are not in doubt — the three values already exist and
`alignedOffset` already answers them — and the migration is small on the C++ side. What is in doubt
is the type: it is bought for a general case the tree has not shown it needs, and the size-as-
arguments shape is the fallback if nothing but text ever gets aligned.
