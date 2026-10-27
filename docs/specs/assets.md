# Assets

The mark the panel draws when no page is active is compiled into the firmware, not a file on the
device: [scripts/asset.py](../../scripts/asset.py) turns the source image into a header the panel
build includes, and the renderer draws the packed bits directly. There is no run-length encoding and
no file under `/assets/`.

## Making the mark

Save the drawing as an **uncompressed BMP** — 1, 4, 8, 24 or 32 bits per pixel — and convert it:

```powershell
python scripts/asset.py --source firmware/gardener/assets/eggplant.bmp \
                        --out libraries/Gardener/src/panel/Mark.h --threshold 224
```

Whatever the source depth, the output is one bit: a pixel becomes ink when its luminance falls below
`--threshold`, and everything else is paper, which is not drawn at all — whatever the page already put
there shows through.

| Flag | Default | Meaning |
| --- | --- | --- |
| `--threshold` | `128` | the luminance (0–255) a pixel has to fall below to be ink |
| `--invert` | off | the drawing is the *light* part, for art made light on dark |
| `--dither` | off | spread the error of that cut over the neighbouring pixels |

Line art wants the defaults; `--dither` is for a photograph or a gradient, where a hard cut loses
everything between black and white. Dithering a two-colour drawing only adds noise.

The shipped mark is made with `--threshold 224`. Its illustration is shaded, so the default cut leaves
the lighter half of the body and the tip of the stem as paper; at 224 everything that is not the
white background is ink, which is the whole eggplant at 1337 pixels.

A 16-bit BMP and a compressed one are refused with a message rather than guessed at. A 32-bit pixel
is read as BGR with its alpha byte ignored.

The header is then compiled with the panel build, which is the only place the mark needs to be.

## The header

The converter writes one name in the `gardener` namespace:

```cpp
constexpr uint8_t MarkBits[] = { 0x80, 0x40, ... };
```

`MarkBits` carries its own size: the first byte is the width and the second the height, then the
image packed one bit per pixel — row-major, the most-significant bit first in each byte, and each row
padded to a whole byte, so a 128-pixel row is 16 bytes. A set bit is ink; a clear bit is paper.

## On the device

`gfx::shapes::MonoBitmap` is the whole of it: it reads the width and the height from the first two
bytes, walks the rows and draws each ink pixel, skipping the paper. The panel page calls it on every
redraw from the array the header defines, so there is no file to read and no loaded copy to keep in
step.

A mark is drawn whole and at its own size: there is no scaling.

The art itself lives in [firmware/gardener/assets/](../../firmware/gardener/assets), outside the
build, so the source image is not compiled — the header generated from it is.
