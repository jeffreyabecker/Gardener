# Which panel for a green-field AliExpress build?

The fitted panel is a $7 1.3" 128x64 SH1106, two wires on I2C, no backlight, one kilobyte of frame
buffer and a contrast no colour part matches. It was bought as the cheapest thing that let the panel
code be written while the enclosure was still unknown. There is no alpha board and no enclosure yet,
so nothing commits us to it: the panel for a 100-300 unit run bought on AliExpress can be chosen
fresh, on product and supply grounds, rather than inherited from the proof of concept.

Why it is worth settling now: the layout work is already shaped by the size. The readout is two
64-pixel columns, and a five-character value at the 24 px face nearly fills one, which is why
[the bindings spec](../specs/ui/bindings.md) prints the unit in the bottom band instead of beside the
number. With no board yet the panel sets the board, not the other way round, so the decision is
cheapest to make before the carrier and the enclosure are drawn.

## What the part actually constrains

The panel feels small in pixels, but what a reader feels is millimetres: a glyph's height on the
glass is its size in pixels times the pixel pitch.

| Part | Controller | Active area (mm) | Pitch (mm) | 24 px glyph | Rows over a 12-row band | Interface | AliExpress ≈100–300 (non-promo) | Supply |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0.96" mono 128x64 | SSD1306 | 21.7 x 10.9 | 0.17 | 4.1 mm | 52 | I2C or SPI | $1.80–2.50 | very wide |
| 1.3" mono 128x64 (fitted) | SH1106 | 29.4 x 14.7 | 0.23 | 5.5 mm | 52 | I2C | $2.50–3.50 | very wide |
| 1.5" mono 128x64 | SSD1309 | 35.0 x 17.5 | 0.27 | 6.6 mm | 52 | I2C or SPI | $3.00–4.50 | medium |
| 2.42" mono 128x64 | SSD1309 | 55.0 x 27.5 | 0.43 | 10.3 mm | 52 | SPI, 7-pin | $5.00–8.00 | narrow |
| 1.5" grey 128x128 | SSD1327 | 26.9 x 26.9 | 0.21 | 5.0 mm | 116 | SPI, 4-wire | $6.00–9.00 | narrow |
| 1.8" colour 128x160 | ST7735 | 28.0 x 35.0 | 0.22 | 5.3 mm | 148 | SPI | $2.00–3.00 | wide |
| 2.0" colour 240x320 | ST7789 | 40.8 x 30.6 | 0.13 | 3.1 mm | 228 | SPI | $3.00–4.50 | very wide |
| 2.4" colour 240x320 | ST7789 | 49.0 x 36.7 | 0.15 | 3.7 mm | 228 | SPI | $3.50–5.00 | very wide |
| 2.9" e-paper 296x128 | SSD1680 | 66.9 x 29.0 | 0.23 | 5.5 mm | 116 | SPI | $6.00–10.00 | medium |

Three things fall out. Every colour panel under three inches makes a 24 px glyph *smaller* than the
panel already fitted, so a 240x320 part only helps if the text is allowed to shrink or the face is
scaled up, and scaling up eats the extra rows. The only part that doubles legibility without touching
the code is the same 128x64 image on bigger glass — pixels are not what is missing; millimetres are.
And the AliExpress reality is that monochrome on I2C ends at 1.5 inches: the 2.42" mono ships in
seven-pin SPI, so "bigger, same contract, still two wires" is not an option that exists there.
Prices in the table are non-promotional unit prices at 100–300 pieces, not the front-page figure: a
$1 0.96" or 1.3" OLED is a Welcome Deal, a Choice Day price or a first-order discount, and the two
cheapest mono rows are where the promo and the real price part ways the furthest. The landed number
is taken up below.

## The options

**Keep the 1.3" SH1106.** The commodity mono panel on AliExpress: every seller lists it, I2C, about
20 mA, two signal pins, 25 ms to flush 1 KB at 400 kHz with the frame at 4 Hz. It is already at its
limit: the settings list shows four or five rows, the route readout four lines, and the unit cannot
sit beside the number. Keeping it means accepting that those numbers stay where they are, but it is
the one choice with no supply risk and no code change.

**1.5" monochrome 128x64 (SSD1309).** The I2C ceiling. Same one-bit contract, same buffer, roughly
20% more glyph height and a dollar or so more than the 1.3". Only a driver name and two constants
move.
Its pitch is coarser (0.27 mm), so it is genuinely easier to read rather than merely bigger. Supply
is medium: many sellers list it, fewer than the 1.3", but the controller is a drop-in cousin.

**2.42" monochrome 128x64 (SSD1309, SPI).** The biggest glyph of any panel considered, double the
1.3" at arm's length. On AliExpress this is a seven-pin SPI module; the I2C version is rare, more
expensive, and usually the same board with the mode resistors set. Supply is narrow — a handful of
sellers — and the outline sets the enclosure at roughly 68 x 37 mm. Same one-bit image, so the code
change stays a driver name and two constants, but it drags SPI in with it.

**1.5" grayscale 128x128 (SSD1327).** Doubles the rows for a settings list or a taller graph, but its
pitch is finer, so a 24 px glyph is *smaller* than the fitted panel. It also breaks a contract rather
than changing a part: [the display's own surface](../../libraries/Gardener/src/hal/Display.h) is ink,
paper and a rectangle of pixels that flips, [the asset pipeline](../specs/assets.md) cuts an image to
one bit, and the components spec is written in those terms. Sixteen greys would be pleasant for a
graph and expensive everywhere else, and the part is niche.

**1.8–2.4" colour TFT (ST7735 / ST7789, SPI).** The supply answer. The ST7789 240x320 IPS is the
single most-listed small display on AliExpress — dozens of sellers, stable pricing, a part that will
still exist in a year — and the 1.8" ST7735 sits just behind it. The cost is the drawing surface:
the panel contract is that drawing lands in the driver's frame buffer and `display()` sends it, which
is 1 KB here and 150 KB at 240x320 — or a rewrite to partial windows and dirty rectangles, which is a
different display with tearing to manage. Colour buys nothing for a number, a bubble grid and a bar
graph, and it makes the asset pipeline and the spec's ink-and-paper language untrue. It wants five or
six pins and a backlight, and it never burns in.

**E-paper, 2.13" or 2.9".** The dark horse for a bench instrument. Near-zero idle current, no
backlight, sunlight-readable, never burns in, and the face persists with the power off — a vacuum
controller spends most of its life showing one settled pressure. The catch is the live readout: a
full refresh is a second or more, partial refresh is a fraction of a second with ghosting, and the
UI redraws four times a second. A slow pressure, fine; a ramping routine, not.

**A round 1.28" 240x240 (GC9A01).** Ruled out by the layout: the readout is two columns over a
full-width band, which is the one thing a circle cannot hold.

**Bare glass instead of a module.** AliExpress sells both: a module is the glass on a breakout with a
regulator and a header, and bare glass is a flex tail onto a ZIF connector. Bare glass saves under a
dollar a unit and the module's carrier, but it moves the bezel, the connector and the mounting into
the enclosure. At 300 units that is only worth it once the enclosure owns the cut-out anyway.

## Sourcing on AliExpress at 100–300

The panel stops being a part and becomes a supply decision, and five things change.

**Money stops being the axis, once promo pricing is set aside.** AliExpress lists a 1.3" OLED at
$1 and charges $3 for it; the $1 is a Welcome Deal, a Choice Day price or a first-order discount,
not a quantity quote. At 100–300 pieces the fitted 1.3" is roughly $2.50–3.50, the 1.5" mono
$3–4.50, the 2.0" 240x320 IPS $3–4.50. At 200 units the step from the 1.3" to either of the others
is about $100–200 — a fraction of one percent of a $200 instrument. The screen is not where the
unit's cost lives, which means price cannot be the deciding axis. The figure that actually matters
is the landed one — freight, and whatever a parcel of 300 attracts at the border — which is a quote
asked for in a chat window rather than a cart quantity.

**A listing is not a part number.** The real cost of AliExpress is not the price, it is that a search
for "1.3 inch OLED I2C" returns modules that differ in pin order, in controller — an SH1106 needs a
column offset an SSD1306 does not — in board outline and in hole spacing. Two consequences. Buy the
whole run in one order from one lot, plus ten percent spares and two panels kept back as golden
samples, because the risk sits between orders rather than inside one. And make the panel a plug-in
subassembly — a header or a JST-SH connector with the pinout on the silkscreen — so a substituted
module is a cable or a solder bridge rather than a board respin.

**Commodity beats niche.** The strongest signal on AliExpress is how many sellers list a part. The
SSD1306/SH1106 128x64 and the ST7789 240x320 have dozens of listings each and will still be there in
a year; the 2.42" mono and the 1.5" greyscale have a handful and can vanish between orders. A
300-unit run is covered by one lot either way, but a warranty replacement, a second run or a respin
needs the part to still be listed. Choosing a niche panel is choosing a future respin; choosing a
commodity one is insurance that costs nothing.

**Choose an interface, not a hedge.** With no board yet there is nothing to hedge against: the
interface is still a free choice, so make it rather than papering over it. The division is clean —
I2C for a monochrome 1.3" or 1.5" panel, SPI for anything bigger, because that is how the bigger
panels ship. A seven-pin header (3V3, GND, SDA, SCL, then CS, DC, RST) is a hedge for a board that
has to serve both; with no board, pick the interface the chosen panel wants and design for it.

**A static white OLED ages.** A shipped instrument sits on one face for years: the band, the border,
the same digits. Lit pixels dim, so the face that never moves is the face that fades, and the
warranty arrives as a patchy screen. The mitigations are cheap but have to be decisions: dim after a
minute, blank after ten, or a face that is mostly paper. An IPS or e-paper panel does not burn in,
which is the one production argument for them that has nothing to do with colour.

## What would have to be true

For the 1.5" mono: that a 20% legibility gain settles it, and that the I2C ceiling is the right place
to stop — both plausible, and the cheapest comfortable reading.

For the 2.42" mono: that the instrument is read at arm's length or further, where 5.5 mm glyphs are
marginal and 10.3 mm are comfortable; that an enclosure face of roughly 68 x 37 mm is acceptable; and
that accepting SPI is fine, because the I2C version is rare and dear on AliExpress.

For a 2.0" ST7789: that the product wants more rows than any mono panel offers — a longer settings
list, a taller graph — or that burn-in matters more than the rewrite of the drawing surface. That
rewrite is the price, and it is a real one.

For e-paper: that the instrument's face is mostly static — a settled pressure read, a status screen —
and that the live ramping readout is allowed to be slow. It is not a drop-in; it is a different kind
of face.

## What is still unknown

Whether the finished machine is a bench instrument or a handheld, and so what the viewing distance
is — the one fact every other choice leans on. Whether four settings rows and four route lines are
enough once a routine with more settings exists. Whether the enclosure is an off-the-shelf box,
laser-cut acrylic or a mould, which decides how much a 68 x 37 mm face costs. Whether a listing that
says "2.42 I2C" actually ships in I2C, or is the same seven-pin board with a moved resistor. What any
of these panels lands at after freight and duty at 300 pieces, and whether the same seller still has
the part in a year — both questions a sample order and a chat with the seller answer better than the
listing does.

## Where I land

Strong on monochrome. The one-bit contract is one kilobyte of buffer, no backlight, no inverter, no
dimming and a driver that is a template argument; colour would cost the drawing surface, the asset
pipeline and three specs to show the same number in a colour nobody asked for. But AliExpress
weakens the "monochrome gets bigger" path: the big mono panels are SPI and niche, so growing the
mono screen means giving up the two-wire commodity and the wide supply at once.

Fairly strong on leaving the 1.3". It was the right panel for proving the loop and it is the wrong
panel for a product sold by the hundred: the money it saves is a couple of percent of the unit, and
the thing it costs is the first thing a buyer looks at.

The fork is between two directions, not a ranking. If the instrument stays a small bench box read
close up, the 1.5" SSD1309 is the sensible end of monochrome: one driver name and two constants,
still I2C, still commodity-adjacent, a little bigger. If the product wants room to grow — more rows,
a taller graph, no burn-in — the answer is the 2.0" ST7789, the most-listed small display on the
platform, and the price is the drawing-surface rewrite. The 2.42" mono sits between them and mostly
loses to both: it wants the SPI of the ST7789 and the narrow supply of the niche panels, for a screen
that is still 64 rows.

E-paper is the one I have not ruled out: it is the right face for a settled bench instrument and the
wrong one for a live ramping readout, and only the product knows which of those it is.

Buy a sample of each — a 1.5" SSD1309, a 2.42" SSD1309, a 2.0" ST7789 IPS and a 2.9" e-paper — and
look at them across a bench at the distance the instrument will actually sit. About $30 against a
run whose tooling and assembly dwarf it, and the only way to answer the question the table cannot.
