// The drawing functions: a string, a glyph, a bar, a scrollbar and a duration, driven through the
// canvas each answers and read back as pixels.

#include <math.h>

#include <doctest.h>

#include <string>
#include <vector>

#include "suite.h"

#include "core/SystemState.h"
#include "fonts/Ttyp011.h"
#include "fonts/Ttyp016.h"
#include "fonts/Ttyp022.h"
#include "panel/page/PressureReadout.h"

namespace gardener::gfx::test {
namespace {

// A paint that inks the whole canvas it is given, which is what a glyph does and what a case reads
// back as a filled box.
PaintNodeRef fill() {
  return callable(
      [](Canvas& own) { own.draw(Rectangle(0, 0, own.width(), own.height(), Ink, true)); });
}

TEST_CASE("a text measures the built in face") {
  const Size ink = measureText(nullptr, "AB", 1);
  CHECK_EQ(ink.width, 12);
  CHECK_EQ(ink.height, 8);
}

TEST_CASE("a text scales the face it draws") {
  const Size ink = measureText(nullptr, "AB", 2);
  CHECK_EQ(ink.width, 24);
  CHECK_EQ(ink.height, 16);
}

TEST_CASE("the Adafruit family is the one the library ships and the one a name answers to") {
  // A draw or a measurement that names no family, and a page that names "adafruit", all mean this
  // one: the design the panel drew in before there was another.
  CHECK_EQ(defaultFamily(), findFamily("adafruit"));
  CHECK_EQ(familyOr(nullptr), findFamily("adafruit"));
  CHECK_EQ(findFamily(nullptr), findFamily("adafruit"));
  CHECK_EQ(findFamily(""), findFamily("adafruit"));
  CHECK_EQ(defaultFamily()->count, 1);
  CHECK_EQ(defaultFamily()->cuts[0], &AdafruitFont);

  // Setting it is what a composition does to change the look, and nullptr puts the Adafruit family
  // back.
  setDefaultFamily(findFamily("ttyp0"));
  CHECK_EQ(defaultFamily(), findFamily("ttyp0"));
  CHECK_EQ(find(nullptr), &Ttyp011);
  setDefaultFamily(nullptr);
  CHECK_EQ(defaultFamily(), findFamily("adafruit"));
  CHECK_EQ(find(nullptr), &AdafruitFont);

  // A name the module does not hold draws in the default family rather than raising.
  CHECK_EQ(findFamily("sans12"), defaultFamily());
  CHECK_EQ(find("sans12"), defaultFamily()->cuts[0]);
}

TEST_CASE("a text draws at its box") {
  reset();
  text(Box(10, 10, 6, 8), "I")->draw(canvas);
  CHECK(display.anyInk(10, 10, 6, 8));
  CHECK(!display.anyInk(0, 0, 10, 8));
}

TEST_CASE("a text past its canvas is cut") {
  reset();
  View narrow = canvas.child(Rect(0, 0, 6, 8));
  text(Box(0, 0, 6, 8), "AB")->draw(narrow);
  CHECK(display.anyInk(0, 0, 6, 8));
  CHECK_EQ(display.inkCount(6, 0, 6, 8), 0);
}

TEST_CASE("a string is centred in the box it is given") {
  reset();
  View band = canvas.child(Rect(0, 0, 100, 20));
  // Twenty rows hold two lines of the Adafruit face, so the letter is drawn at two scales: twelve
  // pixels of ink across in a hundred, from the forty-fourth column, and sixteen rows of the
  // twenty, two down.
  text(Box::whole(), "A")->draw(band);
  CHECK_EQ(display.inkCount(0, 0, 44, 20), 0);
  CHECK(display.anyInk(44, 2, 12, 16));
  CHECK_EQ(display.inkCount(56, 0, 44, 20), 0);
}

TEST_CASE("an icon draws in its box") {
  reset();
  icon(Box(30, 20, 12, 8), fill())->draw(canvas);
  CHECK_EQ(display.inkCount(30, 20, 12, 8), 12 * 8);
  CHECK(!display.anyInk(0, 0, 30, 20));
}

TEST_CASE("an icon from hex digits draws the bitmap they name") {
  reset();
  // Three pixels of ink on the first row of a three-by-two bitmap, and none on the second.
  icon(Box(10, 10, 3, 2), "0302e000")->draw(canvas);
  CHECK_EQ(display.inkCount(10, 10, 3, 1), 3);
  CHECK_EQ(display.inkCount(10, 11, 3, 1), 0);
}

TEST_CASE("an icon from a string that names no bitmap draws nothing") {
  reset();
  icon(Box(10, 10, 12, 8), "not a glyph")->draw(canvas);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, PageHeight), 0);
}

TEST_CASE("a progress bar draws at its ends") {
  reset();
  progressBar(Box(0, 0, 20, 8), 0.0f)->draw(canvas);
  CHECK_EQ(display.inkCount(1, 1, 18, 6), 0);

  reset();
  progressBar(Box(0, 0, 20, 8), 100.0f)->draw(canvas);
  CHECK_EQ(display.inkCount(1, 1, 18, 6), 18 * 6);

  reset();
  progressBar(Box(0, 0, 20, 8), 50.0f)->draw(canvas);
  CHECK_EQ(display.inkCount(1, 1, 9, 6), 9 * 6);
  CHECK_EQ(display.inkCount(10, 1, 9, 6), 0);
}

TEST_CASE("a progress bar holds its value to its ends") {
  reset();
  progressBar(Box(0, 0, 20, 8), 180.0f)->draw(canvas);
  CHECK_EQ(display.inkCount(1, 1, 18, 6), 18 * 6);

  reset();
  progressBar(Box(0, 0, 20, 8), -40.0f)->draw(canvas);
  CHECK_EQ(display.inkCount(1, 1, 18, 6), 0);
}

TEST_CASE("a scrollbar draws its thumb") {
  reset();
  scrollbar(Box(120, 0, 6, 20), 0, 40, 10)->draw(canvas);
  CHECK(display.anyInk(120, 0, 6, 20));
}

TEST_CASE("a scrollbar with no rows draws nothing") {
  reset();
  scrollbar(Box(120, 0, 6, 20), 0, 0, 10)->draw(canvas);
  CHECK_EQ(display.inkCount(118, 0, 10, 20), 0);
}

TEST_CASE("a duration formats each unit") {
  char out[16];
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 270, true, out, sizeof(out))),
           std::string("4m30s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 300, true, out, sizeof(out))),
           std::string("5m"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 45, true, out, sizeof(out))),
           std::string("45s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Seconds, 90, true, out, sizeof(out))),
           std::string("90s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Hours, 3723, true, out, sizeof(out))),
           std::string("1h2m3s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Hours, 7260, true, out, sizeof(out))),
           std::string("2h1m"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 0, true, out, sizeof(out))),
           std::string("0s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 0, false, out, sizeof(out))),
           std::string("--"));
}

TEST_CASE("a duration draws its text") {
  reset();
  duration(Box(0, 0, 40, 8), DurationFormat::Minutes, 270)->draw(canvas);
  CHECK(display.anyInk(0, 0, 40, 8));

  reset();
  duration(Box(0, 0, 40, 8), DurationFormat::Minutes, 0, nullptr, false)->draw(canvas);
  CHECK(display.anyInk(0, 0, 40, 8));
}

TEST_CASE("an inverted paint flips the whole cell it was given") {
  reset();
  View band = canvas.child(Rect(0, 0, 20, 8));
  inverted(callable([](Canvas& own) { own.draw(Rectangle(0, 0, 10, 8, Ink, true)); }))->draw(band);
  CHECK_EQ(display.inkCount(0, 0, 10, 8), 0);
  CHECK_EQ(display.inkCount(10, 0, 10, 8), 10 * 8);
}

TEST_CASE("an inverted paint with nothing to draw draws nothing") {
  reset();
  const PaintNodeRef nothing = inverted(PaintNodeRef());
  CHECK(!nothing);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, PageHeight), 0);
}

TEST_CASE("an underlay clears the cell the child draws in") {
  reset();
  canvas.draw(Rectangle(0, 0, 20, 8, Ink, true));
  PaintNodeRef child = callable([](Canvas& own) { own.draw(Rectangle(0, 0, 10, 8, Ink, true)); });
  View band = canvas.child(Rect(0, 0, 20, 8));
  underlay(std::move(child))->draw(band);
  CHECK_EQ(display.inkCount(0, 0, 10, 8), 10 * 8);
  CHECK_EQ(display.inkCount(10, 0, 10, 8), 0);
}

TEST_CASE("an underlay with nothing to draw clears nothing") {
  reset();
  canvas.draw(Rectangle(0, 0, 20, 8, Ink, true));
  const PaintNodeRef nothing = underlay(PaintNodeRef());
  CHECK(!nothing);
  CHECK_EQ(display.inkCount(0, 0, 20, 8), 20 * 8);
}

TEST_CASE("a triangle factory inscribes the arrow in its rectangle") {
  const Rect box(10, 20, 9, 5);  // 10..18 across, 20..24 down
  const Triangle up = Triangle::up(box, Ink, true);
  CHECK_EQ(up.x0, 14);  // the apex, on the middle column
  CHECK_EQ(up.y0, 20);
  CHECK_EQ(up.x1, 10);
  CHECK_EQ(up.y1, 24);  // the base, along the bottom
  CHECK_EQ(up.x2, 18);
  CHECK_EQ(up.y2, 24);
  CHECK(up.filled);

  const Triangle down = Triangle::down(box, Ink);
  CHECK_EQ(down.x0, 10);
  CHECK_EQ(down.y0, 20);
  CHECK_EQ(down.x1, 18);
  CHECK_EQ(down.y1, 20);
  CHECK_EQ(down.x2, 14);
  CHECK_EQ(down.y2, 24);
  CHECK_FALSE(down.filled);

  const Triangle left = Triangle::left(box, Ink, true);
  CHECK_EQ(left.x0, 10);  // the apex, on the middle row
  CHECK_EQ(left.y0, 22);
  CHECK_EQ(left.x1, 18);
  CHECK_EQ(left.y1, 20);
  CHECK_EQ(left.x2, 18);
  CHECK_EQ(left.y2, 24);

  const Triangle right = Triangle::right(box, Ink, true);
  CHECK_EQ(right.x0, 18);
  CHECK_EQ(right.y0, 22);
  CHECK_EQ(right.x1, 10);
  CHECK_EQ(right.y1, 20);
  CHECK_EQ(right.x2, 10);
  CHECK_EQ(right.y2, 24);
}

TEST_CASE("a triangle factory draws in its rectangle and nowhere else") {
  reset();
  canvas.draw(Triangle::up(Rect(10, 10, 9, 6), Ink, true));
  CHECK(display.anyInk(14, 10, 1, 1));   // the apex, on the middle column
  CHECK(!display.anyInk(10, 10, 1, 1));  // the corner beside it is outside the arrow
  CHECK(display.anyInk(10, 15, 1, 1));   // the base, corner to corner
  CHECK(display.anyInk(18, 15, 1, 1));
  CHECK(!display.anyInk(10, 16, 9, 1));  // nothing below the box
  CHECK_EQ(display.inkCount(0, 0, PageWidth, PageHeight), display.inkCount(10, 10, 9, 6));
}

TEST_CASE("the faces the library ships answer to their names") {
  // The ttyp0 faces are registered as the library loads, so a page names one and is given it back.
  CHECK_EQ(find("ttyp011"), &Ttyp011);
  CHECK_EQ(find("ttyp016"), &Ttyp016);
  CHECK_EQ(find("ttyp022"), &Ttyp022);
  CHECK_NE(find("ttyp016"), faceOr(nullptr));

  // The Adafruit is a fixed face, drawn straight from glcdfont.c's packing, and the ttyp0 faces are
  // tables of glyphs.
  CHECK(AdafruitFont.kind == Font::Kind::Fixed);
  CHECK(Ttyp016.kind == Font::Kind::Variable);

  // A name the library does not hold is still a draw: it falls back rather than raising.
  CHECK_EQ(find("ttyp012"), faceOr(nullptr));
}

TEST_CASE("a face says its own line box") {
  // The rows above the baseline and on and below it, which is a face's height without a string to
  // measure: the fixed face's last row sits on the baseline, and the ttyp0 faces declare one row of
  // descent over the rows their line is.
  CHECK_EQ(AdafruitFont.ascent, 7);
  CHECK_EQ(AdafruitFont.descent, 1);
  CHECK_EQ(lineHeight(AdafruitFont), 8);
  CHECK_EQ(lineHeight(Ttyp011), 10);
  CHECK_EQ(lineHeight(Ttyp016), 13);
  CHECK_EQ(lineHeight(Ttyp022), 17);
}

TEST_CASE("a fixed face draws the cell it declares rather than the Adafruit's") {
  // Two columns by three rows, one byte to a column with the least significant bit the top row: an
  // 'A' that inks the top and bottom row of its first column is 0x05 there, and the second column
  // is the blank one the advance leaves.
  std::vector<uint8_t> bits(256 * 2, 0x00);
  bits['A' * 2] = 0x05;
  const Font face = {Font::Kind::Fixed, bits.data(), 2, 3};

  // The face's cell, not the classic six-by-eight: two columns and a blank, three rows.
  const Size ink = measureText(&face, "AA", 1);
  CHECK_EQ(ink.width, 6);
  CHECK_EQ(ink.height, 3);

  reset();
  const Font* cuts[] = {&face};
  const Family family = {"probe", cuts, 1};
  text(Box(0, 0, 128, 64), "AA", &family, Justify::Start, Justify::Start)->draw(canvas);
  CHECK_EQ(display.inkCount(0, 0, 1, 3), 2);
  CHECK_EQ(display.inkCount(1, 0, 1, 3), 0);
  CHECK_EQ(display.inkCount(3, 0, 1, 3), 2);
  CHECK_EQ(display.inkCount(0, 0, 128, 64), 4);
}

TEST_CASE("a ttyp0 face measures and draws the glyph it was converted from") {
  // The ink of an 'A' in the three sizes, as the u8g2 glyphs measured it.
  const Size small = measureText(&Ttyp011, "A", 1);
  CHECK_EQ(small.width, 4);
  CHECK_EQ(small.height, 8);
  const Size middle = measureText(&Ttyp016, "A", 1);
  CHECK_EQ(middle.width, 6);
  CHECK_EQ(middle.height, 10);
  const Size large = measureText(&Ttyp022, "A", 1);
  CHECK_EQ(large.width, 10);
  CHECK_EQ(large.height, 13);

  // Drawn, it is an 'A' where the glyph says it is: an apex two pixels wide on the top row of the
  // ink, the crossbar across the sixth, a clear column for its bearing, and nothing past the last
  // row. The ink starts on the second row of the box, because the face's ascent is twelve rows and
  // the letter is ten tall.
  reset();
  text(Box(0, 0, 128, 13), "A", findFamily("ttyp016"), Justify::Start, Justify::Start)
      ->draw(canvas);
  CHECK_EQ(display.inkCount(3, 2, 2, 1), 2);
  CHECK_EQ(display.inkCount(1, 8, 6, 1), 6);
  CHECK_EQ(display.inkCount(0, 0, 1, 12), 0);
  CHECK_EQ(display.inkCount(0, 12, 128, 52), 0);
  CHECK_EQ(display.inkCount(0, 0, 128, 64), 24);
}

TEST_CASE("a face puts every string of it on one baseline") {
  // The baseline is the face's ascent and not the height of the string being drawn: a letter and a
  // letter with a descender start on the same row of their boxes, and only the descender inks below
  // it. Measured from the string, the pair would sit three rows lower than the letter alone.
  reset();
  text(Box(0, 0, 40, 16), "A", findFamily("ttyp016"), Justify::Start, Justify::Start)
      ->draw(canvas);
  text(Box(40, 0, 40, 16), "Ag", findFamily("ttyp016"), Justify::Start, Justify::Start)
      ->draw(canvas);
  CHECK_EQ(display.inkCount(0, 0, 80, 2), 0);
  CHECK(display.anyInk(0, 2, 8, 1));
  CHECK(display.anyInk(40, 2, 40, 1));
  CHECK_EQ(display.inkCount(0, 12, 40, 4), 0);
  CHECK(display.anyInk(40, 12, 40, 4));
}

// A readout to draw, in the unit and at the values a case expects on the glass.
SystemState readoutState(float readingKpa, float targetKpa) {
  SystemState state;
  state.running = true;
  state.currentPressureKpa = readingKpa;
  state.targetPressureKpa = targetKpa;
  state.unit = DisplayUnit::Kpa;
  return state;
}

TEST_CASE("a readout gives the reading the top of its box and the target the rest") {
  // The box is divided in three fifths and two: the reading fills the upper eighteen rows of a
  // thirty row band at whatever cut fits them, and the target the twelve below, its line box against
  // the end of its part, so no pixel of one lands in the other's rows.
  const SystemState state = readoutState(-101.3f, 45.6f);
  reset();
  View band = canvas.child(Rect(0, 0, 128, 30));
  pressureReadout(Box::whole(), state)->draw(band);

  // Six characters of the Adafruit face at two scales are 72 pixels of ink, centred in the hundred
  // and twenty-eight across and sixteen of the eighteen rows down, one row in.
  CHECK_EQ(display.inkCount(0, 0, 128, 1), 0);
  CHECK_EQ(display.inkCount(0, 1, 128, 16), display.inkCount(28, 1, 72, 16));
  // The target is four characters at one scale: 24 pixels of ink, centred across, in the last eight
  // rows of the twelve under the reading.
  CHECK_EQ(display.inkCount(0, 17, 128, 5), 0);
  CHECK_EQ(display.inkCount(0, 22, 128, 8), display.inkCount(52, 22, 24, 8));
  CHECK_EQ(display.inkCount(0, 30, 128, 34), 0);
}

TEST_CASE("a family is a design's cuts, shortest first") {
  // A page names the design and a height picks the cut, so the cuts are the design's own sizes in
  // the order the shortest of them comes first.
  const Family* ttyp0 = findFamily("ttyp0");
  REQUIRE(ttyp0 != nullptr);
  CHECK_EQ(ttyp0->count, 3);
  CHECK_EQ(ttyp0->cuts[0], &Ttyp011);
  CHECK_EQ(ttyp0->cuts[1], &Ttyp016);
  CHECK_EQ(ttyp0->cuts[2], &Ttyp022);

  // The Adafruit face is a family of one cut, and it is the answer a name the module does not hold
  // and a page that names none are both given.
  CHECK_EQ(findFamily("adafruit")->count, 1);
  CHECK_EQ(findFamily("adafruit")->cuts[0], &AdafruitFont);
  CHECK_EQ(findFamily(nullptr), findFamily("adafruit"));
  CHECK_EQ(findFamily(""), findFamily("adafruit"));
  CHECK_EQ(findFamily("sans"), findFamily("adafruit"));
}

}  // namespace

}  // namespace gardener::gfx::test
