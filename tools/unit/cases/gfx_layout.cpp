// The layout: the cell a layout occupies of its canvas, the cells its children declare, and the
// pixels each child's paint lands in.

#include <doctest.h>

#include <string>

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

TEST_CASE("a column divides the canvas down") {
  reset();
  column(1.0f, {Child{Cell(1.0f, 0.5f), fill()}, Child{Cell(1.0f, 0.5f), fill()}})(canvas);
  CHECK_EQ(display.inkCount(0, 0, 128, 32), 128 * 32);
  CHECK_EQ(display.inkCount(0, 32, 128, 32), 128 * 32);
}

TEST_CASE("equal thirds of a row fill it") {
  reset();
  Canvas base = canvas.child(Rect(0, 0, 96, 12));
  row(1.0f, {Child{Cell(1.0f / 3.0f, 1.0f), fill()}, Child{Cell(1.0f / 3.0f, 1.0f), fill()},
             Child{Cell(1.0f / 3.0f, 1.0f), fill()}})(base);
  CHECK_EQ(display.inkCount(0, 0, 32, 12), 32 * 12);
  CHECK_EQ(display.inkCount(32, 0, 32, 12), 32 * 12);
  CHECK_EQ(display.inkCount(64, 0, 32, 12), 32 * 12);
}

TEST_CASE("a rounded span is clamped to what is left") {
  reset();
  Canvas base = canvas.child(Rect(0, 0, 10, 9));
  column(1.0f, {Child{Cell(1.0f, 0.5f), fill()}, Child{Cell(1.0f, 0.5f), fill()}})(base);
  CHECK_EQ(display.inkCount(0, 0, 10, 5), 10 * 5);
  CHECK_EQ(display.inkCount(0, 5, 10, 4), 10 * 4);
  CHECK_EQ(display.inkCount(0, 9, 10, 1), 0);
}

TEST_CASE("a declared size above one is pixels") {
  reset();
  column(1.0f, {Child{Cell(1.0f, 8.0f), fill()}, Child{Cell(1.0f, 1.0f), fill()}})(canvas);
  CHECK_EQ(display.inkCount(0, 0, 128, 8), 128 * 8);
  CHECK_EQ(display.inkCount(0, 8, 128, 56), 128 * 56);
}

TEST_CASE("a layout takes the alignment it is given") {
  reset();
  Canvas base = canvas.child(Rect(0, 0, 100, 20));
  Alignment centred;
  centred.vertical = Vertical::Center;
  column(1.0f, {Child{Cell(1.0f, 0.2f), fill()}}, centred)(base);
  CHECK_EQ(display.inkCount(0, 0, 100, 8), 0);
  CHECK_EQ(display.inkCount(0, 8, 100, 4), 100 * 4);
  CHECK_EQ(display.inkCount(0, 12, 100, 8), 0);
}

TEST_CASE("a layout draws its children in order") {
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
  CHECK_EQ(std::string(order), std::string("abc"));
}

TEST_CASE("a nested layout lands in its cell") {
  reset();
  row(1.0f, {Child{Cell(0.5f, 1.0f), row(1.0f, {Child{Cell(1.0f, 1.0f), fill()}})}})(canvas);
  CHECK_EQ(display.inkCount(0, 0, 64, PageHeight), 64 * PageHeight);
  CHECK_EQ(display.inkCount(64, 0, 64, PageHeight), 0);
}

TEST_CASE("a cell cuts what paints past it") {
  reset();
  row(1.0f, {Child{Cell(0.5f, 1.0f), spill()}, Child{Cell(0.5f, 1.0f), fill()}})(canvas);
  CHECK_EQ(display.inkCount(0, 0, 64, PageHeight), 64 * PageHeight);
  CHECK_EQ(display.inkCount(64, 0, 64, PageHeight), 64 * PageHeight);
}

TEST_CASE("a layout takes the children it is given at draw time") {
  reset();
  std::vector<Child> rows{Child{Cell(1.0f, 0.5f), fill()}, Child{Cell(1.0f, 0.5f), fill()}};
  column(1.0f, rows)(canvas);
  CHECK_EQ(display.inkCount(0, 0, 128, 32), 128 * 32);
  CHECK_EQ(display.inkCount(0, 32, 128, 32), 128 * 32);
}

TEST_CASE("a row takes the height and a column the width") {
  reset();
  Canvas base = canvas.child(Rect(0, 0, PageWidth, 40));
  row(8.0f, {Child{Cell(1.0f, 1.0f), fill()}})(base);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, 8), PageWidth * 8);
  CHECK_EQ(display.inkCount(0, 8, PageWidth, 32), 0);

  reset();
  column(32.0f, {Child{Cell(1.0f, 1.0f), fill()}})(base);
  CHECK_EQ(display.inkCount(0, 0, 32, 40), 32 * 40);
  CHECK_EQ(display.inkCount(32, 0, PageWidth - 32, 40), 0);
}

}  // namespace

}  // namespace gardener::gfx::test
