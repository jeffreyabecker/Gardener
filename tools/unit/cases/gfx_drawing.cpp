// The drawing functions: a string, a glyph, a bar, a scrollbar and a duration, driven through the
// canvas each answers and read back as pixels.

#include <math.h>

#include <doctest.h>

#include <string>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

// A paint that inks the whole canvas it is given, which is what a glyph does and what a case reads
// back as a filled box.
PaintNodeRef fill() {
  return callable(
      [](Canvas& own) { own.draw(shapes::Rect(0, 0, own.width(), own.height(), Ink, true)); });
}

TEST_CASE("a text measures the built in face") {
  const Size ink = shapes::measureText(nullptr, "AB", 1);
  CHECK_EQ(ink.width, 12);
  CHECK_EQ(ink.height, 8);
}

TEST_CASE("a text scales the face it draws") {
  const Size ink = shapes::measureText(nullptr, "AB", 2);
  CHECK_EQ(ink.width, 24);
  CHECK_EQ(ink.height, 16);
}

TEST_CASE("a text measures a compiled face") {
  const Size ink = shapes::measureText(fonts::Sans12, "A", 1);
  CHECK(ink.width > 0);
  CHECK(ink.height > 8);
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
  const Size ink = shapes::measureText(nullptr, "A", 1);
  const int16_t x = static_cast<int16_t>((100 - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((20 - ink.height) / 2);
  aligned(sized(borrowedText(Box::whole(), "A"), ink))->draw(band);
  CHECK(display.anyInk(x, y, ink.width, ink.height));
  CHECK_EQ(display.inkCount(0, 0, 100, y), 0);

  reset();
  const Size large = shapes::measureText(nullptr, "A", 2);
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
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 270, true, out, sizeof(out))), std::string("4m30s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 300, true, out, sizeof(out))), std::string("5m"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 45, true, out, sizeof(out))), std::string("45s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Seconds, 90, true, out, sizeof(out))), std::string("90s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Hours, 3723, true, out, sizeof(out))), std::string("1h2m3s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Hours, 7260, true, out, sizeof(out))), std::string("2h1m"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 0, true, out, sizeof(out))), std::string("0s"));
  CHECK_EQ(std::string(durationText(DurationFormat::Minutes, 0, false, out, sizeof(out))), std::string("--"));
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
  inverted(callable([](Canvas& own) { own.draw(shapes::Rect(0, 0, 10, 8, Ink, true)); }))
      ->draw(band);
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
  canvas.draw(shapes::Rect(0, 0, 20, 8, Ink, true));
  PaintNodeRef child = callable(
      [](Canvas& own) { own.draw(shapes::Rect(0, 0, 10, 8, Ink, true)); });
  View band = canvas.child(Rect(0, 0, 20, 8));
  underlay(std::move(child))->draw(band);
  CHECK_EQ(display.inkCount(0, 0, 10, 8), 10 * 8);
  CHECK_EQ(display.inkCount(10, 0, 10, 8), 0);
}

TEST_CASE("an underlay with nothing to draw clears nothing") {
  reset();
  canvas.draw(shapes::Rect(0, 0, 20, 8, Ink, true));
  const PaintNodeRef nothing = underlay(PaintNodeRef());
  CHECK(!nothing);
  CHECK_EQ(display.inkCount(0, 0, 20, 8), 20 * 8);
}

}  // namespace

}  // namespace gardener::gfx::test
