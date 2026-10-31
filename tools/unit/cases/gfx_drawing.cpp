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

TEST_CASE("the built in face is the one the library ships and the one a name answers to") {
  // A draw or a measurement that names no face, and a page that names "builtin", all mean this one.
  CHECK_EQ(defaultFace(), &BuiltinFont);
  CHECK_EQ(faceOr(nullptr), &BuiltinFont);
  CHECK_EQ(find(nullptr), &BuiltinFont);
  CHECK_EQ(find(""), &BuiltinFont);
  CHECK_EQ(find("builtin"), &BuiltinFont);

  // Naming it is what a caller does to say so, and nullptr puts the built-in back.
  setDefaultFace(&BuiltinFont);
  CHECK_EQ(defaultFace(), &BuiltinFont);
  setDefaultFace(nullptr);
  CHECK_EQ(defaultFace(), &BuiltinFont);
  CHECK_EQ(faceOr(nullptr), &BuiltinFont);

  // A name the module does not hold draws in the default face rather than raising.
  CHECK_EQ(find("sans12"), defaultFace());
}

// A face with one glyph: a filled five-by-three box of ink, advancing six, drawn from the baseline.
const uint8_t ProbeBits[] = {0xF8, 0xF8, 0xF8};
const Glyph ProbeGlyphs[] = {{0, 5, 3, 6, 0, 0}};
const Font ProbeFace = {Font::Kind::Variable, ProbeBits, 0, 0, ProbeGlyphs, 'A', 'A', 8};

TEST_CASE("a registered face comes out of find and measures as its own face") {
  REQUIRE(registerFont("probe", &ProbeFace));

  const Font* found = find("probe");
  CHECK_EQ(found, &ProbeFace);
  // What the face says it is, not the built-in's six-by-eight cell.
  const Size ink = measureText(found, "A", 1);
  CHECK_EQ(ink.width, 5);
  CHECK_EQ(ink.height, 3);

  // One name is one face: the built-in's name cannot be taken, nor one that is already an answer.
  CHECK_FALSE(registerFont("builtin", &ProbeFace));
  CHECK_FALSE(registerFont("probe", &BuiltinFont));
  CHECK_FALSE(registerFont("", &ProbeFace));
  CHECK_FALSE(registerFont("probe", nullptr));

  // A name it does not hold is a face to fall back to, so a page naming one still draws.
  CHECK_EQ(find("missing"), defaultFace());
}

TEST_CASE("a registered name longer than an entry is refused") {
  CHECK_FALSE(registerFont("a name too long to hold", &ProbeFace));
  CHECK_EQ(find("a name too long to hold"), defaultFace());
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

TEST_CASE("a string is centred on its canvas") {
  reset();
  View band = canvas.child(Rect(0, 0, 100, 20));
  const Size ink = measureText(nullptr, "A", 1);
  const int16_t x = static_cast<int16_t>((100 - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((20 - ink.height) / 2);
  aligned(sized(borrowedText(Box::whole(), "A"), ink))->draw(band);
  CHECK(display.anyInk(x, y, ink.width, ink.height));
  CHECK_EQ(display.inkCount(0, 0, 100, y), 0);

  reset();
  const Size large = measureText(nullptr, "A", 2);
  const int16_t largeX = static_cast<int16_t>((100 - large.width) / 2);
  const int16_t largeY = static_cast<int16_t>((20 - large.height) / 2);
  aligned(sized(borrowedText(Box::whole(), "A", nullptr, 2), large))->draw(band);
  CHECK(display.anyInk(largeX, largeY, large.width, large.height));
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
  CHECK_NE(find("ttyp016"), defaultFace());

  // The built-in is a fixed face, drawn straight from glcdfont.c's packing, and the ttyp0 faces are
  // tables of glyphs.
  CHECK(BuiltinFont.kind == Font::Kind::Fixed);
  CHECK(Ttyp016.kind == Font::Kind::Variable);

  // A name the library does not hold is still a draw: it falls back rather than raising.
  CHECK_EQ(find("ttyp012"), defaultFace());
}

TEST_CASE("a fixed face draws the cell it declares rather than the built-in's") {
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
  text(Box(0, 0, 128, 64), "AA", &face, 1)->draw(canvas);
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

  // Drawn, it is an 'A' where the glyph says it is: an apex two pixels wide on the top row, the
  // crossbar across the sixth, a clear column for its bearing, and nothing past the last row.
  reset();
  text(Box(0, 0, 128, 64), "A", &Ttyp016, 1)->draw(canvas);
  CHECK_EQ(display.inkCount(3, 0, 2, 1), 2);
  CHECK_EQ(display.inkCount(1, 6, 6, 1), 6);
  CHECK_EQ(display.inkCount(0, 0, 1, 10), 0);
  CHECK_EQ(display.inkCount(0, 10, 128, 54), 0);
  CHECK_EQ(display.inkCount(0, 0, 128, 64), 24);
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

TEST_CASE("a readout stacks a reading too wide for its glass over the target") {
  // Six characters at scale two is 72 pixels of ink and four at scale one is 24, so the two cannot
  // sit side by side in 128: the reading is given the top of a column and the target the end of the
  // cell under it, and no pixel of one lands in the other's rows.
  const SystemState state = readoutState(-101.3f, 45.6f);
  reset();
  View band = canvas.child(Rect(0, 0, 128, 30));
  pressureReadout(Box::whole(), state, 0.5f)->draw(band);

  // The reading is centred across the top cell and the target flush to the end of the lower one,
  // the pair centred down the 30 rows it was given: three clear above, sixteen, eight, three clear.
  CHECK_EQ(display.inkCount(0, 0, 128, 3), 0);
  CHECK_EQ(display.inkCount(0, 3, 128, 16), display.inkCount(28, 3, 72, 16));
  CHECK_EQ(display.inkCount(0, 19, 128, 8), display.inkCount(104, 19, 24, 8));
  CHECK_EQ(display.inkCount(0, 27, 128, 3), 0);
}

TEST_CASE("a readout puts the target beside the reading when there is room") {
  // At a size hint of one both readings are drawn at the main scale, in the two halves of a row:
  // nothing is drawn across the seam between them.
  const SystemState state = readoutState(-12.3f, 45.6f);
  reset();
  View band = canvas.child(Rect(0, 0, 128, 32));
  pressureReadout(Box::whole(), state, 1.0f)->draw(band);

  // Five characters at scale two are 60 pixels of ink, centred in the first half and eight rows
  // down; four are 48, centred in the second, so no pixel of one is drawn in the other's half.
  CHECK_EQ(display.inkCount(0, 0, 64, 32), display.inkCount(2, 8, 60, 16));
  CHECK_EQ(display.inkCount(64, 0, 64, 32), display.inkCount(72, 8, 48, 16));
}

}  // namespace

}  // namespace gardener::gfx::test
