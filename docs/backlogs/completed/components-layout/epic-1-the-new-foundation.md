# Epic 1 — the new foundation

The v2 component framework's base, built beside the v1 components the shipped pages still use. The
v2 classes carry a `V2` suffix so they do not shadow the v1 names the pages construct at runtime;
the suffix is dropped in the porting pass.

- 1.1 — `screen.width()` and `screen.height()` answer the panel's size in `Screen.cpp`, and
  `tools/berry/tests/screen.test.be` asserts they read 128 and 64 on the bitmap panel.
- 1.2 — `system/components/v2/component.be` defines `ComponentV2`: a box (`x, y, w, h`) and the
  `inverted` flip, with no insets.
- 1.3 — `system/components/v2/icon.be` defines `IconV2`: a component with an intrinsic `w` by `h`,
  the box the layout gives it, and the inherited flip.

Verified: `python tools/berry/check.py --tests` passes — 30 cases — and the shipped pages still
render unchanged with the v2 files loaded.
