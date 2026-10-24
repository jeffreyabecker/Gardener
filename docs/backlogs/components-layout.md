# Backlog — component layouts

The tasklist for [the component layouts plan](../plans/components-layout.md): a component is a box
and a flip, text fits the compiled faces first, a layout component sizes and arranges its children,
and the set bubbles, pressure display and duration are rebuilt on it. Each item is numbered
`epic.item`, so every one has a stable name for its branch and its commit. An item names the code
that is wrong now, what is true when it lands, and the check that shows it.

The work is parallel: the new framework is built beside the existing one, and the shipped pages
stay on the existing components throughout. The new components live under
`/system/components/v2/`, one file named in `/system/order` per component. The v2 classes carry a
`V2` suffix — `ComponentV2`, `TextV2` and so on — so they do not shadow the v1 classes the shipped
pages construct by name; the suffix is dropped in the porting pass. The epics are worked in order
and every item leaves the tree building. The spec line a behaviour changes is written in the commit
that changes it.

Porting the pages to the v2 components and adopting them is out of scope for now and is a
parking-lot line.

## Epic 1 — The new foundation

- [x] 1.1 Add `width()` and `height()` to the `screen` module. Today
  `Screen.cpp` in `libraries/PanelLink/src/routines/bindings/` binds only the draw calls,
  `text_bounds`, `flush` and `unit`, and `Display::width()` and `Display::height()` exist but are
  not exposed, so a layout has no panel size to derive from. Bind the two, answering the display's
  width and height. Check: a harness assertion reads both and gets 128 and 64 on the bitmap panel.

- [x] 1.2 Add the new component base at `/system/components/v2/`. Today
  `system/components/component.be` carries `margin`, `border` and `padding` and the `frame()` and
  `content()` rects; the spec's component is `x, y, w, h` plus `inverted`, drawing inside its box.
  Add the new base, named in `/system/order`, and leave the existing one in place, so the shipped
  pages keep drawing. Check: both load, the pages render as before, and `python
  tools/berry/check.py` passes.

- [x] 1.3 Add the new `Icon` at `/system/components/v2/` as a component with an intrinsic `w` by
  `h`, a box, and the `inverted` flip. Today `system/components/icon.be` is boxless and drawn with
  `draw(x, y)`, and the browser still needs it. Check: an icon instance draws in its box and flips
  when inverted; the browser's rows are unchanged.

## Epic 2 — Text

- [x] 2.1 Add the new `Text` at `/system/components/v2/` with the compiled-first ladder. Today
  `system/components/text.be` interleaves the built-in 5×7 at sizes 3 and 2 among the compiled
  faces and returns the smallest candidate when nothing fits. The new `Text` tries the compiled
  face at 24, 18, 12 and 9 px, bold when the style is bold, and the built-in 5×7 only last, fitting
  once per string and caching the face. Check: a string that fits at 9 px draws at 9 px, and one
  that does not draws at the built-in face.

- [x] 2.2 Truncate a string wider than its box at the built-in face with a trailing ellipsis. Today
  `draw_fitted` centres the smallest candidate, so the ink spills out both sides of the box. Check:
  a long string draws with an ellipsis at the box's edge and nothing past it.

- [x] 2.3 Render bold on the built-in face by inverting the text's ink box. Today bold exists only
  as the compiled bold faces, so bold on the built-in face is impossible. Check: bold built-in text
  draws and its ink box is then inverted.

## Epic 3 — The layout component

- [x] 3.1 Add the layout component at `/system/components/v2/`: a box holding children in order, a
  `fill_direction` (`lr-tb` first) and a `spacing`, placing measured children along the direction.
  Today no layout exists and every page writes each coordinate itself. Check: two measured children
  draw side by side with the spacing between them.

- [x] 3.2 Add growing children and the cross axis. Measured children keep their measured size;
  growing children share the space left after them equally; every child is stretched to the
  layout's cross size and aligns by its own `align` and `v_align`. Check: two growing children make
  equal columns; a measured word followed by a growing bar gives the bar the rest of the line.

- [x] 3.3 Add `wrap`, carrying a child that no longer fits to the next line or column, each
  separated by the spacing. Check: children beyond the line's width wrap and stack, each line as
  tall as its tallest child.

- [x] 3.4 Add the remaining fill directions `rl-tb`, `tb-lr` and `bt-lr`. Check: children fill from
  each named corner in order.

- [x] 3.5 Give a layout a natural size so a parent measures it: the sum of its children's sizes
  along the fill direction plus the spacing, and the largest child across it. Check: a layout
  inside a layout sizes to its children.

## Epic 4 — The rebuilt primitives

- [ ] 4.1 Add the `Bubble` primitive at `/system/components/v2/`: a square of a shape, filled or
  outlined, a solid square below 3 px. Check: each shape draws, and a bubble smaller than 3 px is a
  solid square with no outline.

- [ ] 4.2 Add the new `ProgressBubbles` at `/system/components/v2/` as a wrapping layout of
  `max_count` bubbles whose `fill_direction` is the field's, filling the first `count` children in
  fill order. Today `system/components/progress_bubbles.be` lays its own grid with `compute()` and
  maps `fill_direction` onto the grid. The bubble size stays the measure that lets the count fill
  the box. Check: every count from 1 to 20 fills from the matching corner in each of the four
  directions, and below-3-px bubbles are solid squares.

- [ ] 4.3 Add the new `ProgressBar` at `/system/components/v2/`, a one-pixel frame around the box
  with the inside filled to a value clamped to 0–100. Check: 0 draws the frame and no fill, 100
  fills the inside, and a value past the ends clamps.

- [ ] 4.4 Add the new `Scrollbar` at `/system/components/v2/` with the thumb height and position
  and the hollow and framed forms. Check: the thumb's height is `h·visible/count` clamped to 1–`h`,
  and its top moves from the first to the last index.

- [ ] 4.5 Add the new `Triangle` at `/system/components/v2/` in the four directions, filled or
  empty. Check: each direction draws in its box.

- [ ] 4.6 Add the new `PressureDisplay` at `/system/components/v2/` with `family` and `style` and
  no border, formatting the reading in the panel's unit. Check: a nil reading draws `--`, and a
  reading formats with the unit's decimals in each unit.

- [ ] 4.7 Add the secondary reading to `PressureDisplay` at `/system/components/v2/`:
  `secondary_pressure`, `secondary_position`, `size_hint`, and `secondary_family` and
  `secondary_style`. Check: a secondary sits smaller in each of the four corners, and side by side
  at `size_hint` 1 on the side the second character of `secondary_position` names.

- [ ] 4.8 Add the `Duration` component at `/system/components/v2/` with the `format` property.
  Today `duration_text` in `system/settings/settings.be` formats minutes and seconds only, and no
  component draws a duration. Check: `2m30s`, `1m`, `45s`, `1h2m30s` and `0s` in each of the three
  formats.

## Epic 5 — The baseline suite and the component captures

- [ ] 5.1 Add a capture path that renders a named component to a frame, from either framework, so a
  component can be seen without driving a whole scene. Check: a frame for a named component is
  written and opens as a BMP.

- [ ] 5.2 Add a baseline test suite that drives the existing `/system/components/` components
  directly, not through the pages: `Text`'s fit ladder, `ProgressBubbles`' grid at several counts,
  `ProgressBar`, `Scrollbar`, `Triangle`, `PressureDisplay` and `Icon`, asserting each one's current
  behaviour and capturing a frame of it. This is the record of what the old framework renders
  before the v2 components replace it. Check: `python tools/berry/check.py --tests` passes and a
  frame per component is written.

- [ ] 5.3 Add test suites under `tools/berry/tests` for the v2 components, asserting the fit ladder
  and truncation, measure and place, the four fill directions, the secondary pressure in each
  corner and side by side, the duration formats, and the bubble field at every count. Check:
  `python tools/berry/check.py --tests` passes.
