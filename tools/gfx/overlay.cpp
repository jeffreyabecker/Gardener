// The band across the bottom of the glass: what it does with the cells it is given — an equal share
// each, an erased canvas under them, and a cell left alone when it is given nothing.

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

void aBandDrawsItsCellsInEqualShares() {
  reset();
  Canvas own = canvas.child(Rect(0, 0, PageWidth, BandHeight));
  drawBand(own, {filled(), filled(), filled(), filled()});
  const int quarter = (PageWidth / 4) * BandHeight;
  expectEq(display.inkCount(0, 0, PageWidth / 4, BandHeight), quarter, "the first cell is a quarter");
  expectEq(display.inkCount(PageWidth / 4, 0, PageWidth / 4, BandHeight), quarter,
           "and so is the second");
  expectEq(display.inkCount(PageWidth / 2, 0, PageWidth / 4, BandHeight), quarter,
           "and the third");
  expectEq(display.inkCount(PageWidth * 3 / 4, 0, PageWidth / 4, BandHeight), quarter,
           "and the last");
}

void aBandClearsWhatWasUnderIt() {
  reset();
  canvas.draw(gfx::shapes::FillRect(0, 0, PageWidth, BandHeight, Ink));
  Canvas own = canvas.child(Rect(0, 0, PageWidth, BandHeight));
  drawBand(own, {});
  expectEq(display.inkCount(0, 0, PageWidth, BandHeight), 0, "the band erases the page under it");
}

void aBandGivesTheLastCellWhatIsLeft() {
  reset();
  constexpr int16_t narrow = 125;
  Canvas own = canvas.child(Rect(0, 0, narrow, BandHeight));
  drawBand(own, {filled(), filled(), filled()});
  expectEq(display.inkCount(0, 0, narrow, BandHeight), narrow * BandHeight,
           "the shares add up to the width the band was given");
}

void aBandLeavesACellGivenNothingAlone() {
  reset();
  Canvas own = canvas.child(Rect(0, 0, PageWidth, BandHeight));
  drawBand(own, {Paint(), filled()});
  expectEq(display.inkCount(0, 0, PageWidth / 2, BandHeight), 0,
           "a cell given nothing draws nothing");
  expectEq(display.inkCount(PageWidth / 2, 0, PageWidth / 2, BandHeight),
           (PageWidth / 2) * BandHeight, "and the cell beside it takes the other half");
}

}  // namespace

void addOverlayCases(std::vector<Case>& cases) {
  cases.push_back({"a band draws its cells in equal shares", aBandDrawsItsCellsInEqualShares});
  cases.push_back({"a band clears what was under it", aBandClearsWhatWasUnderIt});
  cases.push_back({"a band gives the last cell what is left", aBandGivesTheLastCellWhatIsLeft});
  cases.push_back({"a band leaves a cell given nothing alone", aBandLeavesACellGivenNothingAlone});
}

}  // namespace gardener::gfx::test
