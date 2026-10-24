# Component layouts

The panel's pages still draw the old way: every component is a pixel box with `margin`, `border`
and `padding` insets, text picks the built-in face alongside the compiled ones, the set bubbles run
their own grid, `Icon` is a boxless exception, and each page writes its own coordinates. The spec
[components.md](../specs/ui/components.md) describes the system to build instead: a component that
is only a box and a flip, a layout component that sizes and arranges its children, text that falls
back to the built-in face only as a last resort, a pressure display with a secondary reading, a
duration display, and bubbles rebuilt on a wrapping layout.

The work is done in parallel: the new framework is built beside the existing one, and both keep
rendering until the new one has been fully vetted. Porting the pages onto the new framework is out
of scope for this plan. The plan carries the requirement to bring the component framework to that
spec and to prove it on the host with a test suite that captures what each component renders — for
the framework that exists today and the one that replaces it, so the two can be seen side by side.

## Requirements

### 1. The new framework is built beside the old one and vetted before any switch

Today the spec describes a replacement, and the requirements that follow read as rewriting the
components in place, which would break the pages mid-way and leave nothing to compare the new
rendering against. Instead the new component and layout framework is built alongside the existing
one, in its own folder, and the existing components and pages keep working unchanged throughout.
Porting the pages onto the new framework and deleting the old one is out of scope for this plan and
is a parking-lot line.

Check: while the work is under way `python tools/berry/check.py` still drives the existing pages
and passes, and the existing components stay untouched.

### 2. A component is a box and a flip, and Icon is one

Today [component.be](../../firmware/gardener/data/system/components/component.be) defines the
`margin`, `border` and `padding` insets and the `frame()` and `content()` rects on top of the box,
and [icon.be](../../firmware/gardener/data/system/components/icon.be) is a boxless exception drawn
with `draw(x, y)`. The spec says a component is `x, y, w, h` plus `inverted`, with nothing drawn
outside the box, and that `Icon` is a component with an intrinsic `w` by `h` whose box the layout
gives it and which flips when inverted.

Check: the new component base carries no insets and the new `Icon` is a boxed component; the
existing `component.be` and `icon.be` stay untouched throughout this plan; the Berry check still
drives every page.

### 3. A layout component sizes and arranges its children

Today there is no layout: [ui.be](../../firmware/gardener/data/modules/ui.be),
[browser.be](../../firmware/gardener/data/modules/browser.be) and the two routine chromes write
every `x, y, w, h` as a literal, and [progress_bubbles.be](../../firmware/gardener/data/system/components/progress_bubbles.be)
computes its own grid. The spec says a layout is a component whose box holds children and which
derives each child's box in two passes: measure (a text's ink box at the face that fits the cross
size, an icon's intrinsic size, a nested layout's measured size) and place (measured children keep
their size, growing children share the remainder equally, every child is stretched to the cross
size and aligned by its own `align` and `v_align`). A layout has `fill_direction` — `lr-tb`,
`rl-tb`, `tb-lr`, `bt-lr` — `spacing`, and `wrap`, which carries a child to the next line or
column when it no longer fits; a layout's own natural size is what its parent measures it as.

Check: a page states relationships, not pixel numbers, and a change to the content reflows the page
without an edit; the four fill directions and `wrap` are each exercised by the suite in requirement 9.

### 4. Text fits the compiled faces first, then truncates

Today [text.be](../../firmware/gardener/data/system/components/text.be) tries the compiled faces
interleaved with the built-in 5×7 at sizes 3 and 2, returns the smallest candidate when nothing
fits, and centres the result, so a string wider than the box at the smallest face spills out of it;
bold is only the compiled bold faces, so bold on the built-in face is impossible. The spec says the
candidate ladder is the compiled face at 24, 18, 12 and 9 px, bold when the style is bold, and then
the built-in 5×7 as the last resort; a string still wider than its box at that face is cut short
with a trailing ellipsis, and a bold built-in face is drawn and then its ink box inverted.

Check: a string that fits at 9 px draws at 9 px, one that does not draws at the built-in 5×7, and
one wider than the box is truncated with an ellipsis at the box's edge; bold built-in text is
inverted, and nothing a text draws lies outside its box.

### 5. PressureDisplay carries a secondary reading

Today [pressure_display.be](../../firmware/gardener/data/system/components/pressure_display.be)
draws one number, and its `border` draws a frame the spec no longer has. The spec adds
`secondary_pressure` — a second kPa reading, nil by default — drawn per `size_hint` in the corner
`secondary_position` names, with `family` and `style` for the main number and `secondary_family`
and `secondary_style` for the second (nil means the main's). `size_hint` is a real from 0 to 1:
below 1 it is a fraction of the main number's face height and the secondary sits smaller in the
corner; at 1 the two numbers are the same size, side by side, the secondary on the side the second
character of `secondary_position` names.

Check: the readout still draws one number when `secondary_pressure` is nil; a set secondary appears
smaller in each of the four corners and side by side at `size_hint` 1.

### 6. Duration formats whole seconds

Today a duration is formatted by `duration_text` in
[settings.be](../../firmware/gardener/data/system/settings/settings.be), which only knows minutes
and seconds, and no component draws one. The spec adds `Duration`, which takes whole seconds, draws
the text filling its box with `Text`'s fit, and has a `format` property — `seconds`, `minutes`
(default) or `hours` — with a leading zero-valued unit omitted and no zero padding.

Check: `2m30s`, `1m` on a whole minute, `45s` under a minute, `1h2m30s`, and `0s` for nothing, in
each of the three formats.

### 7. ProgressBubbles is a wrapping layout of bubbles

Today [progress_bubbles.be](../../firmware/gardener/data/system/components/progress_bubbles.be)
lays its own grid with `compute()` and maps `fill_direction` onto the grid itself. The spec adds a
`Bubble` primitive — a square of a shape, filled or outlined, a solid square below 3 px — and
recasts `ProgressBubbles` as a wrapping layout of `max_count` bubbles whose `fill_direction` is the
layout's, filling the first `count` children in fill order; the bubble size stays as the measure
that lets the count fill the box.

Check: the same count in each of the four fill directions fills from the matching corner; the field
still fills its box at every count from 1 to 20, and a bubble below 3 px is a solid square.

### 8. The screen reports its size

Today the `screen` module has no size call, and every page lays out against a hard-coded 128×64.
The spec says the panel is `screen.width()` by `screen.height()`, so the new components and layouts
derive their boxes from the panel's own size, never from a literal.

Check: `screen.width()` and `screen.height()` answer the panel's size on the host and the device.
The pages keep their literals until they are ported, which is out of scope for this plan.

### 9. The Berry tooling tests and captures the rendering

Today [check.py](../../tools/berry/check.py) drives every page, `--art` and `--scene` save one
frame, and `--tests` runs the suites in `tools/berry/tests` with a GIF per case, but there is no
test of fit, measure, wrap or fill order, no frame of a lone component, and no captured record of
what the old framework rendered to compare the new one against.

Instead the tooling gains a suite that asserts each requirement above, and a capture that renders a
named component to a frame. The frames are kept for both frameworks at the component level: the
existing components are driven directly by a baseline suite and their frames committed, then the
new components' frames are captured for the same inputs, so a v2 component can be seen next to the
v1 component it replaces. The new framework's frames for the changed content (a set count of 20,
every fill direction, side-by-side pressures, the hours-minutes-seconds formats) must show nothing
overlapping and nothing clipped at the panel's edge.

Check: `python tools/berry/check.py --tests` passes with no failing assertion; the committed
baseline frames and the new frames for the same inputs are written where they can be opened and
looked at.

## Consequences

The `screen` module gains `width()` and `height()`, the one native addition the spec calls for; the
binding is already documented in [bindings.md](../specs/ui/bindings.md). The description of
components in [routine.md](../specs/ui/routine.md) still calls `Icon` the boxless exception and is
rewritten to point at the components spec. No stored setting, wire reply or gesture changes.

## Deferred

Porting the existing pages — the readout, the browser, the settings list and the two routine
chromes — onto the new components, and deleting the old framework, is out of scope for this plan
and is a parking-lot entry.

Making the overlay's own band adapt to the panel — today it is a fixed 12 rows — is not part of
this plan; only the page area above it reflows. That is a parking-lot entry.

## How it is known to hold

`python tools/berry/check.py --tests` runs the suite, which asserts the fit ladder and truncation,
measure and place, the four fill directions, the secondary pressure in each corner and side by
side, the duration formats, and the bubble field at every count. The capture pass writes the
committed baseline of the existing components and the new framework's frames; opening them side by
side is the observation that each new component reproduces the old one's rendering and reflows
where the spec says it should. The pages are not ported in this plan.
