# Epic 1 — the AutoLayout engine

`AutoLayout` in `firmware/gardener/data/system/components/layout.be`, exercised by the relative
layout cases in `tools/berry/tests/layout.test.be`.

- 1.1 — `bounds(w, h)` carries a width and a height fraction, each a number in [0, 1] or nil for
  the page. `AutoLayout` is a `Component` with a box: it resolves each child's bounds against that
  box, stacks the siblings along the axis they divide — down a column when each child is the full
  width, across a row when each is the full height — and stamps the resolved box on each child. A
  component carrying no bounds is a leaf and takes its layout's box whole, and `bounds(nil, nil)`
  names the page, so a root layout is bound to `Size.page()` at construction. A resolved span is
  clamped to what is left of its parent, so no child falls outside it. The flat `Layout` stayed
  while the chromes still extended it.

Verified: `python tools/berry/check.py --test layout` passes ten cases, including the page root at
(0, 0, 128, 52), the 13 / 26 / 13 rows, the half-width readings row, the 16-and-112 glyph row, a
leaf filling its layout, and the child walk. `python tools/berry/check.py --tests` passes all 90
cases, so the image still loads and every page still runs.
