# How should a child be placed inside the box it is given?

## In short

A centred title is a lambda around a call today, because `drawCentered` is not a `Paint`:

```
gfx::Child{gfx::Cell(1.0f, TitleBandPct),
           [this](gfx::Canvas& canvas) { gfx::drawCentered(canvas, title()); }},
```

One function replaces that lambda and the measured cell the other pages write by hand:

```
gfx::Child{gfx::Cell(1.0f, TitleBandPct), gfx::aligned(gfx::text(title()))},
```

`aligned` takes an ink box with the paint that draws in it — that pair is `Sized` — and two
`Justify`s, horizontal first and both `Center` unless the page says otherwise:

```
// A paint and the box it draws in, where that box is not the canvas it is handed.
struct Sized {
  Sized(Size ink, Paint paint) : ink(ink), paint(std::move(paint)) {}

  Size ink;
  Paint paint;
};

// The child drawn at the place the two justifications name, horizontal first, as a paint that
// fills the canvas it is handed.
Paint aligned(Sized child, Justify horizontal = Justify::Center,
              Justify vertical = Justify::Center);
```

`Size` is the two numbers a thing is wide and tall, and belongs beside `Rect` in the geometry header
every drawing already includes: a `Rect` converts to one, `measureText` can answer one, and a
bitmap's first two bytes are one. So `shapes::TextBox` goes — it is that type under a narrower name,
in the wrong namespace. `text(value, face, scale)` answers a `Sized` by measuring the string it is
about to draw, so the box and the draw cannot disagree; `sized(paint, size)` makes one for a child
whose size the caller already knows. Delete `drawCentered` and the three measured cells, and fold
the Berry `centred` node into `text` with the same two names.

`Paint` does not need to change for that. A size carried on `Paint` is four bytes and no edits
anywhere, but it asks every paint and every node kind a question that only alignment uses, and a
wrong answer is silent. Measuring a paint by drawing it into a recorder needs no vocabulary at all,
and is refused on `Canvas::draw`'s promise that a shape is called once and not kept.

What is settled is the arithmetic and the words: `alignedOffset` already answers the one, and two
`Justify`s already name the other. What is not settled is the type — `Sized` is a bet until a page
wants to align something that is not a string, and if one arrives the ink moves onto `Paint` and
`Sized` goes rather than sitting beside it.

`drawCentered` measures a string and draws it in the middle of the canvas it is handed, and it is
the only thing in the tree that places something smaller than the cell it lands in. A layout can
only be made to do it by hand: a `Cell` declares a share of the parent, never the child's own size,
and a `Justification` places the run of cells rather than one child in its share, so the page
measures the string first, declares a cell of the ink and wraps it in a one-child column. Three
pages do exactly that. The question is what one function would replace both spellings, if it had to
work on any child rather than on a string, and had to name both axes rather than only the middle.

## The two spellings, and what each costs

`drawCentered` in [Drawing.cpp](../../libraries/Gfx/src/Drawing.cpp) is a call and not a `Paint`, so
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
end the same way, so a function built on it reproduces the pixels `drawCentered` makes for ink that
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

**The size as arguments, no new type.** `aligned(ink, child, horizontal, vertical)`: the box and the
paint both stated at the call. It removes the one-child-column trick and the `Cell` arithmetic and
nothing else, and it leaves the page measuring by hand what `aligned(text(…))` measures for it, in a
place where the number can disagree with the paint.

**A size beside the paint.** The pair `aligned` takes above. `Sized` holds a `Size`, which is
whatever the source happens to know: `text` and a hex-named `icon` answer a `Sized` beside the
`Paint` they answer now, measuring what they are about to draw; `sized(paint, size)` covers
everything else, and a caller with the numbers in hand writes `sized(paint, Size(width, height))`.
It is one word at the call site, one source of truth for the box, and a measurement at construction
instead of one a frame. It owes `inverted` and `underlay` the same overload, or the chip that
inverts its own label — `aligned(inverted(text("OFF", nullptr, 2)))` — goes back to measuring by
hand.

**A size that is whatever the caller has.** `Sized` a template over its box and `aligned` a template
over that, so any struct with a `width` and a `height` is a size and nothing converts:

```
template <typename Ink>
Paint aligned(Sized<Ink> child, Justify horizontal, Justify vertical) { ... }
```

It buys a `Rect`, a `shapes::TextBox` and a page's own metrics passing as they are. It costs the
body in the header, where every other function in `Layout.h` is a declaration; an instantiation per
ink type, each one a closure and a `std::function`; and a return type of `Sized<shapes::TextBox>` on
`text`, which drags the shapes header into `Drawing.h` — the include that a two-number `Size` beside
`Rect` avoids. What it does not buy is the generality that matters: whatever the type, the numbers
have to exist when the paint is built, and every source that has them answers two integers.

**Alignment on the child in a layout.** A `Justify` pair on `Child`, beside its `Cell`. It is the
shape that reads most like a browser's `align-self`, but the child still has to declare a pixel cell
to be smaller than the cell it lands in, so the caller's measurement stays, and the pair only means
anything for children that declared a size.

**A paint that carries its ink.** `Paint` a small struct instead of an alias: the callable, and the
box it draws in where that box is not simply the canvas it is handed.

The version where every paint states a size is the one the constraint above forbids, and it is the
version worth refusing: most draws fill the box they are given and would have to invent a number,
which the alignment would then act on as though it were true. The version where a *missing* size
means the canvas invents nothing. `progressBar`, `scrollbar` and a nested layout are already the
canvas they are handed, so they answer nothing and need no edit; `text`, a hex glyph and a
`duration` answer their ink, which they can measure as they build themselves. Then `aligned` is five
lines and no new type, `inverted` and `underlay` answer their child's ink — so
`aligned(inverted(text("OFF", nullptr, 2)))` inverts the string rather than the cell — and
`BoolScene`'s chip and the three measured cells collapse to one call.

What it costs is not room. The field is four bytes on a `Paint` — 32 to 40 where `std::function` was
measured — and no site has to change: a paint built from a callable is still a paint, and its ink is
empty. What it costs is a claim nothing checks. Today a draw and a number beside it cannot disagree,
because only `text` knows its size and only `text` uses it. With a field, every paint the tree
builds and every node kind the draw tree answers is a place to answer the question wrongly, and a
paint that reports the cell it fills while drawing a small glyph is aligned to a number that is a
lie. No case in the suite can see the difference.

**A paint measured by drawing it.** Nothing states a size: `aligned` runs the child against a canvas
that writes no pixels and answers the box it was written to, then runs it again placed by that box.
The device already keeps such a rect for the flush, so the shape is in the tree, and a recorder is
thirty lines; alignment would then be right for any child, with no type change and no site touched,
and a draw could not disagree with its own measurement. It costs a second run of every aligned paint
on every repaint — four times a second here — including whatever formatting and allocation the paint
does as it draws, and it contradicts the contract `Canvas::draw` states, that a shape "is taken by
value … and it is called once and not kept". Ruled out by that contract rather than by the seconds.

**Alignment on `text` alone.** Two `Justify` arguments on the existing `text` — the narrow version,
and the one the Berry boundary would take most cleanly. It does not generalise, which is half of
the question, so it is not the answer; it is what the boundary can have if the general function
proves too much.

## How the two axes are named

`Justify` already has the three values and spells the middle `Center`, so nothing new is needed; the
question is only how a pair is passed. The tree has been here before — `Horizontal{Left, Center,
Right}` and `Vertical{Top, Center, Bottom}` in one `Alignment` record, replaced because one field
meant the block's placement in a row and a cell's placement in a column. A box infers no direction,
so that trouble does not return; but a record is still filled field by field at the call site, which
is exactly what the `centred()` helpers in `BoolScene` and `DurationScene` are. Two parameters read
as the position does, as in a `Rect`:

```
aligned(text("4m"), Justify::Center, Justify::End)
```

Nine combinations then cost no table of names. The cost is that a transposed pair compiles, and is
only seen on the panel.

## What has to be true for the preferred one

- `shapes::TextBox` becomes `gfx::Size` beside `Rect`, and `measureText` answers it: the header,
  `Text.cpp`, `Drawing.cpp`, the three scenes that measure a label and `Screen.cpp`, every one of
  which already sees `Rect.h`, so nothing new is included anywhere.
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
- `drawCentered` leaves rather than sitting beside the new function. Its nine calls in four scenes
  then read as the new one does, and the case "a string is centred on its canvas" in
  `tools/unit/cases/gfx_drawing.cpp` should pass unchanged on the new path, because the arithmetic
  is the same — that is the observation that says the general function is a superset rather than a
  rewrite.
- Ink wider than its box changes behaviour. `alignedOffset` answers zero for a negative leftover, so
  a too-wide string starts at the left edge and is clipped at the right, where `drawCentered` starts
  it off the left edge and clips both ends.

## What is still unknown

- Whether the type is earned. Every aligned child in the tree today is a string, so `Sized` is a bet
  on a page nobody has written — an icon in a cell wider than the glyph, a fixed block of some
  other kind. The cheaper shape costs nothing if no such page arrives.
- Whether the size is the paint's or the alignment's. Putting the ink on `Paint` is four bytes and
  no edits, and it makes `aligned` a function with no vocabulary; keeping it in `Sized` means the
  only paints that answer the question are the ones being aligned. What would decide it is a second
  consumer of "how big is this thing" — a hit region, a fit check, a scroll extent — and there is
  none.
- Whether `icon` should answer a `Sized` too, reading the width and height out of the bitmap it
  names. Nothing centres one, and until something does an icon takes the cell it is given.
- Whether a size is ever more than two numbers. `Size` says how wide and how tall and nothing else,
  so a child whose baseline or optical centre mattered could not say so; the templated `Sized` is
  what to reach for when one does, and nothing wants it now.
- Whether `aligned` belongs in `Layout.h`, where the alignment vocabulary and `alignedOffset` live,
  or in `Drawing.h` beside `inverted` and `underlay`, where the wraps live. It divides nothing, and
  it is a wrap.
- Whether one enum should serve both readings. `Justify` would then mean a layout's leftover in a
  layout and a box's leftover in a box, and the signature says which; a second enum that names the
  same three words would say it at the type, and cost a conversion wherever the two meet.
- Whether the Berry node should keep a kind named for the placement, or the two names fold into
  `text` and the kind disappears.

## Recommendation

Take the size beside the paint: `Size` beside `Rect` in place of `shapes::TextBox`, `Sized` in
`Drawing.h` holding one of those and a paint, a measuring `text` overload and a `sized` escape, and
`aligned(Sized, Justify horizontal, Justify vertical)` in `Layout.h` defaulting to `Center` on both
axes so that the common case is one word. `Sized` stays one type over one `Size` rather than a
template: the alignment consumes two integers, and a template would move the body into the header
and instantiate once per caller to save a conversion nobody minds. Delete `drawCentered` and the
three hand-written cells, and fold `screen.centred` into the `text` node's two alignment names
rather than inventing an `aligned` node nothing calls — the narrow shape the C++ side sets aside,
but the one the `.be` pages, which centre only strings, actually ask for.

Held moderately. The axes and the arithmetic are not in doubt — the three values already exist and
`alignedOffset` already answers them — and the migration is small on the C++ side. What is in doubt
is the type: it is bought for a general case the tree has not shown it needs, and the size-as-
arguments shape is the fallback if nothing but text ever gets aligned.

The one change worth making to `Paint` is the ink field, and it is refused here for a reason and
not for its price: if alignment is ever wanted on a child the caller did not build — a glyph in a
cell wider than the glyph, a block of some other kind — the field is the change to make, and `Sized`
is deleted rather than kept beside it.
