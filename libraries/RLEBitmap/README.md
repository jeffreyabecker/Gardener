# RLEBitmap (vendored)

Run-length encoded bitmaps from [MHotchin/RLEBitmap](https://github.com/MHotchin/RLEBitmap),
upstream 1.2.0, MIT — see [LICENSE](./LICENSE). The two files in `src/` are upstream's, unmodified.

Only `src/` is here. Upstream also ships weather and moon image sets, the `bmper` encoder and example
sketches; none of them are part of this build. This firmware's images live on the device rather than
in headers, so they are built with [scripts/asset.ps1](../../scripts/asset.ps1) and stored under
`firmware/gardener/data/assets/` — the format and the tool are described in
[docs/specs/assets.md](../../docs/specs/assets.md).

The renderer walks a `RLEBitmapInfo` and calls `writeFastHLine` on an `RLEGraphicsContext`. The
firmware supplies its own context over the panel in `libraries/VacuumCore/src/app/Asset.cpp`, so the
`_ADAFRUIT_GFX_H` and `_TFT_eSPIH_` adapters at the bottom of the header are unused here.

`RLEBitmapInfo` holds 32-bit "far addresses" read with `pgm_read_byte_far`; on the ESP32 core those
are ordinary pointer reads with `PROGMEM` empty, so the data can be a RAM buffer as easily as a flash
array — which is what makes reading an image from LittleFS possible without a copy into PROGMEM.
