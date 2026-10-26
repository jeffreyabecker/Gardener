# Relative layout

The backlog for [relative-layout.md](../../plans/relative-layout.md). Worked one item at a time, in
the order below, so the tree builds and the previous item's checks still hold after each step. The
file is deleted with the plan when the last item lands; each epic's summary lives on under
`docs/backlogs/completed/relative-layout/`.

## Epic 1 — the AutoLayout engine

- [x] 1.1 — `bounds(w, h)` is a value a layout accepts — a width and a height fraction, each a
  number in [0, 1] or nil for the page — and an `AutoLayout` is a component with a box, loaded
  before the routines, that resolves each child's `bounds` against its own box, stacking siblings
  along one axis (rows down, a row's children across), stamps the child's `pos` and `dim`, and
  draws the children in order. Verified: on the 128×52 page, `bounds(nil, nil)` resolves to
  (0, 0, 128, 52); a row `bounds(1, 0.25)` to (0, 0, 128, 13), `bounds(1, 0.50)` to
  (0, 13, 128, 26) and the last to (0, 39, 128, 13); and a child `bounds(0.5, 1)` inside the
  readings row to (0, 13, 64, 26) or (64, 13, 64, 26) — asserted in `layout.test.be`.

## Epic 2 — the chromes move to AutoLayout

- [x] 2.1 — `HomeChrome` is an `AutoLayout` of two `bounds(0.5, 1)` children, the pressure display
  and the timer, and its constructor drops `pos, dim`. Verified: `home.test.be` and the `chromes`
  suite pass, and the halves resolve to (0, 0, 64, 52) and (64, 0, 64, 52), unchanged.
- [x] 2.2 — `RampedStaticChrome` is the nested tree — glyph column `bounds(0.125, 1)`, title
  `bounds(0.875, 1)`, the two readings `bounds(0.5, 1)` in the middle row, and status and bar each
  `bounds(0.5, 1)` in the last row — rooted at `bounds(nil, nil)`. Verified: the pinned boxes —
  glyph column (0, 0, 16, 13), title (16, 0, 112, 13), readings at y 13, status and bar each half
  of the last row — are asserted, and the static chrome frame draws.
- [ ] 2.3 — `RampedIntervalChrome` is the same nested tree, taking `icon` and `title_text` only.
  Verified: the pinned boxes are asserted, and the interval chrome frame draws.

## Epic 3 — components stop taking their box at construction

- [ ] 3.1 — `Component.init()` and every subclass drop `pos` and `dim`; a component's box is set
  after construction, before the first draw — by the layout for a child of a layout, and by the
  caller for a list row's icon (which keeps its 12×8 measure), the browser's scrollbar and a
  settings editor's inline text or bar. Verified: `python tools/berry/check.py --tests` passes with
  every component constructor stripped of `pos, dim`.

## Epic 4 — scenes always take the whole page

- [ ] 4.1 — `Scene.init()` takes no box and always sets `Position.top_left()` and `Size.page()`;
  every scene's `super(self).init(...)` drops its arguments, and `EditScene` and `SettingsEditScene`
  stop reading `ux.box()`. Verified: `python tools/berry/check.py --tests` passes with an editor
  framed centred in the page rather than in a sub-box.
- [ ] 4.2 — `ux.box()` is removed from `ux.be` and nothing in the image names it. Verified:
  `python tools/berry/check.py --tests` passes.
