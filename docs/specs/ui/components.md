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
component that stands alone, such as a list row's glyph or a settings editor's inline value.
Nothing a component draws lies outside its box, and a component never draws before it has one.

A page states the numbers it means rather than a partial box: the page is the panel less the band,
`bounds(screen.page_width(), screen.page_height() - overlay_band_height)`, and a whole-panel box is
`bounds(screen.width(), screen.height())`.

`inverted`, when true, flips the whole box as the last thing a draw does: everything the component
drew inside the box reads as paper on ink — a selected row, a cursor or a focused readout. Every
draw ends in `super(self).draw()`, so the flip is always the last thing. A component that returns
before drawing leaves its box alone, so nothing is highlighted.

## Layout

`AutoLayout` is a component with a box that resolves the box of each child from the fraction of
itself that child declares, and draws the children in order. It is the one way a page states where
its parts go; a caller no longer works out `pos.x + 16` or `dim.w / 2`.

A child declares its box as `bounds(w, h)` — the fraction of the parent's width and the fraction of
its height the child occupies. Each fraction is a number in [0, 1], or nil for the whole of the
page. It is the same record a component's `dim` is, and the one `screen.text_bounds` answers.

- `bounds(nil, nil)` names the page rather than a parent, so the layout holding it is a root and is
  bound to the page — the panel less the band — at construction; every other layout waits for the
  parent that places it.
- Siblings stack along the axis they divide: a layout whose children are each the full width stacks
  them down a column, and one whose children are each the full height stacks them across a row.
- A resolved span is clamped to what is left of the parent, so nothing a `bounds` declares can place
  a child outside it.
- A component that declares no bounds is a leaf and takes the layout's box whole, so a layout that
  is not a row holds one such component.
- The layout is itself a component, so its own draw ends in `super(self).draw()` like any other.

`children` is the list handed to `init`, and `add(child)` appends to it. A layout resolves its
children once, from the box it is given; a box that changes afterwards is not re-resolved.

## Text

`Text` draws a string in its box in the largest face that fits. It takes its `value` at `init`; its
box is set with `set_box()` like any other component's.

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
face the caller already chose. `bounds(text, candidate)` answers the ink box, `fit(text, max_w,
max_h)` the largest face whose ink fits a box — the choice `draw()` makes for its own box, and the
one a caller makes when it has to draw several strings at one size, such as a switch's segments —
`fit_height(text, max_h)` the largest face whose ink height fits a cap, and `candidate` is the face
the last draw fitted.

## PressureDisplay

`PressureDisplay` is the reading formatted in the panel's unit, drawing the number filling its box,
using `Text`'s fit.

- `pressure_kpa` — the main reading in kPa, set before `draw()`; `draw()` takes no arguments.
- A nil reading draws `--`.
- The number is the kPa value scaled by the unit's `per_kpa`, with the unit's decimals.
- The number carries no suffix of its own; the unit is the band's, in the bottom band.
- `family`, `style` — the main number's face, as in `Text`.
- `secondary_pressure` — a second pressure in kPa, or nil (default), drawn per `secondary_position`
  and `size_hint`.
- `secondary_position` — `tl`, `tr`, `bl` or `br` (default `br`).
- `size_hint` — a real from 0 to 1 (default 0.5): below 1 a hint for the secondary's size as a
  fraction of the main number's face height, drawn smaller in the corner; at 1 the two numbers are
  the same size and sit side by side.

`AutoPressureDisplay` is a `PressureDisplay` that reads its own numbers from the machine: it takes a
`pressure_order` — `"pressure:target"` (pressure primary, target secondary) or `"target:pressure"`
— and its `draw()` sets `pressure_kpa` and `secondary_pressure` from the machine before drawing.

## Duration

`Duration` is whole seconds formatted in the panel's units of time. `seconds` is the count it draws,
and `draw()` formats and draws it like `Text`. `format` is `minutes` (default), `seconds` or
`hours`. `text(seconds)` formats without drawing: `2m30s`, `1m` on a whole minute, `45s` under a
minute, `1h2m5s`, `0s` for nothing and `--` for nil.

## ProgressBar

`ProgressBar` is a framed bar filled to a percentage. `value` is clamped to 0–100; the fill is the
left `(w − 2) · value / 100` pixels inside the one-pixel frame. 0 draws the frame and no fill.

## Scrollbar

`Scrollbar` is the thumb that shows where a window sits in a list. `index`, `count` and `visible`
are set before `draw()`: the thumb's height is `⌊h · visible / count⌋` clamped to 1–`h`, placed
across the span `count − visible` and clamped inside the box. `count ≤ 0` draws nothing; `hollow`
inverts the two ends of the track, otherwise the component draws a frame and a filled thumb.

## Triangle

`Triangle` is an arrow in one of the four directions, filled or empty, drawn to fill its box.
`direction` — `up` (default), `down`, `left` or `right`; `style` — `filled` (default) or `empty`.

## Icon

`Icon` is a component that draws a glyph in its box. A glyph subclass draws its own ink inside it.
An icon carries the 12 by 8 measure every glyph is drawn on until a layout gives it a box of its
own. The browser puts one at the left of a row and shifts the title right by `dim.w`.