# How should the canvas scale the ink it draws into a larger frame?

The Gfx library draws through `setPixel`, and its only transform is `rotate()`. The ink is 1:1: a
stroke is one pixel, a face is drawn at an integer `size`, and [assets.md](../specs/assets.md) says
a mark is drawn whole at its own size. Layout already resolves boxes as fractions of the panel, so a
larger panel moves and grows the boxes on its own — what does not move is the ink inside them. The
shape being weighed is draw at the design size into a temporary frame, resample that frame into the
display frame, then flush. Three questions follow: what the resample is, where it can live given that
Gfx is deliberately portable C++, and which parts of the geometry deserve to be types rather than
values.

## What the resample can be

**Nearest, by index table.** One source row per destination row and one source column per
destination column, from a table built once per scale. Whole-number upscale degenerates to row
duplication and byte doubling, so the inner loop has no multiply in it at all. Exact and cheap; at a
non-integer ratio it drops and repeats unevenly, which is visible in a stem.

**Two-tap bilinear, separable.** A horizontal pass per source row into a scratch line, then a
vertical pass combining two scratch lines into a destination row: 2 MACs per output pixel per axis,
under 100,000 multiply-accumulates for 256×128. That is the smallest filter that survives a
downscale — averaging and then thresholding recovers text that point sampling shreds — and a
downscale is the only direction where a filter beats nearest-neighbour at all, because upscaling a
1-bit image invents nothing that a threshold does not then throw away.

**Bilinear as a banded matrix product.** The vertical pass is a 1×2 by 2×W product, which is a shape
`dspm_mult_f32` takes; the horizontal pass is a 2-tap dot product per output column, which at best is
`dsps_dotprod_f32` and is no matrix at all. In the installed core (arduino-esp32 3.0.3) the esp-dsp
headers are on the sketch's include path and the prebuilt library on its link line, and on the S3
`dspm_mult_f32` and `dspm_mult_s16` both resolve to the `_aes3` PIE-SIMD kernels. That is what "chip
matrix math" would mean here. It is worth being precise about what it buys: the multiply on a
256-wide row is 512 MACs, and the byte-to-word expansion before it and the clamp back to a byte
after it are the passes that dominate either way. The whole resample written as `Vy · src · Hx` is
5.2 million MACs at 2×, forty times the separable form, so the matrix earns its place as a 2×W band
and not as the transform.

**Forward replication in the canvas.** `setPixel` writes a block, or skips, under the scale already
in the canvas coordinate mapping. No temporary frame, and vector ink comes out right at a whole
number; it leaves gaps no ratio can close, and it cannot resample a bitmap or a glyph at all.

## Where it can live

**In Gfx, with one chip translation unit.** The public type stays in `gardener::gfx` and the
resampler has a portable body plus an `#if defined(__XTENSA__)` body that reaches `<esp_dsp.h>`. The
cost is a change of stance: Gfx includes nothing but `<stdint.h>`, `<math.h>` and `<type_traits>`
today, so this widens it from a library with no platform dependency to one chip dependency, in one
file.

**Above Gfx, in the application.** Gfx carries the temporary frame and the copy; the filter is the
app's. The cost is that the demo sketch and the host harness cannot render a scaled frame, and the
harness is the cheapest place to look at one — its GIF is already a 2× nearest-neighbour enlargement.

**The affine canvas.** `rotate()`'s `rotation_` field becomes a 2×3 matrix, so scale, rotation and
translation come from one transform and shapes rasterise at destination resolution with no temporary
frame. This is the fallback the chip kernel is being preferred to, and it is the right fallback for a
platform without one — but it transforms writes rather than samples, so a bitmap drawn through it
needs supersampling to avoid holes and text drawn through it is a doubled glyph rather than a
filtered one.

## How the geometry should be expressed

This axis is independent of the two above, and unlike the chip kernel it costs nothing against the
library's portability: a canvas parameterised on sizes is still nothing but `<stdint.h>` and
`<type_traits>`.

**All runtime, as now.** One `Canvas` type, and `stride()` re-derives itself on every pixel write: a
branch on the layout and a multiply by the panel's width. The frame is a pointer and a promise that
the caller's bytes are big enough.

**`Canvas<Width, Height>` over the frame.** The template arguments are the *frame's* geometry — the
panel width, and with it the row stride and the layout — while the extent and the rotation stay
runtime values. One type per panel, so `draw`, the shapes and everything that draws still see a
single canvas type and none of them becomes a template; `rotate()` still returns the same type,
because the stride never turns. What it buys: `panelY * stride + panelX` folds to a shift, the
`Gray8`/`Packed1` branch disappears, and the byte count stops being a promise. What it costs: the
four out-of-line functions in `Canvas.cpp` are templates now, so they move into the header. `contains()`
stays runtime, because the extent is: it is the layout's answer for this draw, not the frame's. That
is a handful of cycles per pixel — a fraction of a millisecond over an 8192-pixel frame, real but
not decisive on its own.

**The extent as a parameter too.** Tempting and wrong: every pane is its own type, so every component
that draws into a pane becomes a template on it, and the layout — which computes extents at draw time
— could not name a type for the result. Template the frame, not the space drawn in. Spelled the
codebase's way, `Canvas<int16_t Width, int16_t Height>`: PascalCase like every other constant, and
signed to match `Rect`, not the sketch's lowercase `uint16_t`.

**A `Frame<Width, Height>` that owns the bytes.** The same parameters, but the type also holds
`uint8_t bits[Width * Height]`. That is what answers the temporary frame question structurally:
8 KB of `.bss` whose size the compiler checks, rather than a pointer the caller promises to keep
alive. `Canvas` becomes a view of a frame it cannot outgrow, and the scaling case's design buffer
stops being a number in a comment.

**A second type only where scaling needs one.** Scaling is the one place two geometries exist at
once: a `Frame<128, 64>` of ink and a `Canvas<256, 128>` of glass. Shapes drawing through either have
to become `template <typename TCanvas> void operator()(TCanvas&) const`, which moves the bodies in
`Shapes.cpp` into the header and emits each shape twice. That is the price of the second type, and
the reason to keep the count at two rather than letting each pane be one.

**The scale factor as its own parameter.** Separate from the frame's size, and the one that pays for
the matrix: a blit templated on the ratio has its weight vector and its source index table as
`constexpr` data in flash, its row pass unrolls, and the 1×2 matrix handed to `dspm_mult_s16` is a
constant instead of a stack array. Each distinct ratio is another instantiation, which is cheap if
the panel is a whole multiple of the design size and is not if the panel is arbitrary.

## What has to be true

- The design size becomes a constant rather than a panel fact — and, on the template path, a type.
  `screen.width()` and `page_width()` answer from the panel today, so a temporary frame means the
  components keep laying out in design units and the blit becomes the only thing that knows the
  panel. That is the expensive part of this, and it is a contract in
  [components.md](../specs/ui/components.md) and [bindings.md](../specs/ui/bindings.md), not a
  filter.
- The temporary frame is a `Frame<Width, Height>`, not a block the caller keeps: 128×64 Gray8 is
  8 KB, eight times the 1 KB a 128×64 one-bit panel keeps, and a 400×240 destination is 96 KB on its
  own. Those are sizes worth having the compiler check rather than a comment.
- The panel's geometry is stated twice — by `DisplayDevice::size()` at runtime and by the canvas's
  arguments at compile time. A virtual interface cannot promise a constant, so the two agree only
  because the driver that knows the glass and the canvas that draws into it are chosen at the same
  place. `AdafruitDisplay<Driver>` already has that shape, so the seam exists; nothing checks it.
- A Gray8 temporary frame feeding a one-bit panel thresholds after the resample and never before.
  That ordering is the whole of the antialiasing story on this glass.
- A kernel call per row means scaling the page once amortises the setup and scaling each component's
  ink in its own small buffer does not.

## What is still unknown

- What panel the scaling is for. A whole-number enlargement and an arbitrary one are different
  problems, and nearest-neighbour is right for the first and wrong for the second.
- Whether any of it pays below roughly 200×150, where a scalar separable pass is already under a
  millisecond and a SIMD kernel saves a fraction of it. Nothing has been measured.
- Whether `Text`'s integer `scale` survives. A scaled canvas makes it redundant, unless the canvas
  scales only what goes through `draw` and the ceiling stays.
- Whether the blit ends in a second Gray8 pass and a pack, or writes the panel's bits directly.
- Whether header-only shapes are worth the constant stride: every shape emitted twice, in exchange
  for a shift and a branch removed from each pixel write. Nothing has been measured, and the answer
  probably differs between one canvas type and two.
- Whether the ratio is a type or an argument — `constexpr` weights and an unrolled pass against one
  table built per draw.

## Recommendation

Draw into a design-size Gray8 `Frame` and blit separably into the display frame: nearest with an index
table at a whole-number ratio, two-tap bilinear when it is not. Template the frame's geometry — panel,
stride and layout — and let the frame own its bytes, while the extent and the rotation stay runtime so
there is one canvas type per panel and nothing that draws becomes a template. Let a second type exist
only where the design frame forces one, and make the ratio a parameter of the blit rather than of the
canvas. Keep the vertical pass's weight vector as the seam where the scalar loop becomes
`dspm_mult_f32`, and take the affine canvas as the fallback for a platform without the kernel — after
there is one to fall back on. Held weakly: the panel is unknown, the contract change a design-size
frame forces is bigger than the resample that motivated it, and the template buys less than it looks
like it will.
