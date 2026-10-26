# Relative layout

## What this is

A layout today is a flat list with no box of its own: [layout.be](../../firmware/gardener/data/system/components/layout.be)
holds the children and draws them in order, and every caller positions each child itself, once, at
build time. The chrome classes do that arithmetic by hand — `var half = dim.w / 2`, `pos.x + 16`,
`pos.y + 13`, `Size(half - 8, 8)` in `RampedStaticChrome` and `RampedIntervalChrome`, and the same
split in `HomeChrome`. [component.be](../../firmware/gardener/data/system/components/component.be)
fixes the box at construction and states there is no reflow.

This plan moves the arithmetic into the layout: a layout is itself a component with a box, and
each child declares the portion of its parent's box it occupies — vertically and horizontally, as
fractions — with the children stacked inside their parent, and a root with no parent takes the
whole page area when declared `bounds(nil, nil)`. The layout resolves those fractions to the
child's box, and components stop taking `pos` and `dim` in their constructors — the layout is
what sets both, before the first draw. The pages keep their structure — a glyph-and-title row, a
readings row, a status-and-bar row — but the geometry is redefined as clean proportions: where
today's box was
pixel-intrinsic (the 12×8 glyph slot, the 9-row title, the 8-row bar and its insets), the child
now takes a whole fraction of its parent instead.

What it leaves alone: `List` and `SettingsEditScene` place rows by their own pitch, not by
fractions of a parent, and keep doing so. Live reflow — re-resolving a page when its box changes
after construction — is deferred.

## The requirement

**A layout resolves each child's box from the fraction of its parent the child declares.**

What is wrong now: a child's box is a pair of pixel values the caller computes by hand from its own
`pos` and `dim`. `Layout` has no box and does no placement, and `RampedStaticChrome`,
`RampedIntervalChrome` and `HomeChrome` each repeat the half-width split and the fixed insets.

What is true instead: a layout carries a box, and a child is a component or a nested layout, each
declared with `bounds(w, h)` — the fractions of its parent's width and height it occupies.
`bounds(nil, nil)` is the special case that names the page instead of a parent: the component takes
the whole page area, the box `Size.page()` at the top left. Siblings inside a layout are stacked:
the outer layout stacks its rows down, and a row stacks its children side by side. The layout
resolves each `bounds` to the child's `pos` and `dim`; the caller writes no pixel arithmetic. A
declared `bounds` never places any part of a child outside its parent, a row's children cover its
width, and a layout's rows cover its height.

The shape of it, for `RampedIntervalChrome`. The constructor stops computing `var half = dim.w / 2`
and the fixed insets and declares a tree of nested layouts instead; each `bounds(w, h)` is the
fraction of its parent the child takes, and siblings stack. The syntax below is illustrative —
which class name and argument order the implementer picks is their call:

```
class RampedIntervalChrome : AutoLayout
  def init(icon, title_text)
    super(self).init(bounds(nil, nil), [      # the whole page area
      AutoLayout(bounds(1, 0.25), [          # the glyph-and-title row, 13 rows
        AutoLayout(bounds(0.125, 1), [self.icon]),   # 16 columns, the glyph inside it
        AutoLayout(bounds(0.875, 1), [self.title])   # the rest of the row
      ]),
      AutoLayout(bounds(1, 0.50), [          # the readings row, 26 rows
        AutoLayout(bounds(0.5, 1),
                   [AutoPressureDisplay("pressure:target", "br", 0.25)]),
        AutoLayout(bounds(0.5, 1), [self.sets])
      ]),
      AutoLayout(bounds(1, 0.25), [          # the status-and-bar row, 13 rows
        AutoLayout(bounds(0.5, 1), [self.status_text]),
        AutoLayout(bounds(0.5, 1), [self.progress_bar])
      ])
    ])
  end
end
```

The glyph row is `0.125 + 0.875`, not the `0.22 + 0.88` it was first sketched as: a row's children
have to sum to 1, and 1/8 is the 16 columns the title's left edge already sits at. The readings row
keeps its boxes — the old arithmetic already made it a clean half — and the bar stops being an
inset 56×8 and becomes a full half-row, which is the geometry this plan redefines.

Numbers: a fraction lies in [0, 1] inclusive, of the parent's width on x and height on y, and a
nil in place of a fraction means the page: `bounds(nil, nil)` resolves to (0, 0, 128, 52). A row's
children sum to 1 across, and a layout's rows sum to 1 down. On the 128×52 page the rows are
0.25 / 0.50 / 0.25 of the height, and the readings and status-and-bar rows are 0.5 / 0.5 of the
width.

Checked by: on the 128×52 page, `bounds(nil, nil)` resolves to (0, 0, 128, 52) — the page area — a
row declared `bounds(1, 0.25)` resolves to (0, 0, 128, 13), the
readings row `bounds(1, 0.50)` to (0, 13, 128, 26), and the last row to (0, 39, 128, 13); a child
declared `bounds(0.5, 1)` inside the readings row resolves to (0, 13, 64, 26) or (64, 13, 64, 26).
The chromes, rewritten as nested layouts, resolve to the boxes their bounds name: in
`RampedStaticChrome` and `RampedIntervalChrome` the glyph column at (0, 0, 16, 13), the title at
(16, 0, 112, 13), the two readings at y 13, and the status row and the bar each half of the last
row — asserted in a test.

## Components stop taking their box at construction

**A component's constructor takes no `pos` and no `dim`; the layout sets both before the first
draw.**

What is wrong now: `Component.init(pos, dim)` accepts the box and stores it, and every subclass —
`Text`, `Icon`, `ProgressBar`, `Scrollbar` and the rest — forwards it through
`super(self).init(pos, dim)`. The box is a thing a component is handed at construction, and
[component.be](../../firmware/gardener/data/system/components/component.be) says so: "a box is
given at construction and kept; there is no reflow."

What is true instead: a component's constructor takes only the component's own arguments. The
layout that resolves the component's `bounds` is the sole writer of its `pos` and `dim`, and it
sets both — a `Position` and a `Size`, never one without the other — after construction and before
the first `draw()` call. A component is never drawn before its box is set; the layout setting the
box is the guarantee. Subclasses drop the `pos, dim` parameters and stop forwarding them, and the
chrome constructors drop them too, as the example above shows.

Checked by: a test builds a component with no box, the layout resolves its `bounds` to a box, and
the component draws inside that box; the existing suites — `chromes`, `layout`, `components` —
still pass with every component constructor stripped of its `pos, dim` parameters.

## Consequences

The box contract is stated in the header comments of
[component.be](../../firmware/gardener/data/system/components/component.be) — "a box is given at
construction and kept, there is no reflow" — and
[layout.be](../../firmware/gardener/data/system/components/layout.be) — "a flat list, nothing more"
— and those now say something different: a layout is a component with a box of its own, it resolves
its children, and there is a new `bounds` vocabulary; a component's constructor no longer takes a
box, and the layout is what sets `pos` and `dim` before the first draw. There is no layout spec, so
no spec changes; the contract continues to live in those comments and in
[layout.test.be](../../tools/berry/tests/layout.test.be), which pins the new resolution.

The panel's pages change where today's boxes were pixel-intrinsic: the glyph-and-title row becomes
a 16-column glyph slot and a full-height title, and the bar becomes a full half-row. The readings
row and the home page's `0.5 / 0.5` split were already clean fractions, so those draw as before.
The `chromes` suite's frames are the record of the change.

## Deferred

- Live reflow — a layout re-resolving its children when its box changes after construction. This
  plan resolves once, from the box at build time; nothing today changes a scene's box mid-run. See
  the parking lot.
- The overlay band staying a fixed 12 rows while the page area above it reflows is already in the
  parking lot and is untouched.

## How it is known to hold

Run `python tools/berry/check.py --tests`: every file compiles, every page runs, and the new cases
in [layout.test.be](../../tools/berry/tests/layout.test.be) assert the resolved boxes above, that
each migrated chrome resolves to the boxes its bounds name, and that a component built with no box
draws inside the box the layout set. Run
`python tools/berry/check.py --art static` and `--scene home`; the frames show the redefined
glyph-and-title and bar rows, and the unchanged readings row and home page.
