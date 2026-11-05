# Telling the two panels apart

Two 128 × 64 controllers are common enough that a module can arrive as either, and they are not
interchangeable: an SH1106 has 132 columns of RAM and no address windows, an SSD1306 has 128 columns
and three addressing modes. A driver written for one puts a recognisably wrong picture on the other —
shifted by two columns, or wrapped at the wrong edge.

The question this answers is whether a driver can find out which part it is talking to. It cannot,
not by asking: nothing either part will send back distinguishes them. What can be done is to send a
command only one of them acts on and watch the glass, and the sections below are the probes and what
each outcome means. The maps themselves are [sh1106-i2c.md](./sh1106-i2c.md) and
[ssd1306-i2c.md](./ssd1306-i2c.md).

## Why nothing can be read

There is no identity register in either part, and neither can have its display RAM read back over
I2C. The SH1106 says so under its read-display-data command: no display data can be read via the
serial interface. The SSD1306 says it under its serial-mode note: no data read is provided in serial
mode operation. The RAM a driver has just written is therefore unreadable, which removes the obvious
test — write a pattern to a column only one part has and read it back.

Both parts also answer the same address, `0x3C` or `0x3D` on SA0, so a bus scan finds one device
either way.

That leaves the status register, and it happens to coincide. Read as one byte after an address byte
with the read bit set (SH1106 page 28 of the datasheet, SSD1306 Table 9-2):

| | bit 7 | bit 6 | bit 5 | bit 4 | bit 3 | bit 2 | bit 1 | bit 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SH1106 | busy | display off | — | — | — | 0 | 0 | 0 |
| SSD1306 | 0 | display off | — | — | — | — | — | — |

Both report `0x00` with the display on and idle, and `0x40` with it off. The SH1106's busy bit is set
only while the part is working and is clear again by the time a read is answered, so it marks no
lasting difference, and the reserved bits read as zero on both. Many modules also put a one-way level
shifter in front of the part, so a read may not come back at all — but a read that does come back
still says nothing.

## What does differ

| | SSD1306 | SH1106 |
| --- | --- | --- |
| RAM | 128 columns | 132 columns |
| Glass at RAM column | 0 | 2 on the common 1.3" module, 0 on some |
| Address windows | `0x21`, `0x22` | none, and `0x20`–`0x2F` is undefined |
| Scroll engine | `0x26`/`0x27`/`0x29`/`0x2A`, `0x2E`/`0x2F`, `0xA3` | none |
| Panel supply | charge pump, `0x8D 0x14` | DC-DC, `0xAD 0x8B`, voltage at `0x30`–`0x33` |

Every one of these differences is visible on the glass and none of them is readable on the bus. An
identification is therefore a probe, an observer, and a conclusion — the observer being whoever is
looking at the panel, a camera, or nobody, in which case the part stays unknown.

## The supply probe

The panel supply commands are disjoint: each part acts on one of them and treats the other's as an
undefined byte, so the pair is a straight either-or. With a frame drawn and the display on:

1. Send `0x8D 0x10`, the SSD1306's charge-pump-off command, and look. An SSD1306 loses its panel
   supply and goes dark; an SH1106 treats `0x8D` as undefined and `0x10` as a column high nibble, and
   does not change. `0x8D 0x14` restores the SSD1306.
2. Then send `0xAD 0x8A`, the SH1106's DC-DC-off command, and look. An SH1106 loses the panel supply
   and goes dark; an SSD1306 treats both bytes as undefined. `0xAD 0x8B` restores it.

Whichever command darkens the panel names the part. Nothing is left changed either way, though a
part that stayed dark after the restore is re-initialised rather than guessed at.

Two caveats. A module that feeds the panel's `VCC` from the header rather than from the charge pump
does not darken on `0x8D 0x10`, and an SH1106 module built around an external `VPP` does not darken
on `0xAD 0x8A`; both answers then read as "no change", and the geometry probe decides. The SH1106
datasheet also asks for the DC-DC command to be issued with the display off, which this probe
deliberately does not do — it is a bring-up test on a module whose behaviour is in doubt, not
something to run on a panel that is already known and working.

## The geometry probe

This one is harmless: it writes pixels and leaves nothing switched off. Put the driver in page
addressing, select page 0 and RAM column 0 — `0xB0`, `0x00`, `0x10` — and write one `0xFF` byte
behind a `0x40` control byte.

- A lit column at the very left edge means RAM column 0 is on the glass, which is an SSD1306's
  geometry.
- Nothing at the edge, with the pixel column appearing only when the same byte is written at RAM
  column 2 (`0x02`, `0x10`), means a 132-column part whose glass starts at column 2 — an SH1106.

The probe identifies the column offset more exactly than it identifies the part: an SH1106 with the
zero offset the datasheet allows shows the byte at the edge exactly as an SSD1306 does. It is the
confirmation for the supply probe, and on its own it proves an SH1106 only in the shifted case.

## Past the end of the window

The probe above writes to whichever RAM column it chooses, so it is worth knowing what an address
outside the window does. Three things are meant by that, and only the first two are defined.

A column that exists in RAM but is not bonded to glass takes a write and never shows it. That is
SH1106 columns 0, 1, 130 and 131, and it is why the two-column offset costs nothing: the byte at RAM
column 0 sits in the hidden margin while the picture starts at column 2.

A write that runs past the end of the window wraps. The SSD1306 documents each case:

| Mode | At the end of the column range |
| --- | --- |
| page | column back to the start column, page unchanged |
| horizontal | column back to the start column, page + 1; both at their starts after the last page |
| vertical | page back to the start page, column + 1; both at their starts after the last column |

An over-long row therefore overwrites its own beginning rather than spilling into the next page. The
SH1106 says only that the column counter increments "until address 132 is accessed", so 132 bytes
fill a row and what a 133rd would do is not described.

A start address outside the range the part documents — above 127 on the SSD1306, above 131 on the
SH1106 — is undefined. The SH1106 warns that using any other command risks a system malfunction, and
on that part the address lives in the instruction's own low nibble, so `0x19`–`0x1F` asks for
columns 144 and up, which do not exist. Whether such a byte is dropped, aliased modulo 128 or 132, or
lands somewhere else is not promised anywhere, and a driver that depends on it will work on one batch
and not the next.

An SSD1306 driven at the SH1106's offset of 2 writes 128 columns from column 2 and ends at 129, two
columns past the 128 the part admits, on every page. A wrong offset is therefore not only a picture
shifted by two: it is that shift with an out-of-range write once per page.

## The window probe

The last resort, because it leaves the picture wrong until it is undone. Put the part in horizontal
addressing with `0x20 0x00` and an SSD1306 takes `0x21` as the column window; an SH1106 has no
`0x20`–`0x2F` command at all, so the same bytes are decoded as strays, a column address and a
start-line address. `0x21 0x02 0x7F` therefore sets an SSD1306's column window to 2–127 and an
SH1106's start line to 63, which throws its picture 63 rows up and wraps the top of the frame to the
bottom.

A frame that jumps 63 rows is an SH1106; a frame that is unchanged is an SSD1306. `0x40` restores the
start line, and a redraw puts the frame back.

## What a driver should do

Since the answer needs an observer, no driver can detect the part on a bare boot and a wrong guess
shows up as a picture two columns out rather than as a failure. The panel therefore belongs in
configuration — a setting with a default — and the probes above belong in a bring-up screen that
draws a known frame, sends one command, and asks which of two descriptions matches what is on the
glass. The machine's own panel is the 1.3" SH1106 of [the parts list](../guides/poc-parts.md), so the
setting's default is that part.
