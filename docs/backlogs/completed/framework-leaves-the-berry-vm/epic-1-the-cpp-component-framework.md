# Epic 1 — the C++ component framework

The drawing primitives a page composes its content from are now ordinary C++ in
`libraries/Gfx/src/ui/`, in `namespace gardener::gfx::ui`. Nothing in them names `bvm` or Berry:
they hold typed fields and draw through a `Canvas` over a `DisplayDevice`.

## What landed

| Task | Class | What it is |
| --- | --- | --- |
| 1.1 | `tools/gfx/check.py` | the host check: builds the Gfx sources with a stub display and runs the cases |
| 1.2 | `Component` | the box, the `use` room hook and the inverted flip |
| 1.3 | `Text` | a string at a face and scale, its ink and the draw |
| 1.4 | `Icon` | the 12 by 8 glyph box |
| 1.5 | `ProgressBar` | the value held to 0–100 and the framed fill |
| 1.6 | `Scrollbar` | the thumb from the index, the count and the visible rows |
| 1.7 | `Duration` | whole seconds as `4m30s`, `90s`, `1h2m3s`, `0s` or `--` |
| 1.8 | `Layout`, `RowLayout`, `ColumnLayout` | the declared cell resolved against the box, the stack, the alignment |

Three value types came with them: `Size` (the room a component answers from `use`), `Cell` (the
share of its parent a layout child declares) and `Alignment` (where a thing sits in the space it is
given). `Ink` and `Paper` are the two colours a component draws in, `255` and `0`, which a one-bit
frame and a gray one both take.

## What was verified

`python tools/gfx/check.py` builds and runs 31 cases, all passing: the box and the flip, the ink box
at both faces and both scales, the cell the room is clipped to, the bar at its ends and past them,
the thumb at the top and the bottom, every duration format, the layout's stacking in both
directions, the equal thirds, the rounded span clamped, a declared size above one, the alignment at
both levels, the drawing order and the nested box.

`python scripts/arduino.py --task build` compiles the same sources for the device, so the classes
are the ones the firmware ships rather than a host-only copy.

## What is not here yet

The bindings are a later epic, so the image still compiles its own `/system/components/*.be` and no
page uses these classes yet. `pressure_display.be` stays Berry for good: it reads the machine's
readings and prints them in the panel's unit, which is the machine's business rather than the
drawing's, so both `PressureDisplay` and `AutoPressureDisplay` stay in the image.
