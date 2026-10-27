# Implement the standalone Gfx drawing library

Implement [docs/specs/graphics.md](C:/ode/eggplan-vacuum/docs/specs/graphics.md) as a real
standalone Arduino library under `libraries/Gfx`, independent of the main application library. The
spec is the contract; this plan breaks it into ordered, checkable items.

## Scope

In scope: the library skeleton, `Rect`, `DisplayDevice`, `Canvas` with `setPixel`, `invert` and the
`draw` template, the shape vocabulary the library ships — the geometry shapes, `Bitmap` and `Text` —
plus a demo example sketch that compiles the library and a spec-banner removal.

The first five items below are in the tree and compile for the board; the shapes, the example and
the spec banner are not.

Out of scope this pass: a concrete `DisplayDevice` driver and wiring the main application library
to depend on `Gfx`. Those follow in later work.

## Decisions

- Library folder `libraries/Gfx`, namespace `gardener::gfx`.
- Interface-only `DisplayDevice`; the demo sketch defines a small fake device.
- Grayscale 8-bit frame: one byte per pixel, row-major, stride = width, `0` black to `255` white.
- Extent-relative coordinates on every write, clipped to the extent; nothing outside the extent is
  written. `setPixel(x, y, color)` maps to `buffer[(extent.y + y) * panel.width + (extent.x + x)]`.
- The drawing surface is `setPixel(x, y, color)`, `invert(x, y, w, h)` and
  `draw<TShape>(TShape shape)`. `TShape` is a callable taking a `Canvas&` that is
  copy-constructible: `draw` takes it by value, `static_assert`s both facts with a message naming
  the contract, calls it once and stores nothing, so no shape is boxed, allocated or outlived.
- `invert` stays a canvas member because it is the one operation that reads the frame, and a shape
  draws only by writing pixels. It writes `255 - value` across the rectangle, clipped like any
  other write.
- Shapes ship as values in `gardener::gfx::shapes`, each holding its arguments and implementing
  `void operator()(Canvas&) const`: `Line`, `HLine`, `VLine`, `Rect`, `FillRect`, `RoundRect`,
  `FillRoundRect`, `Circle`, `FillCircle`, `Ellipse`, `FillEllipse`, `Triangle`, `FillTriangle`,
  `RotatedRect`, `FillRotatedRect`, `Bitmap` and `Text`. Each draws through `setPixel`, so a shape
  clips to the extent without being told where it is. `shapes::Rect` is the outlined rectangle,
  distinct from the `gfx::Rect` geometry value its arguments are measured in.
- `Bitmap(x, y, const uint8_t* bits)`: `bits[0]` is the width, `bits[1]` the height, then
  `width * height` grayscale bytes row-major. No transparency — every pixel is drawn, clipped to
  the extent. This is the library's own 8-bit format, not Adafruit's packed 1-bit layout, and the
  data has to be alive for the call.
- `Text(x, y, value, face, size, color)`: the face is the shape's own, `nullptr` meaning the
  built-in 5x7, so there is no `setFont` state on the canvas. It draws each glyph at the baseline
  with no newline handling — no character is read as a line break, so a caller that wants a second
  line draws it itself. The `GFXfont` struct and the glyph-writing code are cribbed from
  Adafruit_GFX and keep its license and attribution where they land.
- `Text` holds its string as an Arduino `String`, copied at construction, so drawing a temporary
  `const char*` or `String` is safe. Only `Text` reaches `<Arduino.h>`; `Rect`, `DisplayDevice`,
  `Canvas` and the other shapes stay pure C++.
- Follow `.github/skills/cpp` naming and formatting and `.github/skills/firmware` (no speculative
  generality; implement exactly the pinned surface).

## Files

- `libraries/Gfx/library.properties` — name, version, `includes=Gfx.h`
- `libraries/Gfx/src/Gfx.h` — umbrella header including the public headers
- `libraries/Gfx/src/gfx/Rect.h` — the `Rect` value type
- `libraries/Gfx/src/gfx/DisplayDevice.h` — the two-method interface
- `libraries/Gfx/src/gfx/Canvas.h` — `Canvas` declaration, with the `draw` template and its
  `static_assert`s
- `libraries/Gfx/src/gfx/Canvas.cpp` — `setPixel` and `invert`
- `libraries/Gfx/src/gfx/Shapes.h` — the geometry shapes and `Bitmap`, header-only and `stdint`-only
- `libraries/Gfx/src/gfx/GFXfont.h` — the Adafruit `GFXfont` struct, with its license and
  attribution
- `libraries/Gfx/src/gfx/Text.h` — the `Text` shape with the face it carries
- `libraries/Gfx/src/gfx/Text.cpp` — the glyph-writing code cribbed from Adafruit_GFX, with
  attribution
- `libraries/Gfx/examples/demo/demo.ino` — draws shapes, a bitmap and text through a fake device
- `docs/specs/graphics.md` — remove the unimplemented banner

## Verification

`arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default --libraries libraries libraries/Gfx/examples/demo`

The demo sketch includes `<Gfx.h>`, which pulls the library sources into the compile. It draws with
the shipped shapes, with a shape type of its own and with a lambda, so the `draw` template's
constraints are exercised rather than assumed. No other sketch includes `Gfx` yet, so the main build
does not touch it.

## Todos

1. `create-library-skeleton` — create the library.properties, umbrella header, and `src/gfx` layout
2. `add-rect` — the `Rect` value type
3. `add-display-device` — the `DisplayDevice` interface
4. `add-canvas-core` — `Canvas` constructor, `setPixel`, `invert`, and extent clipping
5. `add-draw` — the `draw` template and the `static_assert`s that pin the shape contract
6. `add-geometry-shapes` — the fifteen geometry shapes
7. `add-bitmap-shape` — the 8-bit grayscale `Bitmap` shape
8. `add-text-shape` — vendored `GFXfont`, the `Text` shape and the cribbed glyph code with
   attribution
9. `add-example` — the demo sketch with a fake device
10. `build-verify` — compile the demo example with arduino-cli
11. `update-spec` — remove the unimplemented banner from the spec
