# Assets

The pictures the panel draws are files on the device, not arrays in a header: one image is a file
under `/assets/`, written by [scripts/asset.ps1](../scripts/asset.ps1), and read once at boot.

Run-length encoding is what makes this cheap. A one-bit drawing is long runs of the same pixel, so
the 128x64 mark ships in **367 bytes** rather than the 1024 the packed array took. The decoder is
[MHotchin/RLEBitmap](../libraries/RLEBitmap), vendored unmodified in `libraries/RLEBitmap`.

## Making an asset

Save the drawing as an **uncompressed 1-bit BMP** — two colours, which is what the panel has — and
convert it:

```powershell
powershell -File scripts/asset.ps1 -Source firmware/gardener/assets/eggplant.bmp `
                                      -Out firmware/gardener/data/assets/eggplant.rle
```

`-Ink` says which colour is the drawing. It defaults to `Black`, for art drawn dark on light; say
`-Ink White` for art drawn light on dark. Everything that is not ink becomes paper, and paper is not
drawn at all — whatever the page already put there shows through.

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

`App::begin()` reads `/assets/eggplant.rle` into the heap once, after the filesystem mounts, and
`AppContext::mark()` is what both pages draw — the disconnected page at rest, and the connected
page's first frame. An asset is drawn whole and at its own size: there is no scaling.

A missing file is not fatal. The log says `ui: /assets/eggplant.rle missing, the pages have no
mark` and the pages draw nothing, because the data image is where the file ships from and its
absence is a packaging fault rather than a mode to support.

The art itself lives in [firmware/gardener/assets/](../firmware/gardener/assets), outside `data/`,
so a source image is never uploaded to the device.
