# Graphics

The contract for a standalone C++ drawing library: a caller-owned frame buffer, the
`DisplayDevice` that owns the glass and flushes the buffer, and the `Canvas` that draws into an
extent of it. The library is built from scratch and inherits nothing from Adafruit_GFX; that name
appears in this file only to say what the library does not do.

> **Unimplemented.** This file is the contract the library is being written to; no part of it is
> backed by code yet.

This library is its own library, separate from the main application library, and the main
application depends on it. The dependency runs one way: the application may include this library,
and this library never includes, inherits from, or reaches into the application.

The library's three types — `Rect`, `DisplayDevice` and `Canvas` — live in the `gardener::gfx`
namespace. A concrete device driver is the one place that knows its own glass and bus; it is an
implementation of `DisplayDevice`, not a fourth contract here.

## The frame buffer

The library is always buffered. Every drawing verb writes into a frame buffer the caller owns;
nothing reaches the glass until `flush`. There is no verb that draws straight to the glass.

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

`Canvas` is the only place drawing verbs live; `DisplayDevice` and `Rect` have none.

Every verb takes coordinates relative to the extent's top-left. A verb given `(x, y)` addresses the
buffer at `(extent.x + x, extent.y + y)`.

Every verb clips to the extent. Pixels outside `[0, extent.width) × [0, extent.height)` are not
written, and a shape that crosses the extent's edge is cut at the edge. Nothing outside the extent
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

### The rest of the surface

The same rule governs every other verb `Canvas` carries: lines (`line`, `hLine`, `vLine`),
rectangles (`rect`, `fillRect`, `roundRect`, `fillRoundRect`), circles (`circle`, `fillCircle`),
ellipses (`ellipse`, `fillEllipse`), triangles (`triangle`, `fillTriangle`), turned rectangles
(`rotatedRect`, `fillRotatedRect`), inversion (`invert`), bitmaps (`bitmap`) and text. Each takes
extent-relative coordinates, clips to the extent, and writes into the buffer at panel-relative
offsets.

Text and bitmaps obey that rule like every other verb. The glyph-writing code is cribbed from
Adafruit_GFX and keeps its attribution, and text reads Adafruit's `GFXfont` format through it. The
library does no newline handling: it draws each glyph it is given and reads no character as a line
break, so a caller that wants a second line draws it itself. The bitmap format is the library's
own, not Adafruit's packed-bitmap layout.
