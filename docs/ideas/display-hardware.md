# Is the 1.3" monochrome OLED the panel to design around?

The panel is a $7 1.3" 128x64 SH1106, two wires on I2C, no backlight, one kilobyte of frame buffer and
a contrast no colour part matches. It was bought as the cheapest thing that let the panel code be
written while the enclosure was still unknown. The question is whether it is the part the finished
machine should be drawn around, or a placeholder that has quietly become a constraint.

Why it is worth settling now: the layout work is already shaped by the size. The readout is two
64-pixel columns, and a five-character value at the 24 px face nearly fills one, which is why
[the bindings spec](../specs/ui/bindings.md) prints the unit in the bottom band instead of beside the
number. The panel's outline also sets the face of the alpha board, so the choice stops being
reversible once that board is laid out.

## What the part actually constrains

The panel feels small in pixels, but what a reader feels is millimetres: a glyph's height on the
glass is its size in pixels times the pixel pitch.

| Part | Active area (mm) | Pitch (mm) | A 24 px glyph | Rows above a 12-row band | One-off price |
| --- | --- | --- | --- | --- | --- |
| 1.3" mono 128x64 (fitted) | 29.4 x 14.7 | 0.23 | 5.5 mm | 52 | $7 |
| 2.42" mono 128x64 | 55.0 x 27.5 | 0.43 | 10.3 mm | 52 | $10-14 |
| 1.5" grey 128x128 | 26.9 x 26.9 | 0.21 | 5.0 mm | 116 | $12-18 |
| 1.8" colour 128x160 | 28.0 x 35.0 | 0.22 | 5.3 mm | 148 | $4-6 |
| 2.0" colour 320x240 | 40.8 x 30.6 | 0.13 | 3.1 mm | 228 | $9-13 |
| 2.4" colour 320x240 | 49.0 x 36.7 | 0.15 | 3.7 mm | 228 | $12-16 |
| 2.8" colour 320x240 | 57.6 x 43.2 | 0.18 | 4.3 mm | 228 | $14-20 |

Two things fall out. Every colour panel under three inches makes a 24 px glyph *smaller* than the
panel already fitted, so a 320x240 part only helps if the text is allowed to shrink or the face is
scaled up, and scaling up eats the extra rows. And the only part that doubles legibility without
touching the code is the same 128x64 image on bigger glass — pixels are not what is missing;
millimetres are.

## The options

**Keep the 1.3".** Cheapest, smallest, about 20 mA, two signal pins, 25 ms to flush 1 KB at 400 kHz
with the frame at 4 Hz. It is already at its limit: the settings list shows four or five rows, the
route readout four lines, and the unit cannot sit beside the number. Keeping it means accepting that
those numbers stay where they are.

**2.42" monochrome 128x64 (SSD1309/SSD1305, I2C or SPI).** Same one-bit contract, same Adafruit GFX
verbs, same driver-buffer flush, roughly double the glyph height for no code change beyond the driver
type in the composition root and two geometry constants. It costs an outline that sets the enclosure
at roughly 68 x 37 mm, about 50 mA instead of 20, and the module's own quirks (Adafruit's asks for
220 uF; the level shifter they ship is for 5 V hosts and a 3.3 V Super Mini does not need it). Power
does not favour it — a 2.0" IPS is also 40-60 mA — so on a 1S pack this is the difference between
roughly 100 hours and 40, not between a good and a bad choice.

**1.5" grayscale 128x128 (SSD1327).** Doubles the rows for a settings list or a taller graph, but its
pitch is finer, so a 24 px glyph is *smaller* than today. It also breaks a contract rather than
changing a part: [the display's own surface](../../libraries/PanelLink/src/hal/Display.h) is ink,
paper and a rectangle of pixels that flips, [the asset pipeline](../specs/assets.md) cuts an image to
one bit, and the components spec is written in those terms. Sixteen greys would be pleasant for a
graph and expensive everywhere else.

**2.0-2.8" colour TFT (ST7789/ILI9341, SPI).** The most pixels for the money and the wrong shape of
change. The panel contract is that drawing lands in the driver's frame buffer and `display()` sends
it, which is 1 KB here and 150 KB at 320x240 — or a rewrite to partial windows and dirty rectangles,
which is a different display surface with tearing to manage. Colour itself buys nothing for a number,
a bubble grid and a bar graph, and it would make the asset pipeline and the spec's ink-and-paper
language untrue. It also wants five or six pins and a backlight.

The cheaper cousin is a 1.8" 128x160 ST7735 at about $5: today's legibility on 148 rows instead of
52, a 40 KB buffer instead of 150, and the same change to the drawing surface.

**A round 1.28" 240x240.** Ruled out by the layout: the readout is two columns over a full-width band,
which is the one thing a circle cannot hold.

**Bare glass on the alpha board.** Orthogonal — whichever module wins sets the cut-out — and it is
the reason the decision should not wait for the board.

## At a hundred to three hundred units

The panel stops being a part and becomes a supply decision, and four things change.

**Money stops being the axis.** From AliExpress at 100–300 pieces a 1.3" mono module is roughly
$2.50–4, a 2.42" mono roughly $6–9, a 2.0" 320x240 IPS roughly $4.50–7. At 200 units the step from
the fitted panel to the 2.42" is about $700 — $3.50 a unit, one or two percent of a $200 instrument.
That is less than the enclosure or the assembly time, and less still than the returns from a screen a
buyer judges cheap. The figure that matters is the landed one — freight, and whatever a parcel of 300
attracts at the border — which is a quote asked for in a chat window rather than a cart quantity.

**A listing is not a part number.** That is the real cost of buying a run this way, not the price. A
search for "1.3 inch OLED I2C" returns modules that differ in pin order, in controller — an SH1106
needs a column offset an SSD1306 does not — in board outline and in hole spacing; and a 2.42" often
ships in SPI mode with two resistors to move before I2C works, which is 200 hand reworks. Two
consequences. Buy the whole run in one order from one lot, plus ten percent spares and two panels
kept back as golden samples, because the risk sits between orders rather than inside one. And make
the panel a plug-in subassembly — a header or a four-pin JST-SH with the pinout on the silkscreen —
so a substituted module is a cable or a solder bridge rather than a board respin.

**The connector is the cheap hedge.** The four-pin panel header on the alpha board could be seven:
3V3, GND, SDA and SCL first, then CS, DC and RST on the pins a colour module wants. An I2C module
uses the first four and ignores the rest; an SPI module uses all seven. Same header, same money, and
it keeps the decisions that are expensive to reverse — mono or colour, 128x64 or 320x240 — open until
the enclosure exists. The spare GPIOs are already there.

**A static white OLED ages.** A shipped instrument sits on one face for years: the band, the border,
the same digits. Lit pixels dim, so the face that never moves is the face that fades, and the
warranty arrives as a patchy screen. The mitigations are cheap but have to be decisions: dim after a
minute, blank after ten, or a face that is mostly paper. An IPS panel ages evenly and does not burn
in, which is the one production argument for colour that has nothing to do with colour.

## What would have to be true

For the 2.42": that the instrument is read at arm's length or further, where 5.5 mm glyphs are
marginal and 10.3 mm are comfortable; that an enclosure face of roughly 68 x 37 mm is acceptable; and
that the 25 ms flush and 50 mA are both fine, which the 4 Hz frame rate and the bench supply say they
are. None of that is true of the 1.3" today only because nobody has said what the finished thing is.

## What is still unknown

Whether the finished machine is a bench instrument or a handheld, and so what the viewing distance
is. Whether four settings rows and four route lines are enough once a routine with more settings
exists. Whether the enclosure is an off-the-shelf box, laser-cut acrylic or a mould, which decides
how much a 68 x 37 mm face costs. What a 2.42" module actually costs with a four-pin I2C header,
whether it is one lot at that price, and what the shipment lands at after freight and duty. How a
particular AliExpress seller behaves on a 300-piece order, which is a question a sample order
answers better than a listing does.

## Where I land

Strong on monochrome. At production volume the one-bit contract is one kilobyte of buffer, no
backlight, no inverter, no dimming and a driver that is a template argument; colour would cost the
drawing surface, the asset pipeline and three specs to show the same number in a colour nobody asked
for. If the panel does get bigger, monochrome gets bigger with it for a driver name and two
constants.

Stronger than before on changing the glass. The 1.3" is the right panel for proving the loop and the
wrong panel for a product sold by the hundred: the money it saves is a couple of percent of the unit,
and the thing it costs is the first thing a buyer looks at. The 2.42" doubles legibility with the
same one-bit image. The 2.0" IPS buys rows and no burn-in, and would have to earn the display
surface it costs.

Moderate on the interface, and that is the open question. If the panel is ever larger than 128x64 it
will be SPI, because that is how those panels ship, and the buffered-flush contract is written for a
one-kilobyte part. The seven-pin header keeps that open for nothing.

Buy three panels — a 2.42" mono, a 1.8" ST7735 and a 2.0" ST7789 IPS — while the board is still a
breadboard, look at them across a bench, and keep one of each as the golden sample. About $25 against
a run whose tooling and assembly dwarf it, and the only way to answer the question the table cannot.
