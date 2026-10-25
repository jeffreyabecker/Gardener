# Components and layout

This is the contract for the drawing primitives a page composes and the layout that holds them: the
box every component draws in, the way text and the other primitives size and draw themselves, and
the flat list a page builds instead of deriving pixel numbers. It is the contract for
`/system/components/`.

What it does not cover: the pixel calls the components make are [bindings.md](./bindings.md)
(`screen`); the lifecycle that calls `draw` is [scenes.md](./scenes.md); what a routine is and how
it registers is [routine.md](./routine.md).

## The panel

The panel is `screen.width()` by `screen.height()`, one bit per pixel, origin top left. Coordinates
are whole pixels. Nothing reaches the glass until `screen.flush()`.

The overlay owns the bottom 12 rows — the unit, the two ways in, the run state and the battery — and
is drawn last, on top of whatever a page drew there. A page lays out in the
`screen.page_width()` × `screen.page_height()` area above it.

## The box

Every component has a box: a `Position` and a `Size`, carried in `pos` and `dim`. `pos.x`, `pos.y`
is the top-left corner and `dim.w`, `dim.h` the width and height, whole pixels. The box is passed to
the component's `init(pos, dim)` and kept; nothing a component draws lies outside it. A component
that draws internal subcomponents may build them with a nil box and stamp the box before drawing.

`Position` and `Size` carry the helpers a page uses to state a box: `Position.top_left()`,
`Position.top(y)`, `Position.left(x)`, `Size.width(s)`, `Size.height(s)`, `Size.page()` (the area
above the overlay) and `Size.screen()`.

`inverted`, when true, flips the whole box as the last thing a draw does: everything the component
drew inside the box reads as paper on ink — a selected row, a cursor or a focused readout. Every
draw ends in `super(self).draw()`, so the flip is always the last thing. A component that returns
before drawing leaves its box alone, so nothing is highlighted.

## Layout

`Layout` is a flat list of components and nothing more. It holds no box of its own — it has no
pixels of its own — and its `draw()` walks the children in order, calling each child's `draw()`. The
caller positions each child itself, once, when it builds the list; a layout does not measure, place
or reflow. `children` is the list and `add(child)` appends to it.

## Text

`Text` draws a string in its box in the largest face that fits, given a box (`pos`, `dim`) and a
`value` at `init`.

- `family` — `sans` (default), `mono`, `serif` or `lcd`. `serif` falls back to `sans` and `lcd` to
  `mono`, because only the sans and mono families are compiled.
- `style` — `none` (default) or `bold`. `oblique` falls back to `none` and `bold-oblique` to `bold`.
- `align` — `left`, `center` (default) or `right`; `v_align` — `top`, `center` (default) or `bottom`.

The candidate faces, tried largest first: the compiled face at 24, 18, 12 and 9 px, bold when the
style is bold, and then the built-in 5×7, the last resort.

The built-in 5×7 is used only when no compiled face fits. Bold has no built-in face: a bold built-in
string is drawn and its ink box is inverted, which is what reads as bold. A string whose ink box is
still wider than the box at that face is cut short with a trailing ellipsis at the box's edge.
Nothing `Text` draws lies outside its box.

`draw()` fits the current `value` once and caches the face it chose; the fit is recomputed only when
the string differs from the one it last fitted. `draw_face(text, candidate)` draws a string at a
face the caller already chose. `bounds(text, candidate)` answers the ink box, `fit_height(text,
max_h)` the largest face whose ink height fits a cap, and `candidate` is the face the last draw
fitted.

## PressureDisplay

`PressureDisplay` is the reading formatted in the panel's unit. It takes a pressure in kPa and draws
the number filling its box, using `Text`'s fit.

- A nil reading draws `--`.
- The number is the kPa value scaled by the unit's `per_kpa`, with the unit's decimals.
- The number carries no suffix of its own; the unit is the overlay's, in the bottom band.
- `family`, `style` — the main number's face, as in `Text`.
- `secondary_pressure` — a second pressure in kPa, or nil (default), drawn per `secondary_position`
  and `size_hint`.
- `secondary_position` — `tl`, `tr`, `bl` or `br` (default `br`).
- `size_hint` — a real from 0 to 1 (default 0.5): below 1 a hint for the secondary's size as a
  fraction of the main number's face height, drawn smaller in the corner; at 1 the two numbers are
  the same size and sit side by side.

`AutoPressureDisplay` is a `PressureDisplay` that reads its own numbers from the machine: it takes a
`pressure_order` — `"pressure:target"` (pressure primary, target secondary) or `"target:pressure"`
— and its `draw()` takes no arguments.

## Duration

`Duration` is whole seconds formatted in the panel's units of time. `seconds` is the count it draws,
and `draw()` formats and draws it like `Text`. `format` is `minutes` (default), `seconds` or
`hours`. `text(seconds)` formats without drawing: `2m30s`, `1m` on a whole minute, `45s` under a
minute, `1h2m5s`, `0s` for nothing and `--` for nil.

## ProgressBar

`ProgressBar` is a framed bar filled to a percentage. `value` is clamped to 0–100; the fill is the
left `(w − 2) · value / 100` pixels inside the one-pixel frame. 0 draws the frame and no fill.

## Bubble

`Bubble` is a square drawn to fill its box, filled or outlined. `shape` — `circle` (default),
`rect`, `roundrect`, `tri-up`, `tri-down`, `tri-left` or `tri-right`; `fill_state` — `filled`,
`unfilled` or `none` (`none` draws nothing). Smaller than 3 px it is a solid square.

## ProgressBubbles

`ProgressBubbles` is a field of `max_count` `Bubble` children laid out in a grid that fills its box.
The first `count` children in fill order are filled; `count` is clamped to 0–`max_count`.
`fill_direction` — `lr-tb` (default), `rl-tb`, `tb-lr` or `bt-lr` — names the order the cells are
assigned. `set_count(total)` rebuilds the field for a new total.

Each bubble is the largest whole square that lets the count fill the box: columns
`⌊√(max_count · w / h)⌋` clamped to 1–`max_count`, rows `⌈max_count / columns⌉`, size
`min(⌊w / columns⌋, ⌊h / rows⌋)`, at least 1 px, with the leftover width and height as the gaps
between bubbles.

## Scrollbar

`Scrollbar` is the thumb that shows where a window sits in a list. `index`, `count` and `visible`
are set before `draw()`: the thumb's height is `⌊h · visible / count⌋` clamped to 1–`h`, placed
across the span `count − visible` and clamped inside the box. `count ≤ 0` draws nothing; `hollow`
inverts the two ends of the track, otherwise the component draws a frame and a filled thumb.

## Triangle

`Triangle` is an arrow in one of the four directions, filled or empty, drawn to fill its box.
`direction` — `up` (default), `down`, `left` or `right`; `style` — `filled` (default) or `empty`.

## Icon

`Icon` is a component that draws a glyph in its box. The caller passes the box at `init`; a glyph
subclass draws its own ink inside it. The browser puts one at the left of a row and shifts the title
right by `dim.w`.