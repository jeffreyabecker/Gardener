// The row list: the model's ring and its window, and what draw() puts on the glass.

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <doctest.h>

#include "suite.h"

#include "shapes/RowList.h"

namespace gardener::gfx::test {
namespace {

constexpr int16_t RowHeight = 8;

// The columns the rows take while the bar shows: the band less the scrollbar's own column.
constexpr int16_t RowsWidth = PageWidth - ScrollbarColumn;

// The bar's own two columns at the band's right edge, and the paper the rest of its column holds.
constexpr int16_t BarWidth = 2;
constexpr int16_t GapWidth = ScrollbarColumn - BarWidth;

// A row painted as a filled rectangle, so where it lands and whether it is inverted are both a count
// of ink: a row out of the window inks its whole cell and a current row inks none of it.
RowList::Row inkRow(const char* value) {
  return RowList::Row{String(value), rectangle(Box::whole(), Ink, true)};
}

std::vector<RowList::Row> inkRows(std::initializer_list<const char*> values) {
  std::vector<RowList::Row> out;
  for (const char* value : values)
    out.push_back(inkRow(value));
  return out;
}

std::vector<RowList::Row> sevenRows() {
  return inkRows({"r0", "r1", "r2", "r3", "r4", "r5", "r6"});
}

// A four-by-four bitmap of one ink pixel in its top-left corner, which is the corner a row places.
const char* CornerMark = "04040080000000";

// A band `lines` rows tall across the panel, which is what a scene hands the list.
View band(int16_t lines) {
  return canvas.child(Rect(0, 0, PageWidth, static_cast<int16_t>(lines * RowHeight)));
}

}  // namespace

// --- the model --------------------------------------------------------------

TEST_CASE("a wrapping row list rings at either end") {
  RowList list(RowHeight, sevenRows(), "r0", true);
  CHECK_EQ(list.current(), 0);
  CHECK(list.nudge(-1));
  CHECK_EQ(list.current(), 6);
  CHECK_EQ(std::string(list.selected().c_str()), std::string("r6"));
  CHECK(list.nudge(1));
  CHECK_EQ(list.current(), 0);
  CHECK(list.nudge(1));
  CHECK_EQ(list.current(), 1);
}

TEST_CASE("a clamped row list stops at either end") {
  RowList start(RowHeight, sevenRows(), "r0", false);
  CHECK_FALSE(start.nudge(-1));
  CHECK_EQ(start.current(), 0);

  RowList end(RowHeight, sevenRows(), "r6", false);
  CHECK_FALSE(end.nudge(1));
  CHECK_EQ(end.current(), 6);
  CHECK_FALSE(end.nudge(0));
  CHECK(end.nudge(-1));
  CHECK_EQ(end.current(), 5);
}

TEST_CASE("a row list whose value no row holds has no current row") {
  RowList list(RowHeight, sevenRows(), "gone", true);
  CHECK_EQ(list.current(), -1);
  CHECK_EQ(std::string(list.selected().c_str()), std::string("gone"));

  // The move enters at the end it came toward: the first row down and the last one up.
  RowList down(RowHeight, sevenRows(), "gone", false);
  CHECK(down.nudge(1));
  CHECK_EQ(down.current(), 0);

  RowList up(RowHeight, sevenRows(), "gone", false);
  CHECK(up.nudge(-1));
  CHECK_EQ(up.current(), 6);
}

TEST_CASE("an empty row list nudges nothing") {
  RowList list(RowHeight, {}, "gone", true);
  CHECK_EQ(list.current(), -1);
  CHECK_FALSE(list.nudge(1));
  CHECK_FALSE(list.nudge(-1));
}

TEST_CASE("a row list of one row nudges nothing") {
  RowList list(RowHeight, inkRows({"only"}), "only", true);
  CHECK_EQ(list.current(), 0);
  CHECK_FALSE(list.nudge(1));
  CHECK_FALSE(list.nudge(-1));
}

TEST_CASE("a row list seats on the value it is given") {
  RowList list(RowHeight, sevenRows(), "r0", true);
  list.select("r3");
  CHECK_EQ(list.current(), 3);
  CHECK_EQ(std::string(list.selected().c_str()), std::string("r3"));
  CHECK(list.nudge(1));
  CHECK_EQ(list.current(), 4);

  // A value no row holds leaves no row current, which is what a nullable setting's nothing seats.
  list.select(String());
  CHECK_EQ(list.current(), -1);
  CHECK_EQ(std::string(list.selected().c_str()), std::string(""));

  RowList empty(RowHeight, {}, "gone", true);
  empty.select("r0");
  CHECK_EQ(empty.current(), -1);
}

// --- the drawing ------------------------------------------------------------

TEST_CASE("a row list draws the rows its band holds") {
  RowList list(RowHeight, sevenRows(), "r0", true);
  reset();
  View view = band(4);
  list.draw(view);

  // Four rows fit: the current one is the hole the inversion makes, and the three after it ink their
  // cells. The rows stop four columns short of the band and the bar inks its two.
  CHECK_EQ(display.inkCount(0, 0, RowsWidth, RowHeight), 0);
  CHECK_EQ(display.inkCount(0, RowHeight, RowsWidth, RowHeight), RowsWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, 2 * RowHeight, RowsWidth, RowHeight), RowsWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, 3 * RowHeight, RowsWidth, RowHeight), RowsWidth * RowHeight);
  CHECK_EQ(display.inkCount(RowsWidth, 0, GapWidth, 4 * RowHeight), 0);
  CHECK(display.anyInk(PageWidth - BarWidth, 0, BarWidth, 4 * RowHeight));
  CHECK_EQ(display.inkCount(0, 4 * RowHeight, PageWidth, PageHeight - 4 * RowHeight), 0);
}

TEST_CASE("a row list scrolls to keep the current row in view") {
  RowList list(RowHeight, sevenRows(), "r4", true);
  reset();
  View view = band(4);
  list.draw(view);

  // The window starts at the second row, so rows one to four are drawn and only the current one is
  // inverted.
  CHECK_EQ(list.current(), 4);
  CHECK_EQ(display.inkCount(0, 0, RowsWidth, 3 * RowHeight), 3 * RowsWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, 3 * RowHeight, RowsWidth, RowHeight), 0);
  CHECK_EQ(display.inkCount(0, 4 * RowHeight, PageWidth, PageHeight - 4 * RowHeight), 0);
}

TEST_CASE("a row list shorter than its band takes the full width") {
  RowList list(RowHeight, inkRows({"r0", "r1", "r2"}), "r1", true);
  reset();
  View view = band(4);
  list.draw(view);

  // Three rows in a four-row band draw no bar, so they take the whole width and the band's last row
  // stays paper.
  CHECK_EQ(display.inkCount(0, 0, PageWidth, RowHeight), PageWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, RowHeight, PageWidth, RowHeight), 0);
  CHECK_EQ(display.inkCount(0, 2 * RowHeight, PageWidth, RowHeight), PageWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, 3 * RowHeight, PageWidth, RowHeight), 0);
}

TEST_CASE("a row list that outgrows its band gives its rows up for the bar") {
  RowList list(RowHeight, sevenRows(), "r1", true);
  reset();
  View view = band(4);
  list.draw(view);

  // The rows end where the bar's column begins, with the two columns of paper between them inked by
  // nothing and the bar's own two inked.
  CHECK_EQ(display.inkCount(0, 0, RowsWidth, RowHeight), RowsWidth * RowHeight);
  CHECK_EQ(display.inkCount(RowsWidth, 0, GapWidth, 4 * RowHeight), 0);
  CHECK(display.anyInk(PageWidth - BarWidth, 0, BarWidth, 4 * RowHeight));
}

TEST_CASE("a five-pixel band draws one clipped row") {
  RowList list(RowHeight, sevenRows(), "gone", true);
  reset();
  View view = canvas.child(Rect(0, 0, PageWidth, 5));
  list.draw(view);

  // One row fits and its eight rows are cut to the five the band has; nothing reaches past them.
  CHECK_EQ(display.inkCount(0, 0, RowsWidth, 5), RowsWidth * 5);
  CHECK_EQ(display.inkCount(0, 5, PageWidth, PageHeight - 5), 0);
}

TEST_CASE("a row list sits where the caller justifies it") {
  RowList list(RowHeight, inkRows({"r0", "r1"}), "gone", false);
  reset();
  View view = band(5);
  list.draw(view);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, 2 * RowHeight), 2 * PageWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, 2 * RowHeight, PageWidth, 3 * RowHeight), 0);

  // Centred, the twenty-four spare rows become twelve above the block and twelve below it.
  reset();
  list.draw(view, Justify::Center);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, 12), 0);
  CHECK_EQ(display.inkCount(0, 12, PageWidth, 2 * RowHeight), 2 * PageWidth * RowHeight);
  CHECK_EQ(display.inkCount(0, 28, PageWidth, 12), 0);

  reset();
  list.draw(view, Justify::End);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, 24), 0);
  CHECK_EQ(display.inkCount(0, 24, PageWidth, 2 * RowHeight), 2 * PageWidth * RowHeight);
}

TEST_CASE("a row list fills the cell of the row it inverts") {
  std::vector<RowList::Row> painted;
  painted.push_back(RowList::Row{String("r0"), text("A")});
  painted.push_back(RowList::Row{String("r1"), text("B")});
  painted.push_back(RowList::Row{String("r2"), text("C")});
  RowList list(RowHeight, std::move(painted), "r1", false);
  reset();
  View view = band(4);
  list.draw(view);

  // The rows are centred strings, so only the current row inks a cell's worth: the inversion fills
  // the cell and the glyph comes out as paper.
  CHECK(display.inkCount(0, 0, PageWidth, RowHeight) < PageWidth * RowHeight / 4);
  CHECK(display.inkCount(0, RowHeight, PageWidth, RowHeight) > PageWidth * RowHeight * 3 / 4);
  CHECK(display.inkCount(0, 2 * RowHeight, PageWidth, RowHeight) < PageWidth * RowHeight / 4);
}

// --- the builders -----------------------------------------------------------

TEST_CASE("a text row carries its value and draws its title") {
  RowList::Row row = RowList::textRow("key", "A");
  CHECK_EQ(std::string(row.value.c_str()), std::string("key"));
  REQUIRE(row.paint.get() != nullptr);
  reset();
  View view = band(1);
  row.paint->draw(view);
  CHECK(display.anyInk(0, 0, PageWidth, RowHeight));
}

TEST_CASE("a text row places its title by the alignment it is given") {
  reset();
  View view = band(1);
  RowList::Row start = RowList::textRow("key", "A", Justify::Start);
  start.paint->draw(view);
  CHECK(display.anyInk(0, 0, 8, RowHeight));
  CHECK_EQ(display.inkCount(PageWidth / 2, 0, PageWidth / 2, RowHeight), 0);

  reset();
  RowList::Row centre = RowList::textRow("key", "A");
  centre.paint->draw(view);
  CHECK_EQ(display.inkCount(0, 0, PageWidth / 4, RowHeight), 0);
  CHECK(display.anyInk(PageWidth / 2 - 6, 0, 12, RowHeight));

  reset();
  RowList::Row end = RowList::textRow("key", "A", Justify::End);
  end.paint->draw(view);
  CHECK(display.anyInk(PageWidth - 8, 0, 8, RowHeight));
  CHECK_EQ(display.inkCount(0, 0, PageWidth / 2, RowHeight), 0);
}

TEST_CASE("an icon row draws its mark before its title") {
  RowList::Row row = RowList::textRow("key", "A", CornerMark);
  CHECK_EQ(std::string(row.value.c_str()), std::string("key"));
  reset();
  View view = band(1);
  row.paint->draw(view);

  // The mark takes the four columns it names, centred down the eight-row cell, and the title inks
  // the columns after it.
  CHECK_EQ(display.inkCount(0, 0, 4, RowHeight), 1);
  CHECK(display.anyInk(0, 2, 1, 1));
  CHECK(display.anyInk(4, 0, PageWidth - 4, RowHeight));
}

TEST_CASE("an icon row places its title by the alignment it is given") {
  RowList::Row left = RowList::textRow("key", "TITLE", CornerMark, Justify::Start);
  reset();
  View view = band(1);
  left.paint->draw(view);
  CHECK(display.anyInk(4, 0, 8, RowHeight));
  CHECK_EQ(display.inkCount(PageWidth / 2, 0, PageWidth / 2, RowHeight), 0);
}

TEST_CASE("an icon that names no bitmap draws the title alone") {
  RowList::Row plain = RowList::textRow("key", "A");
  RowList::Row broken = RowList::textRow("key", "A", "not a bitmap");
  reset();
  View view = band(1);
  plain.paint->draw(view);
  const int plainInk = display.inkCount(0, 0, PageWidth, RowHeight);
  reset();
  broken.paint->draw(view);
  CHECK(plainInk > 0);
  CHECK_EQ(display.inkCount(0, 0, PageWidth, RowHeight), plainInk);
}

}  // namespace gardener::gfx::test
