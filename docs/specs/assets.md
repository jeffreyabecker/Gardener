# Assets

The pictures the panel draws are files on the device, not arrays in a header: one image is a file
under `/assets/`, written by [scripts/asset.ps1](../../scripts/asset.ps1), and read when it is drawn.

Run-length encoding is what makes this cheap. A one-bit drawing is long runs of the same pixel, so
the 128x64 mark ships in **327 bytes** rather than the 1024 the packed array took. The decoder is
[MHotchin/RLEBitmap](../../libraries/RLEBitmap), vendored unmodified in `libraries/RLEBitmap`.

## Making an asset

Save the drawing as an **uncompressed BMP** — 1, 4, 8, 24 or 32 bits per pixel — and convert it:

```powershell
powershell -File scripts/asset.ps1 -Source firmware/gardener/assets/eggplant.bmp `
                                      -Out firmware/gardener/data/assets/eggplant.rle
```

Whatever the source depth, the output is one bit: a pixel becomes ink when its luminance falls below
`-Threshold`, and everything else is paper, which is not drawn at all — whatever the page already put
there shows through.

| Flag | Default | Meaning |
| --- | --- | --- |
| `-Threshold` | `128` | the luminance (0–255) a pixel has to fall below to be ink |
| `-Invert` | off | the drawing is the *light* part, for art made light on dark |
| `-Dither` | off | spread the error of that cut over the neighbouring pixels |

Line art wants the defaults; `-Dither` is for a photograph or a gradient, where a hard cut loses
everything between black and white. Dithering a two-colour drawing only adds noise.

The shipped mark is made with `-Threshold 224`. Its illustration is shaded, so the default cut leaves
the lighter half of the body and the tip of the stem as paper; at 224 everything that is not the
white background is ink, which is the whole eggplant at 1337 pixels and 327 bytes.

A 16-bit BMP and a compressed one are refused with a message rather than guessed at. A 32-bit pixel
is read as BGR with its alpha byte ignored.

Then upload the data image (`-Task data`), which is how every file on the device gets there.

## The file

```
offset  size  contents
0       4     "GRL1"
4       2     width, in pixels
6       2     height, in pixels
8       1     flags: bit 0 set means one-byte runs are legal (a palette of 16 or fewer)
9       1     palette entries
10      2*n   the palette, one RGB565 little-endian word per entry
10+2*n  ...   the runs, to the end of the file
```

Everything is little endian, and the palette is always two entries: **paper** is colour `0x0000`,
which the renderer treats as transparent, and **ink** is `0xFFFF`. That normalisation is what lets one
renderer serve a one-bit panel: a run is either ink or nothing.

A run is one byte when its length is at most 15 — high nibble the length, low nibble the palette
index — and two bytes otherwise: the index, then the length. Two rules come from the decoder rather
than from taste:

- **A run never crosses the end of a row.** The renderer discards the overshoot and starts the next
  row with the next run, so pixels past the row edge are simply lost.
- **A run is at most 255 pixels**, because the length is one byte.

The converter keeps both, which is why it is the tool to use rather than a hand-built stream.

## On the device

`drawAsset()` is the whole of it: it reads the file named, renders it at the point asked for, and
lets the bytes go. The disconnected page calls it on every redraw, so there is no loaded copy
anywhere and nothing to keep in step with the file. A few hundred bytes every redraw is cheaper than
the state an owned buffer would need.

An asset is drawn whole and at its own size: there is no scaling.

A file that is not there is not fatal. `App::begin()` says `ui: /assets/eggplant.rle missing, the
disconnected page has no mark` and the page draws nothing, because the data image is where the file
ships from and its absence is a packaging fault rather than a mode to support.

The art itself lives in [firmware/gardener/assets/](../../firmware/gardener/assets), outside `data/`,
so a source image is never uploaded to the device.
