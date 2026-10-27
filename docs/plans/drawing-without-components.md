# Drawing without components

## What this is

Every page draws through a small object model: a `Component` with a `pos` and a `dim`, `Text`, `Icon`,
`ProgressBar`, `Scrollbar`, `Duration`, and a `Layout` that resolves its children's cells and draws
them. The model exists twice — the Berry classes in `/system/components/`, which is what a page uses,
and the C++ classes in `libraries/Gfx/src/ui/`, which nothing uses yet. This plan states the
requirement for replacing both with functions: a page owns its boxes, asks one function to divide a
stack, and calls the drawing it wants.

## Requirements

### A page draws with functions, not objects

**Now.** A page constructs `Text("Gardener")`, an `Icon`, a `ProgressBar` or a `Layout` tree, gives
each one a box with `set_box` or `to_box`, and calls `draw()`, which recurses through the tree.

**Instead.** The `screen` module is the whole drawing vocabulary: the verbs it has — `draw_text`,
`draw_glyph`, `draw_bitmap`, `draw_rect`, `draw_fillrect`, `draw_invert` and the rest — plus three
that carry arithmetic worth a name:

- `screen.draw_bar(x, y, w, h, percent)` — a framed bar filled to a percentage.
- `screen.draw_scrollbar(x, y, w, h, index, count, visible)` — a thumb sized and placed from them.
- `screen.draw_duration(x, y, scale, seconds[, format[, font]])` — whole seconds in the panel's units.

The rule behind each of them is a free function in `libraries/Gfx/src/ui/Drawing.h` — `barFill`,
`scrollThumb` and `durationText` — which the `screen` module calls and a page never has to. The
module unpacks the arguments and draws through the panel; the library answers the numbers. The
directory keeps its name and loses its classes: what is left of `libraries/Gfx/src/ui/` is functions
and the value types they take.

**Check.** A page's file names no component class, and `python tools/berry/check.py --tests` passes
with the same behaviour on the glass.

### The distribution is a calculation

**Now.** `Layout` is a component that holds its children, resolves each child's declared cell against
its own box on every draw, places them where its alignment says, and draws them.

**Instead.** Two functions in `system/layout.be` answer the boxes and nothing else:

- `cells(x, y, w, h, spans, across)` answers a list of boxes, one per declared span, dividing the box
  along the axis `across` names. A span is a share of the box rounded to the nearest pixel, a pixel
  count when it is above 1, or nil for the whole of it, and no span takes more than what is left.
- `place(x, y, w, h, iw, ih, align, v_align)` answers the corner a thing of that size sits at in the
  box, by the alignment named.

**Number.** The two answers reproduce what `Layout` resolved: the equal thirds of a row fill it, a
rounded share never overflows its box, eight pixels declared of a measured row stays eight, and a
centred ink box is the offset the layout gave it.

**Check.** The layout suite states the two functions' answers instead of a placed tree.

### The object model leaves both sides

**Now.** `/system/components/` holds `component.be`, `text.be`, `icon.be`, `layout.be`,
`progress_bar.be`, `scrollbar.be`, `duration.be` and `pressure_display.be`; `libraries/Gfx/src/ui/`
holds the same model in C++ with a host check over it.

**Instead.** The seven files go — `pressure_display.be` stays, because it reads the machine's readings
and prints them in the panel's unit, which is not the drawing's business — and the C++ classes go
with them. `libraries/Gfx/src/ui/` keeps its name and holds `Drawing.h`, the free functions behind
the three verbs and the duration formatter, with the host check over them. `List` stops being a
component as well: it is a selected index and a loop that draws its rows.

**Number.** The image compiles seven fewer system files, and the host check reports the functions
rather than the classes.

**Check.** `python tools/berry/check.py --memory` no longer lists the component files, and the C++
check passes with the cases rewritten against the functions.

## Consequences

- A frame is a page's own arithmetic: the chrome files, the overlay band and the settings column each
  ask `cells()` for their boxes and draw into them, so a page that draws past its box now paints over
  its neighbour rather than being clipped.
- The specs that describe the component model — `docs/specs/ui/components.md` — become the verb list,
  and the box paragraph in `routine.md` and `scenes.md` follows it.
- The completed component-layout epics described the model that is going; their summaries stay as
  history, and the plan that asked for the native classes is deleted with this one.

## How it is known to hold

`python tools/berry/check.py --tests` passes — every suite, with the layout suite rewritten to the two
functions — and `--art` and `--scene home`/`--scene browser` draw the pages as they drew before.
`python tools/gfx/check.py` passes with the functions' cases, and `python scripts/arduino.py --task
build` compiles the tree for the device.
