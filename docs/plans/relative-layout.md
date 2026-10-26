# Relative layout

## What this is

A layout today is a flat list with no box of its own: [layout.be](../../firmware/gardener/data/system/components/layout.be)
holds the children and draws them in order, and every caller positions each child itself, once, at
build time. The chrome classes do that arithmetic by hand — `var half = dim.w / 2`, `pos.x + 16`,
`pos.y + 13`, `Size(half - 8, 8)` in `RampedStaticChrome` and `RampedIntervalChrome`, and the same
split in `HomeChrome`. [component.be](../../firmware/gardener/data/system/components/component.be)
fixes the box at construction and states there is no reflow.

This plan moves the arithmetic into the layout: a child is added with an extent — the portion of
its parent's box it occupies, on each axis, vertically and horizontally — and the layout resolves
that to the child's box. The pages keep drawing the same boxes; only where the numbers come from
changes.

What it leaves alone: `List` and `SettingsEditScene` place rows by their own pitch, not by
fractions of a parent, and keep doing so. Live reflow — re-resolving a page when its box changes
after construction — is deferred.

## The requirement

**A layout resolves a child's box from the extent the child declares.**

What is wrong now: a child's box is a pair of pixel values the caller computes by hand from its own
`pos` and `dim`. `Layout` has no box and does no placement, and `RampedStaticChrome`,
`RampedIntervalChrome` and `HomeChrome` each repeat the half-width split and the fixed insets.

What is true instead: a layout carries the parent box, and a child is added with a per-axis extent
— where it starts and how far it spans on that axis — given as a fraction of the parent, with a
fixed pixel amount allowed where the child's size is intrinsic (a 12×8 glyph, a 9-row title, an
8-row bar) or where the page uses a fixed inset (the 16-column glyph gutter, the 8-column bar
inset). The layout turns that extent into the child's `pos` and `dim`; the caller no longer writes
`pos.x + 16` or `dim.w / 2`. A child resolves to the same box whether it was declared by extent or
given the pixels directly, and a declared extent never places any part of a child outside the
parent.

The shape of it, for `RampedIntervalChrome`. The arithmetic it repeats today is `var half = dim.w /
2` and a set of fixed insets; after the change the constructor stops doing that math and declares
what each child occupies. The syntax below is illustrative — which axis words and record the layout
takes is the implementer's call — but the boxes it resolves to are the ones the constructor
produces now:

```
# a bare number is a fraction of the parent on that axis; `px` is a fixed row
# or column; `fill` takes what the fraction leaves, for the title's right edge
def init(icon, title_text, pos, dim)
  self.icon = icon
  self.title = Text(nil, nil, title_text)
  self.sets = Text(nil, nil, "")
  super(self).init([
    add(self.icon,  x: [0, 12px],  y: [0, 8px]),     # the 12x8 glyph, top left
    add(self.title, x: [16px, fill], y: [2px, 9px]), # 16-col gutter, the rest of the row
    add(AutoPressureDisplay(nil, nil, "pressure:target", "br", 0.25),
                    x: [0, 0.5],  y: [13px, 26px]),  # left half
    add(self.sets,  x: [0.5, 0.5], y: [13px, 26px]), # right half
    add(Text(nil, nil, "", text_settings("sans", "none", "left", "center")),
                    x: [0, 0.5],  y: [39px, 13px]),  # status row, left half
    add(ProgressBar(nil, nil, nil),
                    x: [0.5 + 8px, 0.5 - 8px], y: [41px, 8px])   # bar, inset from the half
  ])
end
```

`0.5 + 8px` and `0.5 - 8px` on the bar are the only place the two kinds meet: the half split plus
the 8-column inset the bar keeps away from the status row. Everything else is one kind or the
other — the halves and the title's fill are fractions, and the glyph, the title row, the two
display rows, the status row and the bar's own height are fixed rows and columns.

Numbers: a fraction lies in [0, 1] inclusive, of the parent's width on x and height on y. A fixed
pixel amount is a whole number of rows or columns, no more than the parent's extent on that axis.

Checked by: on the 128×52 page, a child declared to span the left half fully — x start 0, span 0.5;
y start 0, span 1 — resolves to (0, 0, 64, 52), and its right-half twin to (64, 0, 64, 52); a fixed
12×8 glyph at the top left resolves to (0, 0, 12, 8). The three chromes, rewritten to declare
extents, resolve to exactly the pixel boxes their arithmetic produces today — in
`RampedStaticChrome`, the glyph at (0, 0, 12, 8), the title at (16, 2, 112, 9), the two half-width
displays at y 13, the status row at y 39, and the bar at (72, 41, 56, 8) — asserted in a test.

## Consequences

The box contract is stated in the header comments of
[component.be](../../firmware/gardener/data/system/components/component.be) — "a box is given at
construction and kept, there is no reflow" — and
[layout.be](../../firmware/gardener/data/system/components/layout.be) — "a flat list, nothing more"
— and those now say something different: a layout owns the parent box and resolves its children.
There is no layout spec, so no spec changes; the contract continues to live in those comments and
in [layout.test.be](../../tools/berry/tests/layout.test.be), which pins the new resolution.

The page the panel draws does not change. The `chromes` suite's frames are the record of that.

## Deferred

- Live reflow — a layout re-resolving its children when its box changes after construction. This
  plan resolves once, from the box at build time; nothing today changes a scene's box mid-run. See
  the parking lot.
- The overlay band staying a fixed 12 rows while the page area above it reflows is already in the
  parking lot and is untouched.

## How it is known to hold

Run `python tools/berry/check.py --tests`: every file compiles, every page runs, and the new cases
in [layout.test.be](../../tools/berry/tests/layout.test.be) assert the resolved boxes above and
that each migrated chrome resolves to the pixel boxes it hard-codes before the change. Run
`python tools/berry/check.py --art static` and `--scene home`; the frames the person looking at
them sees are the pages they already know, because the declared extents resolve to the same boxes.
