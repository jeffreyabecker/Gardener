# Epic 4 — the rebuilt primitives

The v2 primitives in `system/components/v2/`, exercised by `tools/berry/tests/v2.test.be`.

- 4.1 — `BubbleV2`: a square of a shape, filled or outlined, a solid square below 3 px.
- 4.2 — `ProgressBubblesV2`: a wrapping layout of bubbles, one per set, sized to fill the box and
  filled for the first `count`; wrapping now counts spacing when it decides a line break.
- 4.3 — `ProgressBarV2`: a one-pixel frame with the inside filled to a value clamped to 0–100.
- 4.4 — `ScrollbarV2`: a thumb sized and placed from the index, count and visible rows.
- 4.5 — `TriangleV2`: an arrow in the four directions, filled or empty.
- 4.6 — `PressureDisplayV2`: the reading formatted in the panel's unit, filling the box.
- 4.7 — the secondary reading: smaller in a corner capped at `size_hint`, or side by side at
  `size_hint` 1; `TextV2` gained `fit_height` and `draw_face`.
- 4.8 — `DurationV2`: whole seconds with a `format` of seconds, minutes or hours.

Verified: `python tools/berry/check.py --test v2` passes eighteen cases, and the shipped pages
still render unchanged.
