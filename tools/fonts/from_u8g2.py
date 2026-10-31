#!/usr/bin/env python3
"""Convert a u8g2 single-font source into a Gfx face.

    python tools/fonts/from_u8g2.py u8g2_font_t0_16_tf.c Ttyp016
    python tools/fonts/from_u8g2.py u8g2_font_t0_16_tf.c Ttyp016 --first 0x20 --last 0xff

u8g2 keeps a face packed and run-length encoded, in a form only its own decoder reads. Gfx draws
from a plain Glyph table, so this decodes the face once and writes it out as C++: the bitmaps packed
most-significant bit first and byte-aligned per glyph, as Glyph says, and one Glyph for every code in
the range asked for — a code the face has no glyph for is a blank as wide as the advance, so a
character the face lacks still moves the cursor rather than collapsing the string.

It writes NAME.h and NAME.cpp into libraries/Gfx/src/fonts/. The face's own licence is not
reproduced here: put it beside the header, which is what the ttyp0 faces asked for.

The format is the one at https://github.com/olikraus/u8g2/wiki/u8g2fontformat, decoded the way
u8g2_font.c decodes it: bits are read least-significant bit first within each byte, and a glyph's
bitmap is a run of zeros then a run of ones, repeated while the next bit is set.
"""

import argparse
import re
import textwrap
from pathlib import Path

FONTS = Path(__file__).resolve().parents[2] / "libraries" / "Gfx" / "src" / "fonts"

# The u8g2 font header's size and the offsets of the fields this reads.
HEADER_SIZE = 23
OFFSET_GLYPHS = 0
OFFSET_M0 = 2
OFFSET_M1 = 3
OFFSET_BITS_W = 4
OFFSET_BITS_H = 5
OFFSET_BITS_X = 6
OFFSET_BITS_Y = 7
OFFSET_BITS_D = 8
OFFSET_ASCENT = 13
OFFSET_DESCENT = 14


def unescape(literal):
    """The bytes a C string literal holds, for the escapes u8g2 writes."""
    out = bytearray()
    at = 0
    while at < len(literal):
        character = literal[at]
        if character != "\\":
            out.append(ord(character))
            at += 1
            continue
        at += 1
        escaped = literal[at]
        if escaped in "01234567":
            digits = ""
            while at < len(literal) and len(digits) < 3 and literal[at] in "01234567":
                digits += literal[at]
                at += 1
            out.append(int(digits, 8) & 0xFF)
        elif escaped == "x":
            out.append(int(literal[at + 1:at + 3], 16))
            at += 3
        else:
            out.append({"n": 10, "t": 9, "r": 13}.get(escaped, ord(escaped)))
            at += 1
    return bytes(out)


def read_source(path):
    """The face's u8g2 name and its packed bytes."""
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    named = re.search(r"Fontname: (\S+)", text)
    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', text.split("=", 1)[1])
    data = bytearray()
    for literal in literals:
        data += unescape(literal)
    return (named.group(1) if named else Path(path).name), bytes(data)


class Bits:
    """u8g2's bit reader: least-significant bit first within each byte, as u8g2_font.c has it."""

    def __init__(self, data):
        self.data = data
        self.at = 0

    def unsigned(self, count):
        value = 0
        for index in range(count):
            byte = self.data[self.at // 8]
            value |= ((byte >> (self.at % 8)) & 1) << index
            self.at += 1
        return value

    def signed(self, count):
        if count == 0:
            return 0
        return self.unsigned(count) - (1 << (count - 1))


def signed_byte(value):
    return value - 256 if value > 127 else value


def decode_glyph(data, at, header):
    """One glyph's size, offsets, advance and pixels, from the byte its fields start at."""
    bits = Bits(data[at:])
    width = bits.unsigned(header["bits_w"])
    height = bits.unsigned(header["bits_h"])
    x = bits.signed(header["bits_x"])
    y = bits.signed(header["bits_y"])
    advance = bits.signed(header["bits_d"])

    rows = [[0] * width for _ in range(height)]
    total = width * height
    if total > 0:
        # A run of zeros then a run of ones, repeated while the bit after them is set; a new pair is
        # read when it is not, and the glyph ends once the pixels have run past its last row.
        at = 0
        while at // width < height:
            zeros = bits.unsigned(header["m0"])
            ones = bits.unsigned(header["m1"])
            while True:
                at += zeros
                for _ in range(ones):
                    if at // width < height:
                        rows[at // width][at % width] = 1
                    at += 1
                if bits.unsigned(1) == 0:
                    break
    return {"width": width, "height": height, "x": x, "y": y, "advance": advance, "rows": rows}


def read_face(path):
    """Every glyph the face holds below U+0100, and the header facts the Font struct needs."""
    name, data = read_source(path)
    header = {
        "count": data[OFFSET_GLYPHS],
        "m0": data[OFFSET_M0],
        "m1": data[OFFSET_M1],
        "bits_w": data[OFFSET_BITS_W],
        "bits_h": data[OFFSET_BITS_H],
        "bits_x": data[OFFSET_BITS_X],
        "bits_y": data[OFFSET_BITS_Y],
        "bits_d": data[OFFSET_BITS_D],
        "ascent": data[OFFSET_ASCENT],
        "descent": signed_byte(data[OFFSET_DESCENT]),
    }
    glyphs = {}
    at = HEADER_SIZE
    while True:
        jump = data[at + 1]
        if jump == 0:
            break  # the entry with no jump ends the table rather than naming a glyph
        glyphs[data[at]] = decode_glyph(data, at + 2, header)
        at += jump
    return name, header, glyphs


def pack(rows):
    """A glyph's rows as bytes, most-significant bit first and padded to a whole byte."""
    out = bytearray()
    bits = 0
    count = 0
    for row in rows:
        for pixel in row:
            bits = (bits << 1) | pixel
            count += 1
            if count == 8:
                out.append(bits)
                bits = 0
                count = 0
    if count:
        out.append(bits << (8 - count))
    return bytes(out)


def byte_literals(data):
    lines = []
    line = ""
    for byte in data:
        piece = "0x%02X, " % byte
        if len("    " + line + piece) > 96:
            lines.append("    " + line.rstrip())
            line = ""
        line += piece
    if line:
        lines.append("    " + line.rstrip().rstrip(","))
    return "\n".join(lines)


def write_face(symbol, first, last, glyphs, header, note):
    advance = next(iter(glyphs.values()))["advance"] if glyphs else 6
    blank = {"width": 0, "height": 0, "x": 0, "y": 0, "advance": advance, "rows": []}

    bitmap = bytearray()
    entries = []
    for code in range(first, last + 1):
        entry = glyphs.get(code, blank)
        entries.append("    {%u, %u, %u, %u, %d, %d}," % (
            len(bitmap), entry["width"], entry["height"], entry["advance"], entry["x"],
            -(entry["height"] + entry["y"])))
        bitmap += pack(entry["rows"])

    declaration = textwrap.dedent("""\
        #pragma once

        #include "Font.h"

        namespace gardener::gfx {{

        // {note}
        extern const Font {symbol};

        }}  // namespace gardener::gfx
        """).format(symbol=symbol, note=note)

    definition = textwrap.dedent("""\
        #include "fonts/{symbol}.h"

        #include <Arduino.h>

        namespace gardener::gfx {{
        namespace {{

        // {note}
        const uint8_t {symbol}Bits[] PROGMEM = {{
        {bitmap}
        }};

        const Glyph {symbol}Glyphs[] PROGMEM = {{
        {table}
        }};

        }}  // namespace

        // A variable face: each glyph carries its own ink and advance, so the face holds the glyph
        // table, the codes it covers and the rows above and below the baseline, and no cell of its
        // own.
        const Font {symbol} PROGMEM = {{Font::Kind::Variable, {symbol}Bits, 0, 0, {symbol}Glyphs,
                                      0x{first:02X}, 0x{last:02X}, {ascent}, {descent}}};

        }}  // namespace gardener::gfx
        """).format(symbol=symbol, note=note, bitmap=byte_literals(bitmap),
                    table="\n".join(entries), first=first, last=last,
                    ascent=header["ascent"], descent=-header["descent"])

    (FONTS / (symbol + ".h")).write_text(declaration, encoding="utf-8", newline="\r\n")
    (FONTS / (symbol + ".cpp")).write_text(definition, encoding="utf-8", newline="\r\n")
    return len(bitmap), len(entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="a u8g2 single-font .c file, e.g. u8g2_font_t0_16_tf.c")
    parser.add_argument("symbol", help="the C++ name for the face, e.g. Ttyp016")
    parser.add_argument("--first", type=lambda text: int(text, 0), default=0x20,
                        help="the first code the face answers (default 0x20)")
    parser.add_argument("--last", type=lambda text: int(text, 0), default=0xFF,
                        help="the last code the face answers (default 0xFF)")
    args = parser.parse_args()

    name, header, glyphs = read_face(args.source)
    note = ("UW ttyp0 %s: the %u codes of 0x%02X..0x%02X of %s, converted by\n"
            "// tools/fonts/from_u8g2.py; the glyph designs are Uwe Waldmann's, so LICENSE-ttyp0.txt\n"
            "// travels with this header. Its source font:\n"
            "// %s"
            % (args.symbol, args.last - args.first + 1, args.first, args.last,
               Path(args.source).name, name))
    bitmap_bytes, entries = write_face(args.symbol, args.first, args.last, glyphs, header, note)
    print("%s: %u glyphs in the source, %u codes written, %u bytes of bitmap, %u entries"
          % (args.symbol, len(glyphs), args.last - args.first + 1, bitmap_bytes, entries))


if __name__ == "__main__":
    main()
