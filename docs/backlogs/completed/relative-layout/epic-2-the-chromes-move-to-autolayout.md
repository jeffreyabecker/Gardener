# Epic 2 — the chromes move to AutoLayout

The three chrome classes in `firmware/gardener/data/system/home.be`,
`firmware/gardener/data/routines/ramped_static.be` and
`firmware/gardener/data/routines/ramped_interval.be`, exercised by `tools/berry/tests/chromes.test.be`.

- 2.1 — `HomeChrome` became an `AutoLayout` rooted at `bounds(nil, nil)` with two `bounds(0.5, 1)`
  children, the pressure display and the timer, and its constructor dropped `pos, dim`. Its halves
  resolve to (0, 0, 64, 52) and (64, 0, 64, 52), where the hand arithmetic put them.
- 2.2 — `RampedStaticChrome` became the quarter / half / quarter tree: a glyph-and-title row split
  `0.125` and `0.875`, a readings row split in two, and a status-and-bar row split in two. Its
  timer, status and bar are fields of the chrome rather than children reached by index, and the
  glyph column takes (0, 0, 16, 13), the title (16, 0, 112, 13).
- 2.3 — `RampedIntervalChrome` became the same tree, taking only the glyph and the title, and the
  flat `Layout` it and the static chrome had extended was deleted along with its two cases in
  `layout.test.be`.

Verified: `python tools/berry/check.py --tests` passes 88 cases. The `chromes` suite draws all
three chromes and pins the boxes above, so the two redefined rows — the glyph-and-title row and the
bar, which now take their whole cells instead of a fixed inset — are the geometry the plan
redefines. `home.test.be` passes, and every page still runs at `python tools/berry/check.py`.
