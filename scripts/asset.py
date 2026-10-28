#!/usr/bin/env python3
"""Convert a BMP into the header the firmware compiles: the device's mark, packed one bit per pixel.

Any uncompressed BMP -- 1, 4, 8, 24 or 32 bits per pixel -- becomes the one-bit image the panel
shows. The output is a header defining MarkBits, the packed array the panel draws: the first byte
is the width, the second the height, then the pixels row-major, most-significant bit first in each
byte, each row padded to a whole byte. A set bit is ink and is drawn; a clear bit is paper, which
the renderer skips so the page keeps what was under the image.

Colour or grey is reduced by luminance: a pixel darker than --threshold is ink and everything else
is paper. --invert turns that round, for art drawn light on dark. --dither spreads the error of
that cut, which is what a photo or a gradient needs to survive on one bit; line art is better
without it.

    python scripts/asset.py --source firmware/gardener/assets/eggplant.bmp \
                            --out libraries/Gardener/src/panel/Mark.h --threshold 224
"""

import argparse
import sys
from pathlib import Path

# The name the header defines, and the namespace it lives in.
NAMESPACE = "gardener"
BITS_NAME = "MarkBits"

PALETTE_BASE = 14 + 40
BYTES_PER_LINE = 12


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


def pack_bits(width, height, values, threshold, invert):
    """One bit per pixel, row-major and most-significant first, each row padded to a whole byte."""
    stride = (width + 7) // 8
    bits = bytearray(stride * height)
    ink_pixels = 0

    for y in range(height):
        source = y * width
        target = y * stride
        for x in range(width):
            is_ink = values[source + x] >= threshold if invert else values[source + x] < threshold
            if is_ink:
                bits[target + x // 8] |= 0x80 >> (x & 7)
                ink_pixels += 1

    return bits, ink_pixels


def build(source, out, threshold, invert, should_dither):
    width, height, bits_per_pixel, values = read_luma(source)

    if should_dither:
        dither(width, height, values, threshold)

    bits, ink_pixels = pack_bits(width, height, values, threshold, invert)
    data = bytes([width, height]) + bytes(bits)

    lines = [
        "// Generated by scripts/asset.py. The first byte is the width,",
        "// the second the height; then the image, one bit per pixel, row-major, most-significant",
        "// bit first in each byte, each row padded to a whole byte. A set bit is ink and is drawn;",
        "// a clear bit is paper and is left to show what is under it.",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        f"namespace {NAMESPACE} {{",
        "",
        f"constexpr uint8_t {BITS_NAME}[] = {{",
        f"  0x{data[0]:02X}, 0x{data[1]:02X},  // width, height",
    ]
    for i in range(2, len(data), BYTES_PER_LINE):
        row = ", ".join(f"0x{b:02X}" for b in data[i:i + BYTES_PER_LINE])
        lines.append(f"  {row},")
    lines.append("};")
    lines.append("")
    lines.append(f"}}  // namespace {NAMESPACE}")
    lines.append("")

    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines))

    mode = "light is ink" if invert else "dark is ink"
    if should_dither:
        mode += ", dithered"
    print(f"{Path(source).name}: {width}x{height}, {bits_per_pixel} bits per pixel, {mode}, "
          f"threshold {threshold}")
    print(f"  {ink_pixels} ink pixels, {len(data)} bytes in {BITS_NAME}, wrote {out}")


def main():
    parser = argparse.ArgumentParser(description="Convert a BMP into the panel's packed mark header.")
    parser.add_argument("--source", required=True, help="the uncompressed BMP to read")
    parser.add_argument("--out", required=True, help="the header to write")
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
