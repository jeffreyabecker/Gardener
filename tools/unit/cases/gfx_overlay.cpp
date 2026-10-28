// The band across the bottom of the glass: what it does with the cells it is given — an equal share
// each, an erased canvas under them, and a cell left alone when it is given nothing.

#include <doctest.h>

#include <string>

#include "suite.h"

namespace gardener::gfx::test {
namespace {

using namespace gardener::gfx::ui;

// A cell that fills whatever share it is handed, so a case reads the share from the ink it leaves.
Paint filled() {
  return [](Canvas& cell) {
    cell.draw(gfx::shapes::FillRect(0, 0, cell.width(), cell.height(), Ink));
  };
}

TEST_CASE("a band draws its cells in equal shares") {
  reset();
  Canvas own = canvas.child(Rect(0, 0, PageWidth, BandHeight));
  drawBand(own, {filled(), filled(), filled(), filled()});
  const int quarter = (PageWidth / 4) * BandHeight;
  CHECK_EQ(display.inkCount(0, 0, PageWidth / 4, BandHeight), quarter);
  CHECK_EQ(display.inkCount(PageWidth / 4, 0, PageWidth / 4, BandHeight), quarter);
  CHECK_EQ(display.inkCount(PageWidth / 2, 0, PageWidth / 4, BandHeight), quarter);
  CHECK_EQ(display.inkCount(PageWidth * 3 / 4, 0, PageWidth / 4, BandHeight), quarter);
}

TEST_CASE("a band clears what was under it") {
  reset();
  canvas.draw(gfx::shapes::FillRect(0, 0, PageWidth, BandHeight, Ink));
  Canvas own = canvas.child(Rect(0, 0, PageWidth, BandHeight));
  drawBand(own, {});
  CHECK_EQ(display.inkCount(0, 0, PageWidth, BandHeight), 0);
}

TEST_CASE("a band gives the last cell what is left") {
  reset();
  constexpr int16_t narrow = 125;
  Canvas own = canvas.child(Rect(0, 0, narrow, BandHeight));
  drawBand(own, {filled(), filled(), filled()});
  CHECK_EQ(display.inkCount(0, 0, narrow, BandHeight), narrow * BandHeight);
}

TEST_CASE("a band leaves a cell given nothing alone") {
  reset();
  Canvas own = canvas.child(Rect(0, 0, PageWidth, BandHeight));
  drawBand(own, {Paint(), filled()});
  CHECK_EQ(display.inkCount(0, 0, PageWidth / 2, BandHeight), 0);
  CHECK_EQ(display.inkCount(PageWidth / 2, 0, PageWidth / 2, BandHeight), (PageWidth / 2) * BandHeight);
}

}  // namespace

}  // namespace gardener::gfx::test
