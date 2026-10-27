// The Gfx component checks: every class under libraries/Gfx/src/ui/ driven against a stub display
// on the host, with no interpreter and no board. A case asserts the numbers a component answers and
// the pixels it leaves in the frame, and the run exits non-zero when one fails.

#include <stdio.h>
#include <string.h>

#include <Gfx.h>

#include "host/StubDisplay.h"

namespace {

using namespace gardener::gfx;
using namespace gardener::gfx::ui;
using gardener::gfx::test::StubDisplay;

constexpr int16_t PageWidth = 128;
constexpr int16_t PageHeight = 64;

int failures = 0;
const char* failureText = "";

void expect(bool condition, const char* what) {
  if (condition) return;
  if (failures == 0) failureText = what;
  failures++;
}

void expectEq(long actual, long expected, const char* what) {
  if (actual == expected) return;
  if (failures == 0) {
    static char message[160];
    snprintf(message, sizeof(message), "%s: expected %ld, got %ld", what, expected, actual);
    failureText = message;
  }
  failures++;
}

void expectText(const char* actual, const char* expected, const char* what) {
  if (strcmp(actual, expected) == 0) return;
  if (failures == 0) {
    static char message[160];
    snprintf(message, sizeof(message), "%s: expected \"%s\", got \"%s\"", what, expected, actual);
    failureText = message;
  }
  failures++;
}

// A page-sized stub and a canvas over the whole of it: what a component draws through.
StubDisplay display(PageWidth, PageHeight);
Canvas canvas(display.buffer(), display.size(), display.size());

void reset() {
  display.clearFrame();
  display.resetFlushes();
}

// --- the base ---------------------------------------------------------------

void componentStoresTheBoxItWasGiven() {
  Component component;
  expectEq(component.width(), 0, "a component starts with no box");
  component.setBox(3, 4, 20, 10);
  expectEq(component.x(), 3, "the box x");
  expectEq(component.y(), 4, "the box y");
  expectEq(component.width(), 20, "the box width");
  expectEq(component.height(), 10, "the box height");
  const Size room = component.use(20, 10);
  expectEq(room.width, 20, "a plain component takes the whole cell across");
  expectEq(room.height, 10, "a plain component takes the whole cell down");
  expect(component.declaredCell() == nullptr, "a plain component declares no cell");
}

void anInvertedComponentFlipsItsBox() {
  reset();
  canvas.draw(shapes::FillRect(4, 4, 6, 6, Ink));
  Component component;
  component.setBox(4, 4, 6, 6);
  component.setInverted(true);
  component.draw(canvas);
  expectEq(display.at(6, 6), 0, "the flip turned the ink to paper");
  expectEq(display.at(3, 3), 0, "the flip stops at the box");
  expectEq(display.at(10, 10), 0, "the flip stops at the box on the far side");
}

// --- text -------------------------------------------------------------------

void aTextMeasuresTheBuiltInFace() {
  Text text("AB");
  const Size ink = text.ink();
  expectEq(ink.width, 12, "two built-in glyphs are twelve pixels across");
  expectEq(ink.height, 8, "the built-in face is eight pixels tall");
}

void aTextScalesTheFaceItDraws() {
  Text text("AB", nullptr, 2);
  const Size ink = text.ink();
  expectEq(ink.width, 24, "two glyphs at scale two are twenty-four across");
  expectEq(ink.height, 16, "the built-in face at two is sixteen tall");
}

void aTextWithNoFaceIsTheSystemFace() {
  Text withNil("5", nullptr);
  Text withBuiltin("5", &BuiltinFont);
  withNil.setBox(0, 0, 6, 8);
  withBuiltin.setBox(0, 0, 6, 8);
  reset();
  withNil.draw(canvas);
  const int nilInk = display.inkCount(0, 0, 6, 8);
  reset();
  withBuiltin.draw(canvas);
  expectEq(nilInk, display.inkCount(0, 0, 6, 8), "no face draws what the built-in face draws");
  expect(nilInk > 0, "the glyph was drawn");
  expectEq(withNil.scale(), 1, "a text with no scale draws at one");
}

void aTextMeasuresACompiledFace() {
  Text text("A", fonts::Sans12);
  const Size ink = text.ink();
  expect(ink.width > 0, "a compiled face measures a width");
  expect(ink.height > 8, "a compiled face at twelve points is taller than the built-in one");
}

void aTextNarrowsItsRoomToTheCell() {
  Text text("AB");
  const Size wide = text.use(8, 100);
  expectEq(wide.width, 8, "the room is no wider than the cell");
  expectEq(wide.height, 8, "the room keeps the ink height");
  const Size shortCell = text.use(100, 4);
  expectEq(shortCell.width, 12, "the room keeps the ink width");
  expectEq(shortCell.height, 4, "the room is no taller than the cell");
}

void aTextDrawsInItsBox() {
  reset();
  Text text("I");
  text.setBox(10, 10, 6, 8);
  text.draw(canvas);
  expect(display.anyInk(10, 10, 6, 8), "the glyph landed inside the box");
  expect(!display.anyInk(0, 0, 10, 8), "nothing was drawn left of the box");
}

void aTextWiderThanItsCellIsDrawnWhole() {
  reset();
  Text text("AB");
  text.setBox(0, 0, 6, 8);
  text.draw(canvas);
  expect(display.anyInk(6, 0, 6, 8), "the second glyph is drawn past the cell it was given");
}

void aTextKeepsItsBoxAcrossAValueChange() {
  Text text("1");
  text.setBox(2, 3, 20, 8);
  text.setValue("22");
  expectEq(text.x(), 2, "the box x is unchanged");
  expectEq(text.width(), 20, "the box width is unchanged");
}

// --- icon -------------------------------------------------------------------

void anIconIsTwelveByEight() {
  Icon icon;
  expectEq(icon.width(), Icon::Width, "an icon starts twelve wide");
  expectEq(icon.height(), Icon::Height, "an icon starts eight tall");
}

// An icon subclass that inks the whole of its box, which is what a glyph does.
class BlockIcon : public Icon {
 public:
  void draw(Canvas& canvas) override {
    canvas.draw(shapes::FillRect(x(), y(), width(), height(), Ink));
    Icon::draw(canvas);
  }
};

void anIconDrawsInItsBox() {
  reset();
  BlockIcon icon;
  icon.setBox(30, 20, Icon::Width, Icon::Height);
  icon.draw(canvas);
  expect(display.anyInk(30, 20, Icon::Width, Icon::Height), "the glyph landed in the box");
  expect(!display.anyInk(0, 0, 30, 20), "nothing was drawn outside it");
}

// --- the bar and the scrollbar ----------------------------------------------

void aProgressBarDrawsAtItsEnds() {
  reset();
  ProgressBar empty(0.0f);
  empty.setBox(0, 0, 20, 8);
  empty.draw(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 0, "a bar at zero inks none of its inside");

  reset();
  ProgressBar full(100.0f);
  full.setBox(0, 0, 20, 8);
  full.draw(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 18 * 6, "a bar at a hundred inks all of it");

  reset();
  ProgressBar half(50.0f);
  half.setBox(0, 0, 20, 8);
  half.draw(canvas);
  expectEq(display.inkCount(1, 1, 9, 6), 9 * 6, "a bar at half inks the left half");
  expectEq(display.inkCount(10, 1, 9, 6), 0, "and none of the right");
}

void aProgressBarHoldsItsValueToItsEnds() {
  ProgressBar over(180.0f);
  ProgressBar under(-40.0f);
  over.setBox(0, 0, 20, 8);
  under.setBox(0, 0, 20, 8);
  expect(over.value() > 100.0f, "the bar keeps the value it was given");
  reset();
  over.draw(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 18 * 6, "a value past a hundred is held to it");
  reset();
  under.draw(canvas);
  expectEq(display.inkCount(1, 1, 18, 6), 0, "a value below zero is held to it");
}

void aScrollbarDrawsItsThumb() {
  reset();
  Scrollbar bar;
  bar.setBox(120, 0, 6, 20);
  bar.setCount(40);
  bar.setVisible(10);
  bar.setIndex(0);
  expectEq(bar.thumbHeight(), 5, "a quarter of the rows showing is a quarter of the bar");
  expectEq(bar.thumbTop(), 0, "the first row puts the thumb at the top");
  bar.setIndex(30);
  expectEq(bar.thumbTop(), 15, "the last row puts the thumb at the bottom");
  bar.draw(canvas);
  expect(display.anyInk(120, 0, 6, 20), "the bar drew its track and thumb");
}

void aScrollbarWithNoRowsDrawsNothing() {
  reset();
  Scrollbar bar;
  bar.setBox(120, 0, 6, 20);
  bar.draw(canvas);
  expectEq(display.inkCount(118, 0, 10, 20), 0, "a bar with no rows draws nothing");
}

// --- duration ---------------------------------------------------------------

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
  Duration duration(DurationFormat::Minutes);
  duration.setSeconds(270);
  expectText(duration.text(), "4m30s", "the text of ninety seconds short of five minutes");
  duration.setBox(0, 0, 40, 8);
  duration.draw(canvas);
  expect(display.anyInk(0, 0, 40, 8), "the duration drew its text");

  duration.setUnknown();
  expectText(duration.text(), "--", "an unknown duration prints dashes");
}

// --- layout -----------------------------------------------------------------

// A child that takes the whole cell it is given and inks it, so a case can read the box back.
class Cell0 : public Component {
 public:
  void draw(Canvas& canvas) override {
    canvas.draw(shapes::FillRect(x(), y(), width(), height(), Ink));
    Component::draw(canvas);
  }
};

void aLayoutTakesTheBoxItIsGiven() {
  reset();
  Cell0 inner;
  Layout layout;
  layout.add(inner);
  layout.setBox(10, 5, 40, 20);
  layout.draw(canvas);
  expectEq(inner.x(), 10, "the child starts at the layout's corner");
  expectEq(inner.y(), 5, "the child starts at the layout's corner");
  expectEq(inner.width(), 40, "the child fills the layout across");
  expectEq(inner.height(), 20, "the child fills the layout down");
}

void aLayoutStacksFullWidthChildrenDown() {
  Layout firstRow(Cell(1.0f, 0.5f));
  Layout secondRow(Cell(1.0f, 0.5f));
  Layout column;
  column.add(firstRow);
  column.add(secondRow);
  column.setBox(0, 0, 128, 26);
  column.draw(canvas);
  expectEq(firstRow.width(), 128, "a full-width child is the whole width");
  expectEq(firstRow.height(), 13, "half the height is half the column");
  expectEq(secondRow.y(), 13, "the second row starts below the first");
}

void aLeafTakesTheCellItIsGiven() {
  Cell0 leaf;
  Layout column;
  column.add(leaf);
  column.setBox(4, 6, 30, 20);
  column.draw(canvas);
  expectEq(leaf.x(), 4, "a child that declares nothing starts at the layout's corner");
  expectEq(leaf.width(), 30, "and takes the whole cell across");
  expectEq(leaf.height(), 20, "and the whole of it down");
}

void aLayoutStacksNarrowChildrenAcross() {
  Layout rowCell(Cell(0.5f, 1.0f));
  Layout farCell(Cell(0.5f, 1.0f));
  Layout row;
  row.add(rowCell);
  row.add(farCell);
  row.setBox(0, 0, 128, 26);
  row.draw(canvas);
  expectEq(rowCell.x(), 0, "the first cell starts at the left");
  expectEq(rowCell.width(), 64, "a half cell is half the row");
  expectEq(farCell.x(), 64, "the second cell follows it");
}

void equalThirdsOfARowFillIt() {
  Layout a(Cell(1.0f / 3.0f, 1.0f));
  Layout b(Cell(1.0f / 3.0f, 1.0f));
  Layout c(Cell(1.0f / 3.0f, 1.0f));
  Layout row;
  row.add(a);
  row.add(b);
  row.add(c);
  row.setBox(0, 0, 96, 12);
  row.draw(canvas);
  expectEq(a.width(), 32, "a third of ninety-six");
  expectEq(b.width(), 32, "a third of ninety-six");
  expectEq(c.width(), 32, "a third of ninety-six");
  expectEq(c.x() + c.width(), 96, "the three fill the row");
}

void aRoundedSpanIsClampedToWhatIsLeft() {
  Layout firstCell(Cell(1.0f, 0.5f));
  Layout secondCell(Cell(1.0f, 0.5f));
  Layout column;
  column.add(firstCell);
  column.add(secondCell);
  column.setBox(0, 0, 10, 9);
  column.draw(canvas);
  expectEq(firstCell.height(), 5, "half of an odd height rounds up");
  expectEq(secondCell.height(), 4, "and what is left goes to the other");
  expectEq(secondCell.y(), 5, "the second starts where the first ends");
}

void aDeclaredSizeAboveOneIsPixels() {
  Layout rowCell(Cell(1.0f, 8.0f));
  Layout restCell(Cell(1.0f, 1.0f));
  Layout column;
  column.add(rowCell);
  column.add(restCell);
  column.setBox(0, 0, 128, 26);
  column.draw(canvas);
  expectEq(rowCell.height(), 8, "eight pixels is eight pixels");
  expectEq(restCell.height(), 18, "the leaf takes what is left");
}

void aLayoutTakesTheAlignmentItIsGiven() {
  Layout cell(Cell(1.0f, 0.2f));
  Alignment centred;
  centred.vertical = Vertical::Center;
  Layout column(Cell(), centred);
  column.add(cell);
  column.setBox(0, 0, 100, 20);
  column.draw(canvas);
  expectEq(cell.height(), 4, "a fifth of twenty is four");
  expectEq(cell.y(), 8, "the block is centred in the sixteen left over");
}

void aTextSitsWhereTheAlignmentSays() {
  Text text("A");
  Alignment centred;
  centred.horizontal = Horizontal::Center;
  centred.vertical = Vertical::Center;
  Layout layout(Cell(), centred);
  layout.add(text);
  layout.setBox(0, 0, 100, 20);
  layout.draw(canvas);
  const Size ink = text.ink();
  expectEq(text.x(), (100 - ink.width) / 2, "the text is centred across");
  expectEq(text.y(), (20 - ink.height) / 2, "the text is centred down");
  expectEq(text.width(), ink.width, "the text is narrowed to its ink");
}

void aLayoutIsResolvedOnEachDraw() {
  Cell0 child;
  Layout layout;
  layout.add(child);
  layout.setBox(0, 0, 40, 20);
  layout.draw(canvas);
  expectEq(child.width(), 40, "the child took the first box");
  layout.setBox(0, 0, 80, 20);
  layout.draw(canvas);
  expectEq(child.width(), 80, "the child took the box that replaced it");
}

void aRowStatesTheFullWidthAndAColumnTheFullHeight() {
  RowLayout row(0.5f);
  ColumnLayout column(0.25f);
  expectEq((long)(row.declaredCell()->width * 100.0f), 100, "a row is the full width");
  expectEq((long)(row.declaredCell()->height * 100.0f), 50, "a row takes the height it was given");
  expectEq((long)(column.declaredCell()->width * 100.0f), 25,
           "a column takes the width it was given");
  expectEq((long)(column.declaredCell()->height * 100.0f), 100, "a column is the full height");
}

// A child that records that it drew, so the order a layout draws its children in can be read back.
char orderLog[8] = {};
int orderCount = 0;

class Recorder : public Component {
 public:
  explicit Recorder(char id) : id_(id) {}
  void draw(Canvas&) override {
    if (orderCount < 8) orderLog[orderCount++] = id_;
  }

 private:
  char id_;
};

void aLayoutDrawsItsChildrenInOrder() {
  orderCount = 0;
  Recorder first('a');
  Recorder second('b');
  Recorder third('c');
  Layout layout;
  layout.add(first);
  layout.add(second);
  layout.add(third);
  layout.setBox(0, 0, 10, 10);
  layout.draw(canvas);
  orderLog[orderCount] = '\0';
  expectText(orderLog, "abc", "the children drew in the order they were added");
}

void aLayoutHandsItsBoxOnToTheBandItsChildrenDraw() {
  reset();
  Cell0 inner;
  Layout half(Cell(0.5f, 1.0f));
  half.add(inner);
  Layout row;
  row.add(half);
  row.setBox(0, 0, 128, 26);
  row.draw(canvas);
  expectEq(inner.width(), 64, "a nested layout hands its own half on to its child");
  expect(display.anyInk(0, 0, 64, 26), "the nested child drew inside its half");
  expectEq(display.inkCount(64, 0, 64, 26), 0, "and nothing was drawn in the other half");
}

// --- child canvases ---------------------------------------------------------

void aChildDrawsAtItsParentsOffset() {
  reset();
  canvas.flush(display);
  Canvas plot = canvas.child(Rect(20, 10, 40, 20));
  expectEq(plot.width(), 40, "the child is as wide as the area");
  expectEq(plot.height(), 20, "the child is as tall as the area");
  plot.setPixel(0, 0, Ink);
  plot.setPixel(39, 19, Ink);
  expectEq(display.at(20, 10), Ink, "the child's origin is the area's corner");
  expectEq(display.at(59, 29), Ink, "the child's far corner is the area's");
}

void aChildIsClippedToItsRect() {
  reset();
  canvas.flush(display);
  Canvas plot = canvas.child(Rect(20, 10, 40, 20));
  plot.setPixel(40, 0, Ink);
  plot.setPixel(0, 20, Ink);
  plot.setPixel(-1, 0, Ink);
  plot.setPixel(0, -1, Ink);
  expectEq(display.at(60, 10), 0, "a write past the right edge is dropped");
  expectEq(display.at(20, 30), 0, "a write past the bottom edge is dropped");
  expectEq(display.at(19, 10), 0, "a write before the left edge is dropped");
  expectEq(display.at(20, 9), 0, "a write above the top edge is dropped");
}

void siblingsDoNotOverlap() {
  reset();
  canvas.flush(display);
  Canvas left = canvas.child(Rect(0, 40, 10, 10));
  Canvas right = canvas.child(Rect(10, 40, 10, 10));
  left.setPixel(9, 9, Ink);
  expectEq(display.at(9, 49), Ink, "the first child drew at its far corner");
  expectEq(display.at(10, 49), 0, "and nothing landed in the second child's area");
  right.setPixel(0, 0, Ink);
  expectEq(display.at(10, 40), Ink, "the second child starts where its own area does");
}

void aChildOfAChildNests() {
  Canvas middle = canvas.child(Rect(30, 20, 40, 20));
  Canvas inner = middle.child(Rect(5, 5, 10, 10));
  expectEq(inner.width(), 10, "the inner canvas is its own area's width");
  reset();
  inner.setPixel(0, 0, Ink);
  expectEq(display.at(35, 25), Ink, "the inner origin is both areas' corners added up");
}

void aChildOfATurnedParentFollowsTheTurn() {
  reset();
  Canvas turned(display.buffer(), display.size(), display.size());
  turned.setRotation(90);
  Canvas quarter = turned.child(Rect(10, 5, 30, 20));
  expectEq(quarter.width(), 30, "the child is the area's own width, as the parent counts it");
  expectEq(quarter.height(), 20, "and the area's own height");
  quarter.setPixel(0, 0, Ink);
  quarter.setPixel(29, 19, Ink);
  // A turned page's (x, y) is the panel's (width - 1 - y, x).
  expectEq(display.at(122, 10), Ink, "the child's origin is where the parent's would be");
  expectEq(display.at(103, 39), Ink, "and so is its far corner");
}

void aChildCannotBeTurned() {
  Canvas plot = canvas.child(Rect(0, 0, 40, 20));
  plot.setRotation(90);
  expectEq(plot.rotation(), 0, "a child keeps the turn it inherited");
  canvas.setRotation(0);
}

void aFlushSendsWhatAChildDrew() {
  canvas.flush(display);
  reset();
  Canvas plot = canvas.child(Rect(50, 30, 10, 10));
  plot.setPixel(0, 0, Ink);
  canvas.flush(display);
  expectEq(display.flushCount(), 1, "one flush on the frame's canvas");
  const Rect dirty = display.lastDirty();
  expectEq(dirty.x, 50, "the flush covers the child's pixel");
  expectEq(dirty.y, 30, "the flush covers the child's pixel");
  expectEq(dirty.width, 1, "and nothing more than it");
  expectEq(dirty.height, 1, "and nothing more than it");
  canvas.flush(display);
  expectEq(display.lastDirty().width, 0, "a second flush has nothing to send");
}

void aChildOutsideItsParentDrawsNothing() {
  Canvas plot = canvas.child(Rect(PageWidth + 10, PageHeight + 10, 10, 10));
  expectEq(plot.width(), 0, "an area outside the canvas clamps to nothing");
  reset();
  plot.setPixel(0, 0, Ink);
  expectEq(display.inkCount(0, 0, PageWidth, PageHeight), 0, "and writes nothing");
}

struct Case {
  const char* name;
  void (*run)();
};

const Case Cases[] = {
    {"a component stores the box it was given", componentStoresTheBoxItWasGiven},
    {"an inverted component flips its box", anInvertedComponentFlipsItsBox},
    {"a text measures the built-in face", aTextMeasuresTheBuiltInFace},
    {"a text scales the face it draws", aTextScalesTheFaceItDraws},
    {"a text with no face is the system face", aTextWithNoFaceIsTheSystemFace},
    {"a text measures a compiled face", aTextMeasuresACompiledFace},
    {"a text narrows its room to the cell", aTextNarrowsItsRoomToTheCell},
    {"a text draws in its box", aTextDrawsInItsBox},
    {"a text wider than its cell is drawn whole", aTextWiderThanItsCellIsDrawnWhole},
    {"a text keeps its box across a value change", aTextKeepsItsBoxAcrossAValueChange},
    {"an icon is twelve by eight", anIconIsTwelveByEight},
    {"an icon draws in its box", anIconDrawsInItsBox},
    {"a progress bar draws at its ends", aProgressBarDrawsAtItsEnds},
    {"a progress bar holds its value to its ends", aProgressBarHoldsItsValueToItsEnds},
    {"a scrollbar draws its thumb", aScrollbarDrawsItsThumb},
    {"a scrollbar with no rows draws nothing", aScrollbarWithNoRowsDrawsNothing},
    {"a duration formats each unit", aDurationFormatsEachUnit},
    {"a duration draws its text", aDurationDrawsItsText},
    {"a layout takes the box it is given", aLayoutTakesTheBoxItIsGiven},
    {"a layout stacks full-width children down", aLayoutStacksFullWidthChildrenDown},
    {"a leaf takes the cell it is given", aLeafTakesTheCellItIsGiven},
    {"a layout stacks narrow children across", aLayoutStacksNarrowChildrenAcross},
    {"equal thirds of a row fill it", equalThirdsOfARowFillIt},
    {"a rounded span is clamped to what is left", aRoundedSpanIsClampedToWhatIsLeft},
    {"a declared size above one is pixels", aDeclaredSizeAboveOneIsPixels},
    {"a layout takes the alignment it is given", aLayoutTakesTheAlignmentItIsGiven},
    {"a text sits where the alignment says", aTextSitsWhereTheAlignmentSays},
    {"a layout is resolved on each draw", aLayoutIsResolvedOnEachDraw},
    {"a row states the full width and a column the full height",
     aRowStatesTheFullWidthAndAColumnTheFullHeight},
    {"a layout draws its children in order", aLayoutDrawsItsChildrenInOrder},
    {"a layout hands its box on to a nested child", aLayoutHandsItsBoxOnToTheBandItsChildrenDraw},
    {"a child draws at its parent's offset", aChildDrawsAtItsParentsOffset},
    {"a child is clipped to its rect", aChildIsClippedToItsRect},
    {"siblings do not overlap", siblingsDoNotOverlap},
    {"a child of a child nests", aChildOfAChildNests},
    {"a child of a turned parent follows the turn", aChildOfATurnedParentFollowsTheTurn},
    {"a child cannot be turned", aChildCannotBeTurned},
    {"a flush sends what a child drew", aFlushSendsWhatAChildDrew},
    {"a child outside its parent draws nothing", aChildOutsideItsParentDrawsNothing},
};

}  // namespace

int main() {
  int passed = 0;
  int failed = 0;
  for (const Case& item : Cases) {
    failures = 0;
    failureText = "";
    item.run();
    if (failures == 0) {
      printf("[x] %s\n", item.name);
      passed++;
    } else {
      printf("[!] %s\n      %s\n", item.name, failureText);
      failed++;
    }
  }
  printf("%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
