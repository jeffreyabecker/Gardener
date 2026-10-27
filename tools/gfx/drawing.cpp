// The drawing functions: a string, a glyph, a bar, a scrollbar and a duration, driven through the
// canvas each answers and read back as pixels.

#include <math.h>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// A paint that inks the whole canvas it is given, which is what a glyph does and what a case reads
// back as a filled box.
Paint fill() {
  return [](Canvas& own) { own.draw(shapes::FillRect(0, 0, own.width(), own.height(), Ink)); };
}

void aTextMeasuresTheBuiltInFace() {
  const shapes::TextBox ink = shapes::measureText(nullptr, "AB", 1);
  expectEq(ink.width, 12, "two built-in glyphs are twelve pixels across");
  expectEq(ink.height, 8, "the built-in face is eight pixels tall");
}

void aTextScalesTheFaceItDraws() {
  const shapes::TextBox ink = shapes::measureText(nullptr, "AB", 2);
  expectEq(ink.width, 24, "two glyphs at scale two are twenty-four across");
  expectEq(ink.height, 16, "the built-in face at two is sixteen tall");
}

void aTextMeasuresACompiledFace() {
  const shapes::TextBox ink = shapes::measureText(fonts::Sans12, "A", 1);
  expect(ink.width > 0, "a compiled face measures a width");
  expect(ink.height > 8, "a compiled face at twelve points is taller than the built-in one");
}

void aTextDrawsAtItsBox() {
  reset();
  text(Rect(10, 10, 6, 8), "I")(canvas);
  expect(display.anyInk(10, 10, 6, 8), "the glyph landed inside the box");
  expect(!display.anyInk(0, 0, 10, 8), "nothing was drawn left of the box");
}

void aTextPastItsCanvasIsCut() {
  reset();
  Canvas narrow = canvas.child(Rect(0, 0, 6, 8));
  text(Rect(0, 0, 6, 8), "AB")(narrow);
  expect(display.anyInk(0, 0, 6, 8), "the first glyph is drawn");
  expectEq(display.inkCount(6, 0, 6, 8), 0, "the rest is cut at the canvas's edge");
}

void aStringIsCentredOnItsCanvas() {
  reset();
  Canvas band = canvas.child(Rect(0, 0, 100, 20));
  drawCentred(band, "A");
  const shapes::TextBox ink = shapes::measureText(nullptr, "A", 1);
  const int16_t x = static_cast<int16_t>((100 - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((20 - ink.height) / 2);
  expect(display.anyInk(x, y, ink.width, ink.height), "the string landed centred");
  expectEq(display.inkCount(0, 0, 100, y), 0, "and nothing was drawn above it");

  reset();
  drawCentred(band, "A", nullptr, 2);
  const shapes::TextBox large = shapes::measureText(nullptr, "A", 2);
  const int16_t largeX = static_cast<int16_t>((100 - large.width) / 2);
  const int16_t largeY = static_cast<int16_t>((20 - large.height) / 2);
  expect(display.anyInk(largeX, largeY, large.width, large.height),
         "the scale it is given is the scale it centres at");
}

void anIconDrawsInItsBox() {
  reset();
  icon(Rect(30, 20, 12, 8), fill())(canvas);
  expectEq(display.inkCount(30, 20, 12, 8), 12 * 8, "the glyph inked the whole of its box");
  expect(!display.anyInk(0, 0, 30, 20), "nothing was drawn outside it");
}

void aProgressBarDrawsAtItsEnds() {
  reset();
  progressBar(Rect(0, 0, 20, 8), 0.0f)(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 0, "a bar at zero inks none of its inside");

  reset();
  progressBar(Rect(0, 0, 20, 8), 100.0f)(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 18 * 6, "a bar at a hundred inks all of it");

  reset();
  progressBar(Rect(0, 0, 20, 8), 50.0f)(canvas);
  expectEq(display.inkCount(1, 1, 9, 6), 9 * 6, "a bar at half inks the left half");
  expectEq(display.inkCount(10, 1, 9, 6), 0, "and none of the right");
}

void aProgressBarHoldsItsValueToItsEnds() {
  reset();
  progressBar(Rect(0, 0, 20, 8), 180.0f)(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 18 * 6, "a value past a hundred is held to it");

  reset();
  progressBar(Rect(0, 0, 20, 8), -40.0f)(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 0, "a value below zero is held to it");
}

void aScrollbarDrawsItsThumb() {
  reset();
  scrollbar(Rect(120, 0, 6, 20), 0, 40, 10)(canvas);
  expect(display.anyInk(120, 0, 6, 20), "the bar drew its track and thumb");
}

void aScrollbarWithNoRowsDrawsNothing() {
  reset();
  scrollbar(Rect(120, 0, 6, 20), 0, 0, 10)(canvas);
  expectEq(display.inkCount(118, 0, 10, 20), 0, "a bar with no rows draws nothing");
}

void aDurationFormatsEachUnit() {
  char out[16];
  expectText(durationText(DurationFormat::Minutes, 270, true, out, sizeof(out)), "4m30s",
             "minutes and seconds");
  expectText(durationText(DurationFormat::Minutes, 300, true, out, sizeof(out)), "5m",
             "whole minutes drop the seconds");
  expectText(durationText(DurationFormat::Minutes, 45, true, out, sizeof(out)), "45s",
             "under a minute is seconds");
  expectText(durationText(DurationFormat::Seconds, 90, true, out, sizeof(out)), "90s",
             "the seconds format counts past a minute");
  expectText(durationText(DurationFormat::Hours, 3723, true, out, sizeof(out)), "1h2m3s",
             "hours keep the parts that are there");
  expectText(durationText(DurationFormat::Hours, 7260, true, out, sizeof(out)), "2h1m",
             "an even minute drops the seconds");
  expectText(durationText(DurationFormat::Minutes, 0, true, out, sizeof(out)), "0s",
             "nothing is zero seconds");
  expectText(durationText(DurationFormat::Minutes, 0, false, out, sizeof(out)), "--",
             "an unknown count is dashes");
}

void aDurationDrawsItsText() {
  reset();
  duration(Rect(0, 0, 40, 8), DurationFormat::Minutes, 270)(canvas);
  expect(display.anyInk(0, 0, 40, 8), "the duration drew its text");

  reset();
  duration(Rect(0, 0, 40, 8), DurationFormat::Minutes, 0, nullptr, false)(canvas);
  expect(display.anyInk(0, 0, 40, 8), "an unknown duration drew its dashes");
}

}  // namespace

void addDrawingCases(std::vector<Case>& cases) {
  cases.push_back({"a text measures the built-in face", aTextMeasuresTheBuiltInFace});
  cases.push_back({"a text scales the face it draws", aTextScalesTheFaceItDraws});
  cases.push_back({"a text measures a compiled face", aTextMeasuresACompiledFace});
  cases.push_back({"a text draws at its box", aTextDrawsAtItsBox});
  cases.push_back({"a text past its canvas is cut", aTextPastItsCanvasIsCut});
  cases.push_back({"a string is centred on its canvas", aStringIsCentredOnItsCanvas});
  cases.push_back({"an icon draws in its box", anIconDrawsInItsBox});
  cases.push_back({"a progress bar draws at its ends", aProgressBarDrawsAtItsEnds});
  cases.push_back(
      {"a progress bar holds its value to its ends", aProgressBarHoldsItsValueToItsEnds});
  cases.push_back({"a scrollbar draws its thumb", aScrollbarDrawsItsThumb});
  cases.push_back({"a scrollbar with no rows draws nothing", aScrollbarWithNoRowsDrawsNothing});
  cases.push_back({"a duration formats each unit", aDurationFormatsEachUnit});
  cases.push_back({"a duration draws its text", aDurationDrawsItsText});
}

}  // namespace gardener::gfx::test
