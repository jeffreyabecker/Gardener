# Components and layout

This is the contract for the drawing primitives a page composes and the layout that places them:
the box every component draws in, the way text and the other primitives size and draw themselves,
and the rules a page's layout follows so a change to the content reflows the page instead of asking
the author for new pixel numbers. It is the contract for `/system/components/`.

What it does not cover: the pixel calls the components make are [bindings.md](./bindings.md)
(`screen`); the lifecycle that calls `draw` is [scenes.md](./scenes.md); what a routine is and how
it registers is [routine.md](./routine.md).

## The panel

The panel is `screen.width()` by `screen.height()`, one bit per pixel, origin top left.
Coordinates are whole pixels; a box is `x, y, w, h`, with `x, y` its top-left corner and `w, h` its
width and height. Nothing reaches the glass until `screen.flush()`.

The overlay owns the bottom 12 rows — the unit, the two ways in, the run state and the battery —
and is drawn last, on top of whatever a page drew there. A page lays out in the
`screen.width()` × (`screen.height()` − 12) above it.

## The box

Every component has a box: `x, y, w, h`, whole pixels, with `x, y` its top-left
corner and `w, h` its width and height. A component draws inside its box, and nothing it draws lies
outside it.

`inverted`, when true, flips the whole box as the last thing a draw does: everything the component
drew inside the box reads as paper on ink — a selected row, a cursor or a focused readout. A
component that returns before drawing leaves its box alone, so nothing is highlighted.

## Layout

A layout is a component whose box holds other components. It decides each child's box and draws the
children in order. A page does not carry pixel numbers: it composes its components into layouts and
states how the parts relate, and the layout derives every box from the panel and the components'
content, so a change to the content reflows the page without new pixel numbers.

- `children` — the components in the layout, in order.
- `fill_direction` — the order children are placed, and the way they wrap: `lr-tb` (default, left
  to right then the next line below), `rl-tb` (right to left then the next line below), `tb-lr`
  (top to bottom then the next column right), or `bt-lr` (bottom to top then the next column
  right).
- `spacing` — the gap in pixels between children along the fill direction. Default 0.
- `wrap` — with `wrap` true a layout carries a measured child onto the next line when it no longer
  fits the remaining space, in the wrap direction the fill direction names. Each line or column is
  separated by the same `spacing`. Default false.

A layout arranges its children in two passes.

**Measure.** Each child reports its natural size along the fill direction: an icon its intrinsic
`w`, a bubble field the width its count needs, a nested layout its own measured size. A text reports
the ink box of its string at the largest face that fits the layout's cross size — the line's height
for a horizontal fill, the column's width for a vertical fill — so a word in a line of height 8
measures at the built-in 5×7.

**Place.** Measured children keep their measured size along the fill direction. Growing children
share, in equal parts, the space left after every measured child: two growing children make equal
columns, and a measured word followed by a growing bar gives the bar the rest of the line. Every
child is stretched to the layout's cross size, and the child's own alignment places its ink inside
that box — a text centres by its `align` and `v_align`. A child given less than it measured fits
itself into the box: a text shrinks its face, and a child that cannot shrink draws clipped to its
box.

Children are placed in fill direction order, separated by `spacing`; space left after them stays at
the end. In a wrapping layout each line is as tall as its tallest child and each column as wide as
its widest child, and a growing child fills the rest of its own line or column. A child's box never
extends outside the layout's box.

A layout draws by drawing its children in order. `inverted` flips the whole layout box last, so a
highlighted layout reads as one unit. A layout's own natural size is the sum of its children's
sizes along the fill direction plus the `spacing` between them, and the largest child across it.

Every box lies inside the panel; two boxes do not overlap unless one component is meant to draw over
the other (none does); and a derived box has non-negative width and height.

## Text

`Text` is the text component. It draws a string in its box in the
largest face that fits.

- `family` — `sans` (default), `mono`, `serif` or `lcd`. `serif` falls back to `sans` and `lcd` to
  `mono`, because only the sans and mono families are compiled.
- `style` — `none` (default) or `bold`. `oblique` falls back to `none` and `bold-oblique` to `bold`.

The candidate faces, tried in this order:

1. compiled face, 24 px (bold when the style is bold)
2. compiled face, 18 px
3. built-in 5×7, size 3
4. compiled face, 12 px
5. built-in 5×7, size 2
6. compiled face, 9 px
7. built-in 5×7, size 1

`fit(text)` is the first candidate whose ink box — from `screen.text_bounds` — fits the box, and
the last candidate when none fits.

The last candidate is the built-in 5×7 at size 1: 5×7 px per glyph, six pixels of advance, so a
five-letter word measures 29 px. A string whose ink box is still wider than the box at that face is
cut short with a trailing ellipsis at the box's edge. Nothing `Text` draws lies outside its
box.

Alignment places the ink box in the box: `align` is `left`, `center` (default) or `right`;
`v_align` is `top`, `center` (default) or `bottom`.

`draw(text)` fits the string once and caches the face it chose; the fit is recomputed only when the
string differs from the one it last fitted. It draws in the box and flips.

## PressureDisplay

`PressureDisplay` is the reading formatted in the panel's unit. It takes a pressure in kPa and
draws the number filling its box, using `Text`'s fit.

- A nil reading draws `--`.
- The number is the kPa value scaled by the unit's `per_kpa`, with the unit's decimals: one decimal
  for a unit that steps in tenths (kPa, psi, inHg), none for a unit that steps whole (mmHg).
- The number carries no suffix of its own; the unit is the overlay's, in the bottom band.

## ProgressBar

`ProgressBar.draw(value)` is a framed bar filled to a percentage. It draws a one-pixel frame around
the box and fills the inside: the value is clamped to 0–100, and the fill is the left
`(w − 2) · value / 100` pixels inside the frame. A value of 0 draws the frame and no fill.

## Bubble

`Bubble` is a square drawn to fill its box, filled or outlined.

- `shape` — `circle` (default), `rect`, `roundrect`, `tri-up`, `tri-down`, `tri-left` or `tri-right`.
- `filled` — true draws the shape solid; false draws its outline.
- Smaller than 3 px it is a solid square, with no outline and no shape.

## ProgressBubbles

`ProgressBubbles.draw(count)` is a wrapping layout of `max_count` `Bubble` children, one per set,
whose `fill_direction` is the field's. The first `count` children in fill order are filled; `count`
is clamped to 0–`max_count`.

- `unfilled` — how an unfilled bubble draws: `outline` (default) or `none`; `none` skips them.
- `fill_direction` — the layout's fill direction: `lr-tb` (default), `rl-tb`, `tb-lr` or `bt-lr`.

Each bubble measures as the largest whole square that lets the count fill the box: columns
`⌊√(max_count · w / h)⌋` clamped to 1–`max_count`, rows `⌈max_count / columns⌉`, and the size
`min(⌊w / columns⌋, ⌊h / rows⌋)`, at least 1 px. The leftover width and height become the `spacing`
between bubbles.

## Scrollbar

`Scrollbar.draw(index, count, visible)` is the thumb that shows where a window sits in a list.

- The thumb's height is `⌊h · visible / count⌋`, clamped to 1–`h`.
- Its top is placed across the span `count − visible`, so index 0 is the top and the last index the
  bottom, and clamped inside the box.
- `count ≤ 0` draws nothing.
- `hollow` inverts the two ends of the track; otherwise the component draws a frame around the box
  and a filled thumb.

## Triangle

`Triangle` is an arrow in one of the four directions, filled or empty.

- `direction` — `up` (default), `down`, `left` or `right`.
- `style` — `filled` (default) or `empty`.
- It is drawn to fill its box.

## Icon

`Icon` is a component with an intrinsic size. `w` and `h` are the size the icon is drawn at, and
they are the box the layout gives it: `draw()` draws the icon in the box, and it flips when
inverted. The browser puts one in its box at the left of a row and shifts the title right by its
width.
