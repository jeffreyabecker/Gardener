// The layout: the cell a layout occupies of its canvas, the cells its children declare, and the
// pixels each child's paint lands in.

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// A paint that inks the whole canvas it is given, so a case reads back the cell it landed in.
Paint fill() {
  return [](Canvas& own) { own.draw(shapes::FillRect(0, 0, own.width(), own.height(), Ink)); };
}

// A paint that inks past its canvas, so a case can see the canvas cut it at the cell's edge.
Paint spill() {
  return [](Canvas& own) {
    own.draw(shapes::FillRect(-8, -8, own.width() + 16, own.height() + 16, Ink));
  };
}

void aColumnDividesTheCanvasDown() {
  reset();
  column(1.0f, {Child{Cell(1.0f, 0.5f), fill()}, Child{Cell(1.0f, 0.5f), fill()}})(canvas);
  expectEq(display.inkCount(0, 0, 128, 32), 128 * 32, "the first cell filled the top half");
  expectEq(display.inkCount(0, 32, 128, 32), 128 * 32, "the second cell filled the bottom half");
}

void equalThirdsOfARowFillIt() {
  reset();
  Canvas base = canvas.child(Rect(0, 0, 96, 12));
  row(1.0f, {Child{Cell(1.0f / 3.0f, 1.0f), fill()}, Child{Cell(1.0f / 3.0f, 1.0f), fill()},
             Child{Cell(1.0f / 3.0f, 1.0f), fill()}})(base);
  expectEq(display.inkCount(0, 0, 32, 12), 32 * 12, "a third of ninety-six");
  expectEq(display.inkCount(32, 0, 32, 12), 32 * 12, "the second third");
  expectEq(display.inkCount(64, 0, 32, 12), 32 * 12, "the third reaches the end");
}

void aRoundedSpanIsClampedToWhatIsLeft() {
  reset();
  Canvas base = canvas.child(Rect(0, 0, 10, 9));
  column(1.0f, {Child{Cell(1.0f, 0.5f), fill()}, Child{Cell(1.0f, 0.5f), fill()}})(base);
  expectEq(display.inkCount(0, 0, 10, 5), 10 * 5, "half of an odd height rounds up");
  expectEq(display.inkCount(0, 5, 10, 4), 10 * 4, "and what is left goes to the other");
  expectEq(display.inkCount(0, 9, 10, 1), 0, "nothing is drawn past the canvas");
}

void aDeclaredSizeAboveOneIsPixels() {
  reset();
  column(1.0f, {Child{Cell(1.0f, 8.0f), fill()}, Child{Cell(1.0f, 1.0f), fill()}})(canvas);
  expectEq(display.inkCount(0, 0, 128, 8), 128 * 8, "eight pixels is eight pixels");
  expectEq(display.inkCount(0, 8, 128, 56), 128 * 56, "the leaf takes what is left");
}

void aLayoutTakesTheAlignmentItIsGiven() {
  reset();
  Canvas base = canvas.child(Rect(0, 0, 100, 20));
  Alignment centred;
  centred.vertical = Vertical::Center;
  column(1.0f, {Child{Cell(1.0f, 0.2f), fill()}}, centred)(base);
  expectEq(display.inkCount(0, 0, 100, 8), 0, "the block is not at the top");
  expectEq(display.inkCount(0, 8, 100, 4), 100 * 4, "a fifth of twenty is four, centred");
  expectEq(display.inkCount(0, 12, 100, 8), 0, "and not at the bottom");
}

void aLayoutDrawsItsChildrenInOrder() {
  static char order[8] = {};
  static int count = 0;
  count = 0;
  row(1.0f, {Child{Cell(1.0f, 1.0f),
                  [](Canvas&) {
                    if (count < 8) order[count++] = 'a';
                  }},
             Child{Cell(1.0f, 1.0f),
                  [](Canvas&) {
                    if (count < 8) order[count++] = 'b';
                  }},
             Child{Cell(1.0f, 1.0f), [](Canvas&) {
                    if (count < 8) order[count++] = 'c';
                  }}})(canvas);
  order[count] = '\0';
  expectText(order, "abc", "the children drew in the order they were given");
}

void aNestedLayoutLandsInItsCell() {
  reset();
  row(1.0f, {Child{Cell(0.5f, 1.0f), row(1.0f, {Child{Cell(1.0f, 1.0f), fill()}})}})(canvas);
  expectEq(display.inkCount(0, 0, 64, PageHeight), 64 * PageHeight,
           "the inner row filled its cell");
  expectEq(display.inkCount(64, 0, 64, PageHeight), 0, "and drew nothing beside it");
}

void aCellCutsWhatPaintsPastIt() {
  reset();
  row(1.0f, {Child{Cell(0.5f, 1.0f), spill()}, Child{Cell(0.5f, 1.0f), fill()}})(canvas);
  expectEq(display.inkCount(0, 0, 64, PageHeight), 64 * PageHeight,
           "the spilling cell filled its half");
  expectEq(display.inkCount(64, 0, 64, PageHeight), 64 * PageHeight,
           "and the other half is its own");
}

void aRowTakesTheHeightAndAColumnTheWidth() {
  reset();
  Canvas base = canvas.child(Rect(0, 0, PageWidth, 40));
  row(8.0f, {Child{Cell(1.0f, 1.0f), fill()}})(base);
  expectEq(display.inkCount(0, 0, PageWidth, 8), PageWidth * 8, "a row is the full width");
  expectEq(display.inkCount(0, 8, PageWidth, 32), 0, "and the height it declares");

  reset();
  column(32.0f, {Child{Cell(1.0f, 1.0f), fill()}})(base);
  expectEq(display.inkCount(0, 0, 32, 40), 32 * 40, "a column takes the width it declares");
  expectEq(display.inkCount(32, 0, PageWidth - 32, 40), 0, "and is the full height");
}

}  // namespace

void addLayoutCases(std::vector<Case>& cases) {
  cases.push_back({"a column divides the canvas down", aColumnDividesTheCanvasDown});
  cases.push_back({"equal thirds of a row fill it", equalThirdsOfARowFillIt});
  cases.push_back({"a rounded span is clamped to what is left", aRoundedSpanIsClampedToWhatIsLeft});
  cases.push_back({"a declared size above one is pixels", aDeclaredSizeAboveOneIsPixels});
  cases.push_back({"a layout takes the alignment it is given", aLayoutTakesTheAlignmentItIsGiven});
  cases.push_back({"a layout draws its children in order", aLayoutDrawsItsChildrenInOrder});
  cases.push_back({"a nested layout lands in its cell", aNestedLayoutLandsInItsCell});
  cases.push_back({"a cell cuts what paints past it", aCellCutsWhatPaintsPastIt});
  cases.push_back(
      {"a row takes the height and a column the width", aRowTakesTheHeightAndAColumnTheWidth});
}

}  // namespace gardener::gfx::test
