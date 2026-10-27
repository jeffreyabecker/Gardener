# Components and layout

This is the contract for the drawing primitives a page composes and the layout that holds them: the
box every component draws in, the way text and the other primitives size and draw themselves, and
the relative layout a page builds instead of deriving pixel numbers. It is the contract for
`/system/components/`.

What it does not cover: the pixel calls the components make are [bindings.md](./bindings.md)
(`screen`); the lifecycle that calls `draw` is [scenes.md](./scenes.md); what a routine is and how
it registers is [routine.md](./routine.md).

## The panel

The panel is `screen.width()` by `screen.height()`, one bit per pixel, origin top left. Coordinates
are whole pixels. Nothing reaches the glass until `screen.flush()`.

The `ux` component owns the bottom 12 rows — the unit, the two ways in, the run state and the
battery — and draws them last, on top of whatever a page drew there. A page lays out in the panel
less that band: `bounds(screen.page_width(), screen.page_height() - overlay_band_height)`, which is
the box every scene is given.

## The box

Every component has a box: a `point` and a `bounds`, carried in `pos` and `dim`. `pos.x`, `pos.y` is
the top-left corner and `dim.w`, `dim.h` the width and height, whole pixels. Both are records
declared in `/system/records.be` — the fields and an `init`, nothing else. The box is not a
constructor argument: `set_box(pos, dim)` sets both halves together, after the component is built
and before anything draws it. A layout calls it for every child it places; a caller calls it for a
component that stands alone, such as a settings editor's inline value.
Nothing a component draws lies outside its box, and a component never draws before it has one. The
one thing that reads past a box is a string whose face the caller named too large for the cell: it is
drawn whole rather than cut, so a face is the caller's promise that the string fits.

`use(w, h)` answers the room the component takes in a cell of that size: the cell itself for a
component that draws across the whole of it, and its ink for a string, which is what lets the layout
holding it put the box where its alignment says. A caller that has to know the room a string takes —
a pressure display sizing its secondary reading — calls it too.

A page states the numbers it means rather than a partial box: the page is the panel less the band,
`bounds(screen.page_width(), screen.page_height() - overlay_band_height)`, and a whole-panel box is
`bounds(screen.width(), screen.height())`.

`inverted`, when true, flips the whole box as the last thing a draw does: everything the component
drew inside the box reads as paper on ink — a selected row, a cursor or a focused readout. Every
draw ends in `super(self).draw()`, so the flip is always the last thing. A component that returns
before drawing leaves its box alone, so nothing is highlighted.

## Layout

`AutoLayout` is a component with a box that resolves the box of each child from the cell that child
declares, places it where the layout's alignment says, and draws the children in order. It is the one
way a page states where its parts go; a caller no longer works out `pos.x + 16` or `dim.w / 2`.

A child declares its cell as `bounds(w, h)` — the share of the parent's width and the share of its
height the child occupies. Each is a number in [0, 1], a number of pixels when it is above 1, or nil
for the whole of the box it is given. It is the same record a component's `dim` is, and the one
`screen.text_bounds` answers — which is why `bounds(12, 8)` is twelve by eight pixels and `bounds(1,
0.25)` is the full width and a quarter of the height.

`alignment(align, v_align)` is a record in `/system/records.be` holding where a thing sits in the
space it is given: `align` is `left`, `center` or `right`, `v_align` `top`, `center` or `bottom`, and
each is `left` and `top` when it is nil. The layout takes one after its children —
`AutoLayout(bounds, children, alignment)` — and a layout built without one is `alignment("left",
"top")`, the corner nothing has to be moved from.

- `to_box(x, y, w, h)` places a layout: the parent that holds it calls it, and the scene a page is
  composed in calls it with the page — the panel less the band — as the scene is pushed. A layout
  places nothing, itself included, and nothing draws one that has not been placed.
- Siblings stack along the axis they divide: a layout whose children are each the full width stacks
  them down a column, and one whose children are each not the full width stacks them across a row.
- A resolved span is the share rounded to the nearest pixel, and clamped to what is left of the
  parent, so nothing a `bounds` declares can place a child outside it. A row declared as a fraction
  of the height it was measured from resolves back to the pixels it declares, and equal thirds of a
  row fill it. A declaration above 1 is that many pixels, clamped to the box the same way.
- Alignment applies at both levels the box resolves at: the stack of children inside the layout's box
  when they do not fill it, and a child's own room inside its cell. A child that fills its cell does
  not move, so a layout that declares neither alignment nor a smaller child resolves to exactly the
  same pixels with or without one.
- A component that declares no bounds is a leaf and takes the layout's box whole, so a layout that
  is not a row holds one such component. A leaf's room is the whole of its cell; a string's is its
  ink, so a text is placed by the alignment and its `dim` is the ink it draws.
- The layout is itself a component, so its own draw ends in `super(self).draw()` like any other.

`children` is the list handed to `init`, and a caller may give a layout a different list before it
draws — a pressure display swaps the readings its two halves hold. A layout resolves its children on
every draw, from the box it holds, so a box that changes afterwards — or a value that grows — is
resolved again.

## Text

`Text` draws a string at the face it was built with. It takes `value` and `font` at `init`; its box is
set with `set_box()` like any other component's, and it draws at that box — where the box sits is the
alignment of the layout that holds it.

- `font` — a face name from the binding's own table: `sans9`, `sans12`, `sans18`, `sans24`,
  `sansbold18`, `mono12`, `tomthumb` and the rest of [bindings.md](./bindings.md). Nil or an empty
  string is the built-in 5×7, the face that carries the arrow glyphs a hint prints.
- `scale` — the size the face is drawn at, 1 (the default) for the face as designed and 3 for three
  times as tall, whichever face is set.
- `value` — the string drawn, `""` when it is nil.

There is no fitting and no ladder of faces: the caller named the face, so the string is drawn at it.
A compiled face is drawn from its baseline, which is the ink box's height below the box's top; the
built-in face is drawn from its top.

`use(w, h)` answers the room the string takes at its face — the ink box `screen.text_bounds` answers,
no larger than the cell — which is what a layout places the text by. Nothing cuts or wraps a string:
a face named too large for the cell is the caller's choice, and the string is drawn whole and reads
past the box rather than losing letters.

The image itself draws every string in the built-in face: the chrome's title and status rows, the
timers, the set count, the editor's label and hint rows and the list's rows at scale 1, a value in an
editor and the pressure reading at scale 2. The scale is picked per region from the string each one
prints — the built-in face is six pixels a character per scale against the eight it is tall, so a
64-pixel cell holds five characters at 2 and seven at 1.

## PressureDisplay

`PressureDisplay` is the reading formatted in the panel's unit, drawing the number filling its box.
Its readings are placed by layouts of its own: the number filling the box, the secondary alone in the
box so an alignment can put it in a corner, and two halves when the pair sits side by side.

- `pressure_kpa` — the main reading in kPa, set before `draw()`; `draw()` takes no arguments.
- A nil reading draws `--`.
- The number is the kPa value scaled by the unit's `per_kpa`, with the unit's decimals.
- The number carries no suffix of its own; the unit is the band's, in the bottom band.
- `scale`, `secondary_scale` — the size each number is drawn at in the built-in face, `2` and `1`
  when they are nil.
- `secondary_pressure` — a second pressure in kPa, or nil (default), drawn per `secondary_position`
  and `size_hint`.
- `secondary_position` — `tl`, `tr`, `bl` or `br` (default `br`).
- `size_hint` — below 1 the secondary is drawn in its corner at `secondary_scale`; at 1 the two
  numbers sit side by side at `scale`.

`AutoPressureDisplay` is a `PressureDisplay` that reads its own numbers from the machine: it takes a
`pressure_order` — `"pressure:target"` (pressure primary, target secondary) or `"target:pressure"`
— and its `draw()` sets `pressure_kpa` and `secondary_pressure` from the machine before drawing.

## Duration

`Duration` is whole seconds formatted in the panel's units of time. It is a `Text`, so it takes the
face it is drawn at after its `format`. `seconds` is the count it draws, and `draw()` formats and
draws it like `Text`. `format` is `minutes` (default), `seconds` or `hours`. `text(seconds)` formats
without drawing: `2m30s`, `1m` on a whole minute, `45s` under a minute, `1h2m5s`, `0s` for nothing
and `--` for nil.

## ProgressBar

`ProgressBar` is a framed bar filled to a percentage. `value` is clamped to 0–100; the fill is the
left `(w − 2) · value / 100` pixels inside the one-pixel frame. 0 draws the frame and no fill.

## Scrollbar

`Scrollbar` is the thumb that shows where a window sits in a list. `index`, `count` and `visible`
are set before `draw()`: the thumb's height is `⌊h · visible / count⌋` clamped to 1–`h`, placed
across the span `count − visible` and clamped inside the box. `count ≤ 0` draws nothing; `hollow`
inverts the two ends of the track, otherwise the component draws a frame and a filled thumb.

## Icon

`Icon` is a component that draws a glyph in its box. A glyph subclass draws its own ink inside it.
An icon carries the 12 by 8 measure every glyph is drawn on — `Icon.width` and `Icon.height` — until
a layout gives it a box of its own. A list row is that measure, two pixels of paper and the rest of
the row, and the browser puts one at the left of a row for its title to follow.