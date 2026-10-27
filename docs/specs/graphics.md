# Graphics

The contract for a standalone C++ drawing library: a caller-owned frame buffer, the
`DisplayDevice` that owns the glass and flushes the buffer, and the `Canvas` that draws into an
extent of it. The library is built from scratch and inherits no Adafruit_GFX code. Its font
structures and glyph writer are derived from Adafruit_GFX, and that attribution travels with them.

This library is its own library, separate from the main application library, and the main
application depends on it. The dependency runs one way: the application may include this library,
and this library never includes, inherits from, or reaches into the application.

`Rect`, `DisplayDevice` and `Canvas` live in the `gardener::gfx` namespace, as do the `Font` and
`Glyph` structures text is drawn through; the shapes the library ships are in
`gardener::gfx::shapes`. A concrete device driver is the one place that knows its own glass and
bus; it is an implementation of `DisplayDevice`, not a fourth contract here.

## The frame buffer

The library is always buffered. Every drawing call writes into a frame buffer the caller owns;
nothing reaches the glass until `flush`. There is no call that draws straight to the glass.

A frame is `width × height` bytes: one byte per pixel, row-major, top-left origin, stride equal to
the width, no padding. A pixel's byte is a grayscale brightness from `0` — black, the panel off —
through `255` — white, the panel fully on — linear in between. The device maps the byte to its own
depth when it flushes.

The buffer is allocated and owned by the caller. The library never allocates or frees it, stores
the pointer it is given, and relies on the caller to keep that memory alive for as long as any
`Canvas` over it is used.

## Rect

`Rect` holds four `int16_t` fields, set by its constructor:

```cpp
Rect(int16_t x, int16_t y, int16_t width, int16_t height)
```

`x` and `y` are the top-left pixel; `width` and `height` are counts, not the far edge. A `Rect`
therefore spans `x .. x + width - 1` and `y .. y + height - 1`, and `right()` is
`x + width - 1` while `bottom()` is `y + height - 1`. A `Rect` with a `width` or `height` of zero
or less holds no pixels; a `Canvas` over such an extent draws nothing.

## DisplayDevice

`DisplayDevice` is the interface a concrete device implements. It has exactly two methods:

```cpp
virtual Rect size() const = 0;
virtual void flush(uint8_t* buffer) = 0;
```

`size()` reports the panel's full-frame geometry as a `Rect` with its origin at the top-left:
`Rect(0, 0, width, height)`. It is `const`: the geometry does not change across a call.

`flush()` sends the whole panel buffer to the glass. `buffer` must point to a frame of
`size().width × size().height` bytes in the layout above; the device reads exactly that many bytes
and writes none of them back. The buffer is the caller's, not the device's, and one frame may be
reused for every flush.

Anything the part needs before the first flush — power, bus setup, a bring-up handshake — is the
concrete device's own business, done before `flush` is called. It is not a third method on this
interface.

## Canvas

```cpp
Canvas(uint8_t* buffer, Rect panel, Rect extent)
```

- `buffer` is a caller-owned frame of `panel.width × panel.height` bytes in the grayscale layout.
- `panel` is the full-frame geometry — the same `Rect` the device's `size()` reports.
- `extent` is the drawable region, a rectangle inside the panel. The canvas writes nothing outside
  it.

`extent` must lie within `panel`. The canvas clips to `extent`, not to `panel`, so a caller that
passes an extent reaching outside the panel owns the out-of-bounds write that follows.

## Drawing

`Canvas` is the only place drawing lives; `DisplayDevice` and `Rect` have none.

The canvas carries three things: `setPixel`, `invert` and the `draw` template. Everything a page
puts on the frame is a pixel written through `setPixel`, a region the canvas inverts itself, or a
shape value handed to `draw` that writes pixels of its own.

Every coordinate is relative to the extent's top-left: a point given as `(x, y)` addresses the
buffer at `(extent.x + x, extent.y + y)`.

Every write clips to the extent. Pixels outside `[0, extent.width) × [0, extent.height)` are not
written, and something that crosses the extent's edge is cut at the edge. Nothing outside the extent
is ever changed, and nothing outside the panel is ever touched so long as the caller keeps the
extent inside it.

### setPixel

```cpp
void setPixel(int16_t x, int16_t y, uint8_t color)
```

`x` and `y` are relative to the extent's top-left, and `color` is the grayscale byte `0 .. 255`.
A call whose point falls outside the extent does nothing. A call inside writes `color` at

```text
buffer[(extent.y + y) * panel.width + (extent.x + x)]
```

### invert

```cpp
void invert(int16_t x, int16_t y, int16_t w, int16_t h)
```

Each pixel in the rectangle becomes `255` minus what was there, so paper written over ink erases and
a band laid over a border keeps the border visible. This is the one operation that reads the frame,
which is why it is a canvas member rather than a shape: a shape draws by writing pixels and has no
way to read one back. The rectangle is extent-relative and clipped like any other write.

### draw

```cpp
template <typename TShape>
void draw(TShape shape)
```

`TShape` is a callable that accepts a `Canvas&` and is copy-constructible. `draw` takes it by value,
calls it once with the canvas, and stores nothing: no shape outlives the call, and the library
allocates nothing to hold one.

A shape draws by calling the canvas it was given — `setPixel`, `invert`, or another `draw` — so it
clips to the extent exactly as a direct call would, whether or not whoever wrote it knew the extent
existed. A type that cannot be called with a `Canvas&`, or cannot be copied, does not compile as a
`TShape`.

## Shapes

The library ships the vocabulary a page draws with, in `gardener::gfx::shapes`. Each is a value with
its arguments set at construction and

```cpp
void operator()(Canvas& canvas) const;
```

so `canvas.draw(shapes::FillCircle(64, 32, 10, 255))` draws a filled circle. A caller that wants
something the library does not ship passes its own callable to `draw`, and nothing else changes.
`shapes::Rect` is the outlined rectangle, and is not the `gfx::Rect` geometry value its arguments
are measured in.

### Geometry

`Line(x0, y0, x1, y1, color)`, `HLine(x, y, w, color)`, `VLine(x, y, h, color)`,
`Rect(x, y, w, h, color)`, `FillRect(x, y, w, h, color)`, `RoundRect(x, y, w, h, radius, color)`,
`FillRoundRect(x, y, w, h, radius, color)`, `Circle(x, y, radius, color)`,
`FillCircle(x, y, radius, color)`, `Ellipse(x, y, radiusX, radiusY, color)`,
`FillEllipse(x, y, radiusX, radiusY, color)`, `Triangle(x0, y0, x1, y1, x2, y2, color)`,
`FillTriangle(x0, y0, x1, y1, x2, y2, color)`, `RotatedRect(centerX, centerY, w, h, degrees,
color)` and `FillRotatedRect(centerX, centerY, w, h, degrees, color)`.

Every one takes its colour as the grayscale byte at the end of the argument list, and draws in
extent-relative coordinates clipped to the extent.

### Fonts

A `Glyph` carries one glyph: `bitmapOffset` into the face's bitmap, `width` and `height` in pixels,
`xAdvance`, and `xOffset` and `yOffset` from the cursor's baseline to the glyph's top-left. The
glyph rows are packed most-significant bit first with no padding between rows.

A `Font` carries `bitmap`, `glyph`, `first` and `last` — the character range — `yAdvance`, and three
fixed metrics: `fixedWidth`, `fixedHeight` and `fixedAdvance`. A zero fixed metric means that
measure varies from glyph to glyph. A fixed-size face has a non-zero `fixedWidth` and `fixedHeight`;
a monospace face has a non-zero `fixedAdvance`. `BuiltinFont` is the built-in 5x7 face: all 256 byte
values, five pixels wide, eight tall, six pixels apart.

The structures are forked from Adafruit_GFX's `GFXfont` and `GFXglyph` into `gardener::gfx`, and
the glyph writer and the built-in face keep Adafruit's attribution.

The compiled FreeSans, FreeMono and FreeSerif families, TomThumb, Picopixel and the UI symbol faces
ship under `gfx/fonts`, one header per face; `gfx::fonts` names them (`Sans9`, `Mono12`,
`SerifItalic18`, `Symbol12`, and the rest).

### Text

```cpp
Text(int16_t x, int16_t y, const char* value, const Font* face, uint8_t size, uint8_t color)
```

The face belongs to the shape, so a page mixing faces needs nothing from the canvas; a `face` of
`nullptr` selects `BuiltinFont`. `x` and `y` are the baseline of the first glyph, which is the font
format's own rule, and `size` scales what is drawn.

The string is a NUL-terminated run of bytes the caller keeps: a draw copies nothing, so the text
has to be alive for the draw, and naming it needs no allocation.

The library does no newline handling: it draws each glyph it is given and reads no character as a
line break, so a caller that wants a second line draws it itself.

### Bitmap

```cpp
Bitmap(int16_t x, int16_t y, const uint8_t* bits)
```

`bits[0]` is the width, `bits[1]` the height, then `width * height` grayscale bytes row-major. Every
pixel is drawn — there is nothing to skip and no transparency — and the image is clipped to the
extent. This is the library's own format, not Adafruit's packed 1-bit layout. The data has to be
alive for the call, because a draw copies nothing.

