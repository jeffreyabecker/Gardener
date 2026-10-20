#!/usr/bin/env python3
"""Convert a BMP into the monochrome RLE asset the firmware draws.

Any uncompressed BMP -- 1, 4, 8, 24 or 32 bits per pixel -- becomes the one-bit image the panel
can show. The output is the container docs/specs/assets.md describes: a ten-byte header, a two-entry
palette and the runs RLEBitmap walks.

Colour or grey is reduced by luminance: a pixel darker than --threshold is ink and everything else
is paper, which the renderer skips so the page keeps what was under the image. --invert turns that
round, for art drawn light on dark. --dither spreads the error of that cut, which is what a photo
or a gradient needs to survive on one bit; line art is better without it.

The two rules the decoder imposes are kept here: a run never crosses the end of a row (the
renderer discards the overshoot), and no run is longer than 255 pixels.

    python scripts/asset.py --source firmware/gardener/assets/eggplant.bmp \
                            --out firmware/gardener/data/assets/eggplant.rle --threshold 224
"""

import argparse
import sys
from pathlib import Path

# The container, and the only palette it ever holds: paper is colour 0, which the renderer treats as
# transparent, and ink is anything else.
MAGIC = b"GRL1"
FLAGS = 1  # one-byte runs are legal: the palette has 2 entries
PALETTE_PAPER = 0x0000
PALETTE_INK = 0xFFFF

PALETTE_BASE = 14 + 40


def luma(b, g, r):
    """ITU-R BT.601, which is how a colour becomes the one number the panel's bit has to come from."""
    return 0.299 * r + 0.587 * g + 0.114 * b


def int16(data, offset):
    return int.from_bytes(data[offset:offset + 2], "little", signed=True)


def int32(data, offset):
    return int.from_bytes(data[offset:offset + 4], "little", signed=True)


def read_luma(source):
    """Every pixel as a luminance, top row first, whatever the file's own layout was."""
    data = Path(source).read_bytes()

    if len(data) < 62 or data[0] != 0x42 or data[1] != 0x4D:
        sys.exit(f"{source} is not a BMP")

    pixel_offset = int32(data, 10)
    width = int32(data, 18)
    height = int32(data, 22)
    bits_per_pixel = int16(data, 28)
    compression = int32(data, 30)
    palette_count = int32(data, 46)

    if bits_per_pixel not in (1, 4, 8, 24, 32):
        sys.exit(f"{source} is {bits_per_pixel} bits per pixel; this tool reads 1, 4, 8, 24 and 32")
    if compression != 0:
        sys.exit(f"{source} is compressed (mode {compression}); save it as an uncompressed BMP")

    # A negative height means the rows are stored top down, which the format allows.
    top_down = height < 0
    height = abs(height)
    if width <= 0 or height <= 0:
        sys.exit(f"{source} has no pixels")
    if width > 4096 or height > 4096:
        sys.exit(f"{source} is {width} x {height}, which is not a panel image")

    # 1, 4 and 8 bits are indices into a palette; the 24 and 32 bit forms are colours in the pixels.
    palette_luma = []
    if bits_per_pixel <= 8:
        if palette_count == 0:
            palette_count = 1 << bits_per_pixel
        if PALETTE_BASE + palette_count * 4 > len(data):
            sys.exit(f"{source} has a truncated palette")
        for i in range(palette_count):
            entry = PALETTE_BASE + i * 4
            palette_luma.append(luma(data[entry], data[entry + 1], data[entry + 2]))

    row_bytes = ((bits_per_pixel * width + 31) // 32) * 4
    values = [0.0] * (width * height)

    for row in range(height):
        source_row = row if top_down else height - 1 - row
        row_base = pixel_offset + source_row * row_bytes
        target = row * width

        if bits_per_pixel in (24, 32):
            step = bits_per_pixel // 8
            for x in range(width):
                p = row_base + x * step
                values[target + x] = luma(data[p], data[p + 1], data[p + 2])
            continue

        for x in range(width):
            byte = data[row_base + x * bits_per_pixel // 8]
            if bits_per_pixel == 1:
                index = (byte >> (7 - (x & 7))) & 1
            elif bits_per_pixel == 4:
                index = (byte >> 4) & 0x0F if (x & 1) == 0 else byte & 0x0F
            else:
                index = byte
            values[target + x] = palette_luma[index]

    return width, height, bits_per_pixel, values


def dither(width, height, values, threshold):
    """Floyd-Steinberg: the error of rounding a pixel to paper or ink is carried into the neighbours
    the scan has not reached yet, which is what turns a gradient into a dot pattern."""
    for y in range(height):
        # Serpentine, so the pattern has no directional streak.
        left_to_right = y % 2 == 0
        target = y * width
        for step in range(width):
            x = step if left_to_right else width - 1 - step
            old = values[target + x]
            new = 0.0 if old < threshold else 255.0
            carry = old - new
            values[target + x] = new

            ahead = 1 if left_to_right else -1
            behind = -ahead
            if 0 <= x + ahead < width:
                values[target + x + ahead] += carry * 7 / 16
            if y + 1 < height:
                below = target + width
                if 0 <= x + behind < width:
                    values[below + x + behind] += carry * 3 / 16
                values[below + x] += carry * 5 / 16
                if 0 <= x + ahead < width:
                    values[below + x + ahead] += carry * 1 / 16


def pack_runs(width, height, values, threshold, invert):
    """Runs: palette index 1 is ink, 0 is paper. One byte holds a four-bit length and a four-bit
    index; anything longer is the two-byte form -- index, then length -- and never crosses a row."""
    runs = bytearray()
    ink_pixels = 0

    for y in range(height):
        target = y * width
        x = 0
        while x < width:
            is_ink = values[target + x] >= threshold if invert else values[target + x] < threshold
            length = 1
            while x + length < width and length < 255:
                next_ink = (values[target + x + length] >= threshold if invert
                            else values[target + x + length] < threshold)
                if next_ink != is_ink:
                    break
                length += 1
            if is_ink:
                ink_pixels += length

            index = 1 if is_ink else 0
            if length <= 15:
                runs.append((length << 4) | index)
            else:
                runs.append(index)
                runs.append(length)
            x += length

    return runs, ink_pixels


def build(source, out, threshold, invert, should_dither):
    width, height, bits_per_pixel, values = read_luma(source)

    if should_dither:
        dither(width, height, values, threshold)

    runs, ink_pixels = pack_runs(width, height, values, threshold, invert)

    # "GRL1", the size, the flags, the palette length, then the palette.
    asset = bytearray(MAGIC)
    asset += width.to_bytes(2, "little")
    asset += height.to_bytes(2, "little")
    asset.append(FLAGS)
    asset.append(2)
    asset += PALETTE_PAPER.to_bytes(2, "little")
    asset += PALETTE_INK.to_bytes(2, "little")
    asset += runs

    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(asset)

    packed = width * height // 8
    mode = "light is ink" if invert else "dark is ink"
    if should_dither:
        mode += ", dithered"
    print(f"{Path(source).name}: {width}x{height}, {bits_per_pixel} bits per pixel, {mode}, "
          f"threshold {threshold}")
    print(f"  {ink_pixels} ink pixels, {len(runs)} bytes of runs, {len(asset)} bytes in the file "
          f"(1-bit BMP would be {packed})")
    print(f"  wrote {out}")


def main():
    parser = argparse.ArgumentParser(description="Convert a BMP into the panel's RLE asset.")
    parser.add_argument("--source", required=True, help="the uncompressed BMP to read")
    parser.add_argument("--out", required=True, help="the .rle file to write")
    # The luminance a pixel has to fall below to be ink, 0 to 255. The default sits in the middle,
    # which is where a black-on-white drawing lands either side of.
    parser.add_argument("--threshold", type=int, default=128,
                        help="the luminance (0-255) a pixel has to fall below to be ink")
    # The drawing is the light part, for art made light on dark.
    parser.add_argument("--invert", action="store_true", help="the drawing is the light part")
    # Spread the error of the cut over the neighbours the scan has not reached, so a gradient becomes
    # a dot pattern rather than a hard edge. Photographs need it; line art does not.
    parser.add_argument("--dither", action="store_true",
                        help="spread the error of the cut over neighbouring pixels")
    args = parser.parse_args()

    if not 1 <= args.threshold <= 254:
        parser.error("--threshold must be between 1 and 254")
    if not Path(args.source).is_file():
        sys.exit(f"no such file: {args.source}")

    build(args.source, args.out, args.threshold, args.invert, args.dither)


if __name__ == "__main__":
    main()
