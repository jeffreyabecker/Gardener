# Epic 3 — components stop taking their box at construction

`Component` in `firmware/gardener/data/system/components/component.be` and every subclass, with
the call sites that build one.

- 3.1 — `Component.init()` takes no `pos` and no `dim`, and `set_box(pos, dim)` is the one way a
  component is given both halves of its box: the layout calls it for every child it places, and a
  caller calls it for a component that stands alone — the browser's scrollbar, a settings editor's
  inline value or bar, a list row's glyph, the driver's component art scenes and the component
  cases. `Text`, `Duration`, `ProgressBar`, `Scrollbar`, `Triangle`, `PressureDisplay` and
  `AutoPressureDisplay` take only their own arguments; `PressureDisplay` gives its internal texts a
  box on every draw instead of mutating theirs; `Icon` carries the 12 by 8 measure every glyph is
  drawn on until a layout gives it a box; and the glyph subclasses lose the `init` that only
  forwarded the box.

Verified: `python tools/berry/check.py --tests` passes 88 cases, and
`python tools/berry/check.py` loads the image, drives every page and passes. No component
constructor in `firmware/` or `tools/` still names a `pos` or a `dim`; the two `init(pos, dim)` left
are `Scene`, which the next epic takes, and the test scene in `settings.test.be` that subclasses it.
