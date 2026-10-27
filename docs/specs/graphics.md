# Graphics

The contract for a standalone C++ drawing library: the `DisplayDevice` that owns the glass and the
frame it draws on, and the `Canvas` that draws into an extent of that frame. The library is built
from scratch and inherits no Adafruit_GFX code. Its font structures and glyph writer are derived
from Adafruit_GFX, and that attribution travels with them.

This library is its own library, separate from the main application library, and the main
application depends on it. The dependency runs one way: the application may include this library,
and this library never includes, inherits from, or reaches into the application.

`Rect`, `DisplayDevice` and `Canvas` live in the `gardener::gfx` namespace, as do the `Font` and
`Glyph` structures text is drawn through; the shapes the library ships are in
`gardener::gfx::shapes`. A concrete device driver is the one place that knows its own glass and
bus; it is an implementation of `DisplayDevice`, not a fourth contract here.

## The frame buffer

The library is always buffered. Every drawing call writes into a frame buffer the device owns;
nothing reaches the glass until `flush`. There is no call that draws straight to the glass.

A frame is `size().width` by `size().height` pixels, row-major, top-left origin, in one of two
layouts picked by the device's `bitsPerPixel()`:

- `Gray8`, one byte per pixel: a grayscale brightness from `0` — black, the panel off — through
  `255` — white, the panel fully on — linear in between. The stride is the panel's width and there is
  no padding.
- `Packed1`, one bit per pixel: eight pixels to a byte, most significant bit leftmost, each row
  padded out to a whole byte. A colour of `128` or more sets the bit; anything below it clears it.
  This is the layout a one-bit panel keeps its own buffer in.

The buffer is allocated and owned by the device, which returns it from `buffer()`. The library never
allocates or frees it, and a `Canvas` stores the pointer it is given and relies on the device
keeping that memory alive for as long as the canvas is used.

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

`DisplayDevice` is the interface a concrete device implements. It has five methods:

```cpp
virtual bool begin() = 0;
virtual Rect size() const = 0;
virtual uint8_t bitsPerPixel() const = 0;
virtual uint8_t* buffer() = 0;
virtual void flush(const Rect& dirty) = 0;
```

`begin()` brings the part up — power, bus setup, a bring-up handshake — and is called once, before
anything draws. False means the part did not answer, which is the caller's to judge: only it knows
whether a dark panel is a fault or a mode. A device with nothing to bring up answers true.

`size()` reports the panel's full-frame geometry as a `Rect` with its origin at the top-left:
`Rect(0, 0, width, height)`. It is `const`: the geometry does not change across a call.

`bitsPerPixel()` reports the depth the glass shows — `1` for a panel that is only on or off, `8` for
one with levels — and so the layout of the frame above.

`buffer()` returns the frame the device owns and draws into: `size().width` by `size().height`
pixels in that layout. A canvas draws straight into it, so there is one frame and no copy per
canvas. The device keeps it for as long as it lives.

`flush(const Rect& dirty)` sends the pixels inside `dirty` to the glass, and only those. The
rectangle is what a canvas has written since it last flushed, in the panel's own coordinates, so a
device sends a corner it redrew rather than the whole frame. A rectangle holding no pixels — a
width or height of zero or less — asks for nothing, which is what a canvas nobody has drawn on
reports. The device reads the frame and writes none of it back.

Anything the part needs to come up is the concrete device's own business, behind `begin()`: the
interface says when to do it and not what it is, and every draw after it uses the frame the device
already owns.

## Canvas

```cpp
Canvas(uint8_t* buffer, Rect panel, Rect extent, PixelLayout layout = PixelLayout::Gray8)
```

- `buffer` is the frame the device owns, `panel.width × panel.height` pixels in `layout`.
- `panel` is the full-frame geometry — the same `Rect` the device's `size()` reports.
- `extent` is the drawable region, a rectangle inside the panel. The canvas writes nothing outside
  it.
- `layout` is the frame's layout: `PixelLayout::Gray8` or `PixelLayout::Packed1`.

`extent` must lie within `panel`. The canvas clips to `extent`, not to `panel`, so a caller that
passes an extent reaching outside the panel owns the out-of-bounds write that follows.

A canvas holds the drawing state: the buffer, the turn it draws at, and the rectangle of the panel
it has written since the last flush. It is therefore neither copyable nor movable — a copy would be
a second set of writes to the same buffer, and only one of the two could be flushed. The one way to
have a second canvas over the same frame is `child`, below, and it shares that rectangle rather
than keeping one of its own.

### over

```cpp
static Canvas over(DisplayDevice& device)
```

The canvas a whole panel draws on: the device's own buffer, its `size()` as both the panel and the
extent, and the layout its depth implies — `Packed1` for a one-bit glass, `Gray8` for anything
deeper. It is the constructor above with the device's own answers filled in, so the mapping from
depth to layout is written once rather than in every composition. Returned by value like `child`,
so the copy is elided.

### child

```cpp
Canvas child(Rect area) const
```

A canvas over part of this one: the same buffer, the same panel, the same layout, clipped to `area`
and with its origin at `area`'s top-left. `area` is in this canvas's own coordinates, so a point
given as `(x, y)` on the child is `(area.x + x, area.y + y)` on the parent.

`area` is clamped to this canvas's extent, so a child never reaches a pixel its parent could not,
and an area that clamps to nothing yields a canvas that draws nothing — not a pointer to check.
The child is returned by value, and the copy is elided, so a caller writes
`Canvas plot = canvas.child(rect);`. The result is as uncopyable and unmovable as the canvas it came
from, and a child of a child resolves against its own extent, so nesting needs nothing extra.

The child keeps this canvas's turn, which is what keeps its axes lined up with the parent's
coordinate space: the child's extent is the bounding box of `area` in the panel's coordinates, and a
quarter turn swaps its two sides exactly as it swaps the parent's. A child therefore cannot be
turned again — turning one would draw outside the rectangle it was given — so `setRotation` does
nothing on a child and `rotation()` answers the turn it inherited.

The whole tree shares one dirty rectangle: drawing through a child grows the same record the canvas
that owns the frame reports, so one `flush` on the root sends everything drawn through any canvas
made from it.

### setRotation

```cpp
void setRotation(int16_t degrees)
```

Turns the canvas in place: 90, 180 or 270 degrees about the extent's centre. The buffer, the panel
and the extent stay as they were, so a turned canvas writes inside the same rectangle of the glass
with the coordinates of the page turned within it, and its two sides swap for a quarter turn. An
angle that is not a quarter turn leaves the canvas as it was. `rotation()` reports the angle it is
turned to. A child of the canvas cannot be turned: a call on one leaves it at the turn it inherited.

## Drawing

`Canvas` is the only place drawing lives; `DisplayDevice` and `Rect` have none.

The canvas carries three drawing calls: `setPixel`, `invert` and the `draw` template. Everything a
page puts on the frame is a pixel written through `setPixel`, a region the canvas inverts itself, or
a shape value handed to `draw` that writes pixels of its own. `flush` is the fourth call and writes
nothing: it is how what those three drew reaches the glass.

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
On a `Gray8` frame that byte is what lands in the pixel; on a `Packed1` one `128` or more sets the
bit and anything below it clears it. A call whose point falls outside the extent does nothing.

### invert

```cpp
void invert(Rect area)
```

Each pixel in the rectangle becomes `255` minus what was there on a `Gray8` frame, and the bit
flipped on a `Packed1` one, so paper written over ink erases and a band laid over a border keeps the
border visible. This is the one operation that reads the frame, which is why it is a canvas member
rather than a shape: a shape draws by writing pixels and has no way to read one back. The rectangle
is extent-relative and clipped like any other write.

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

### flush

```cpp
void flush(DisplayDevice& device)
```

Hands the device the rectangle of the panel that has been written since the last flush, in panel
coordinates, and forgets it. The canvas is the thing that knows what was drawn, so the rectangle is
grown by `setPixel` and `invert` as they write rather than worked out afterwards; it stays right for
a canvas that draws at a turn or inside an extent, because the coordinates it records are the
panel's own. A canvas nobody has drawn on since the last flush reports a rectangle holding no
pixels, and the device sends nothing.

The device is an argument rather than something the canvas holds: a canvas over a buffer nobody
sends anywhere — one a test draws into and reads back — needs no device at all.

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
`SerifItalic18`, `Symbol12`, and the rest) and answers for a name:

```cpp
bool find(const char* name, const Font*& face)
```

`find` matches a routine's font name against the named faces, treating `nullptr` as the built-in
5x7, and returns false for a name that is not one of them — a mistake in the routine rather than a
face to fall back to.

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

Two helpers measure and draw one glyph where text as a shape does not fit:

```cpp
TextBox measureText(const Font* face, const char* value, uint8_t size)
void drawGlyph(Canvas& canvas, int16_t x, int16_t y, uint8_t code, uint8_t size, uint8_t color)
```

`measureText` gives the ink box of a string measured with the baseline at zero, and a `face` of
`nullptr` measures the built-in 5x7, whose bytes take a six-by-eight cell. `drawGlyph` draws one
byte of that built-in face by code, with `(x, y)` the top-left of its cell.

### Bitmap

```cpp
Bitmap(int16_t x, int16_t y, const uint8_t* bits)
```

`bits[0]` is the width, `bits[1]` the height, then `width * height` grayscale bytes row-major. Every
pixel is drawn — there is nothing to skip and no transparency — and the image is clipped to the
extent. The data has to be alive for the call, because a draw copies nothing.

`MonoBitmap(x, y, bits, color)` takes the packed one-bit image a page's own artwork uses: the same
two leading bytes, then the rows most-significant bit first, each padded to a whole byte. A set bit
draws `color` and a clear bit leaves what is under it alone, so one image can be drawn in either
ink.

