// The fonts module's tables of names: the families a caller registers, which are a design's cuts
// ordered shortest first, and the aliases, which are generic names pointed at them. The shipped
// names are the drawing cases' because they are what a page draws with; what a caller adds is here.

#include <string.h>

#include <doctest.h>

#include "suite.h"

#include "fonts/AdafruitFont.h"
#include "fonts/Battery19.h"
#include "fonts/ProFont10.h"
#include "fonts/ProFont12.h"
#include "fonts/ProFont17.h"
#include "fonts/ProFont29.h"
#include "fonts/Ttyp011.h"
#include "fonts/Ttyp016.h"
#include "fonts/Ttyp022.h"
#include "fonts/X11Cell10x20.h"
#include "fonts/X11Cell5x7.h"
#include "fonts/X11Cell6x10.h"
#include "fonts/X11Cell7x14.h"

namespace gardener::gfx::test {
namespace {

// A probe face of one column and the rows asked for, drawn from a blank bitmap: enough for a
// family's order, which reads only the line box.
Font probeFace(uint8_t rows) {
  static const uint8_t blank[256] = {};
  return {Font::Kind::Fixed, blank, 1, rows, nullptr, 0, 0, static_cast<uint8_t>(rows - 1), 1};
}

}  // namespace

TEST_CASE("the two converted designs answer to one name and hold their rungs shortest first") {
  // Every rung's line box is the one its source declares — not the size in the face's own name,
  // which is the design's nominal em.
  const struct {
    const Font* face;
    uint8_t line;
  } rungs[] = {
      {&ProFont10, 8},   {&ProFont12, 10},  {&ProFont17, 14},   {&ProFont29, 24},
      {&X11Cell5x7, 7},  {&X11Cell6x10, 9}, {&X11Cell7x14, 12}, {&X11Cell10x20, 17},
  };
  for (const auto& rung : rungs) {
    CHECK_EQ(lineHeight(*rung.face), rung.line);
  }

  // The families hold the rungs shortest first, and both are a design a page can name.
  const Family* profont = findFamily("profont");
  REQUIRE(profont != nullptr);
  CHECK_EQ(profont->count, 4);
  CHECK_EQ(profont->cuts[0], &ProFont10);
  CHECK_EQ(profont->cuts[3], &ProFont29);
  const Family* x11 = findFamily("x11");
  REQUIRE(x11 != nullptr);
  CHECK_EQ(x11->count, 4);
  CHECK_EQ(x11->cuts[0], &X11Cell5x7);
  CHECK_EQ(x11->cuts[3], &X11Cell10x20);
  for (uint8_t at = 1; at < profont->count; at++) {
    CHECK(lineHeight(*profont->cuts[at - 1]) <= lineHeight(*profont->cuts[at]));
    CHECK(lineHeight(*x11->cuts[at - 1]) <= lineHeight(*x11->cuts[at]));
  }
}

// The battery marks are an icon face: one cut, eight codes, and the level a page prints is the
// character itself.
TEST_CASE("the battery marks answer to their own name and draw the level asked for") {
  const Family* battery = findFamily("battery");
  REQUIRE(battery != nullptr);
  CHECK_EQ(battery->count, 1);
  CHECK_EQ(battery->cuts[0], &Battery19);
  CHECK_EQ(lineHeight(Battery19), 19);

  // Eight by nineteen from the empty level to the bolt, and the code past them is the blank the face
  // keeps in its table.
  const Size empty = measureText(&Battery19, "0", 1);
  CHECK_EQ(empty.width, 8);
  CHECK_EQ(empty.height, 19);
  CHECK_EQ(measureText(&Battery19, "7", 1).width, 0);

  // Drawn, the empty level inks the outline and a filled one inks the bars inside it.
  reset();
  View box = canvas.child(Rect(0, 0, 16, 24));
  text("0", battery)->draw(box);
  const int outline = display.inkCount(0, 0, 16, 24);
  CHECK(outline > 0);
  reset();
  text("5", battery)->draw(box);
  CHECK(display.inkCount(0, 0, 16, 24) > outline);
}

// The pitch a list of rows is drawn at is a family's shortest cut, not a number a page keeps.
TEST_CASE("a family answers the line its shortest cut takes") {
  setDefaultFamily(nullptr);
  CHECK_EQ(shortestLine(nullptr), 8);  // the Adafruit face, which is the default
  CHECK_EQ(shortestLine(findFamily("ttyp0")), 10);
  CHECK_EQ(shortestLine(findFamily("profont")), 8);
  CHECK_EQ(shortestLine(findFamily("x11")), 7);
  CHECK_EQ(shortestLine(findFamily("battery")), 19);
}

TEST_CASE("a rung of a converted design draws the glyph its source holds") {
  // The smallest and the largest X11 rungs, measured as their glyphs were converted: an 'A' five
  // wide and six tall in 5x7, and eight by thirteen in 10x20.
  const Size small = measureText(&X11Cell5x7, "A", 1);
  CHECK_EQ(small.width, 4);
  CHECK_EQ(small.height, 6);
  const Size large = measureText(&X11Cell10x20, "A", 1);
  CHECK_EQ(large.width, 8);
  CHECK_EQ(large.height, 13);

  // Drawn, the large one starts on the first row of its box: its ascent is thirteen rows and its
  // ink is thirteen tall.
  reset();
  View box = canvas.child(Rect(0, 0, 64, 24));
  const Font* only[] = {&X11Cell10x20};
  const Family oneCut = {"x11_10x20", only, 1};
  text("A", &oneCut, Justify::Start, Justify::Start)->draw(box);
  CHECK(display.anyInk(0, 0, 8, 1));
  CHECK_EQ(display.inkCount(0, 13, 64, 11), 0);
}

TEST_CASE("a height resolves to the tallest cut it can hold and the scale it spends") {
  const Family* ttyp0 = findFamily("ttyp0");

  // A height under the shortest cut is that cut at one scale: the answer is the smallest thing the
  // family has rather than nothing.
  Fit fit = resolve(ttyp0, 5);
  CHECK_EQ(fit.face, &Ttyp011);
  CHECK_EQ(fit.scale, 1);

  // Each cut at one scale where the band holds it exactly, and the tallest cut that fits at all
  // where the band is between two of them: 24 rows is the 22 px design at one scale, not the 16 px
  // one at one.
  CHECK_EQ(resolve(ttyp0, 10).face, &Ttyp011);
  CHECK_EQ(resolve(ttyp0, 13).face, &Ttyp016);
  CHECK_EQ(resolve(ttyp0, 17).face, &Ttyp022);
  CHECK_EQ(resolve(ttyp0, 24).face, &Ttyp022);
  CHECK_EQ(resolve(ttyp0, 24).scale, 1);

  // A band that holds two of a cut's lines is spent at two, and one that holds three at three.
  fit = resolve(ttyp0, 34);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 2);
  fit = resolve(ttyp0, 64);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 3);

  // Two designs answer the same band differently, because the ladder is the design's.
  CHECK_EQ(resolve(findFamily("x11"), 45).face, &X11Cell10x20);
  CHECK_EQ(resolve(findFamily("x11"), 45).scale, 2);
  CHECK_EQ(resolve(findFamily("profont"), 45).face, &ProFont29);
  CHECK_EQ(resolve(findFamily("profont"), 45).scale, 1);
}

TEST_CASE("a width narrows the cut and the scale the height chose") {
  const Family* ttyp0 = findFamily("ttyp0");

  // The home page's timer is five characters in a cell 64 across and 48 down: two lines of the 22 px
  // design are 110 pixels wide, so the timer is that design at one scale.
  Fit fit = resolve(ttyp0, "4m30s", 64, 48);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 1);

  // The same string across a whole page keeps the taller answer.
  fit = resolve(ttyp0, "4m30s", 128, 48);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 2);

  // A box too narrow for the tallest cut steps down through its scales and then to the next cut:
  // five characters in 40 pixels is the 16 px design at one, which is exactly 40 wide.
  fit = resolve(ttyp0, "123.4", 40, 45);
  CHECK_EQ(fit.face, &Ttyp016);
  CHECK_EQ(fit.scale, 1);

  // A string no cut fits is drawn at the height's own answer, and the box clips it.
  fit = resolve(ttyp0, "123.4", 10, 45);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 2);
}

TEST_CASE("no family resolves the default family") {
  // A leaf or a measurement that names no family draws in the default, which is the Adafruit design:
  // its eight row line, five times, in a band of forty-five rows.
  const Fit none = resolve(nullptr, 45);
  const Fit named = resolve(findFamily("adafruit"), 45);
  CHECK_EQ(none.face, named.face);
  CHECK_EQ(none.scale, named.scale);
  CHECK_EQ(none.face, &AdafruitFont);

  // A height that asks for nothing is the design's tallest cut at one scale, which is what measuring
  // a string with no box to fit it means.
  const Fit free = resolve(findFamily("ttyp0"), 0);
  CHECK_EQ(free.face, &Ttyp022);
  CHECK_EQ(free.scale, 1);
}

TEST_CASE("a fitted string takes the cut and the scale its box's height holds") {
  // A band forty-five rows tall: the 22 px design at two scales is thirty-four rows of line, which
  // centred leaves five rows above, and the letter starts six rows into that line box because the
  // face's ascent is sixteen and the letter is thirteen tall.
  reset();
  View band = canvas.child(Rect(0, 0, 128, 45));
  text("A", findFamily("ttyp0"))->draw(band);
  CHECK_EQ(display.inkCount(0, 0, 128, 11), 0);
  CHECK_EQ(display.inkCount(0, 37, 128, 8), 0);
  // Across, the ink is centred: twenty-two pixels of it in one hundred and twenty-eight, from the
  // fifty-fourth to the seventy-fifth column and nothing outside them.
  CHECK_EQ(display.inkCount(0, 0, 54, 45), 0);
  CHECK_EQ(display.inkCount(76, 0, 52, 45), 0);

  // A band of ten rows takes the shortest cut at one scale — its line is exactly ten — and the
  // letter starts one row down it, the face's ascent being nine and the letter eight tall.
  reset();
  View title = canvas.child(Rect(0, 0, 64, 10));
  text("A", findFamily("ttyp0"))->draw(title);
  CHECK_EQ(display.inkCount(0, 0, 64, 1), 0);
  CHECK(display.anyInk(0, 1, 64, 1));
  CHECK_EQ(display.inkCount(0, 10, 64, 54), 0);
}

TEST_CASE("a name answers a design and a size is not a name") {
  // The designs the library ships answer their own names, and a name nobody holds answers nothing.
  CHECK_EQ(findFamily("ttyp0")->count, 3);
  CHECK_EQ(findFamily("ttyp0")->cuts[0], &Ttyp011);
  CHECK_EQ(findFamily("nobody"), nullptr);

  // A cut is not a name of its own: a caller that wants one cut of a design registers it as a
  // family of one, which is the only table a name comes from besides the designs.
  CHECK_EQ(findFamily("ttyp016"), nullptr);
  CHECK_EQ(findFamily("profont10"), nullptr);
}

TEST_CASE("a caller registers a family and its cuts come back ordered") {
  // Static, because the module keeps the cuts' addresses for the rest of the run.
  static const Font small = probeFace(2);
  static const Font large = probeFace(8);
  const Font* cuts[] = {&large, &small};  // written tallest first on purpose

  // An alias may be registered before the family it names: it answers nothing until a family holds
  // the name, and a name an alias points at cannot become another alias in the meantime.
  REQUIRE(registerFamilyAlias("probe text", "probe design"));
  CHECK_EQ(findFamily("probe text"), nullptr);
  CHECK_FALSE(registerFamilyAlias("probe design", "ttyp0"));

  REQUIRE(registerFamily("probe design", cuts, 2));
  const Family* family = findFamily("probe design");
  REQUIRE(family != nullptr);
  CHECK_EQ(findFamily("probe text"), family);
  CHECK_EQ(strcmp(family->name, "probe design"), 0);
  CHECK_EQ(family->count, 2);
  CHECK_EQ(family->cuts[0], &small);
  CHECK_EQ(family->cuts[1], &large);

  // A registered design is a name like any other: its shortest cut is what a name gives a leaf that
  // has no box, and its faces measure as they say rather than as the Adafruit cell does.
  CHECK_EQ(family->cuts[0], &small);
  const Size ink = measureText(family->cuts[1], "A", 1);
  CHECK_EQ(ink.width, 2);
  CHECK_EQ(ink.height, 8);
}

TEST_CASE("an alias answers the family it names") {
  // A generic name answers a shipped design and a caller's family alike, and a lookup through it is
  // the lookup through the family's own name.
  REQUIRE(registerFamilyAlias("body", "ttyp0"));
  REQUIRE(registerFamilyAlias("clock", "probe design"));
  CHECK_EQ(findFamily("body"), findFamily("ttyp0"));
  CHECK_EQ(findFamily("clock"), findFamily("probe design"));
  CHECK_EQ(resolve(findFamily("body"), 45).face, resolve(findFamily("ttyp0"), 45).face);
}

TEST_CASE("a second registration moves an alias to another family") {
  REQUIRE(registerFamilyAlias("body", "x11"));
  CHECK_EQ(findFamily("body"), findFamily("x11"));
  CHECK_EQ(findFamily("body")->cuts[0], findFamily("x11")->cuts[0]);
}

TEST_CASE("an alias is refused a name, a target or a slot the module cannot hold") {
  // Three aliases are held by the cases above, so five of the eight are free.
  CHECK_FALSE(registerFamilyAlias(nullptr, "ttyp0"));
  CHECK_FALSE(registerFamilyAlias("", "ttyp0"));
  CHECK_FALSE(registerFamilyAlias("body", nullptr));
  CHECK_FALSE(registerFamilyAlias("body", ""));
  CHECK_FALSE(registerFamilyAlias("a name too long to hold", "ttyp0"));
  CHECK_FALSE(registerFamilyAlias("body", "a family name too long"));

  // A name stands for one thing: a family's is not an alias's to take, and a family takes no name
  // an alias holds. The family table still has slots here, so the second refusal is the name.
  CHECK_FALSE(registerFamilyAlias("ttyp0", "x11"));
  CHECK_FALSE(registerFamilyAlias("probe design", "ttyp0"));
  static const Font probe = probeFace(2);
  const Font* one[] = {&probe};
  CHECK_FALSE(registerFamily("body", one, 1));

  // An alias names a family, not another alias, and not itself.
  CHECK_FALSE(registerFamilyAlias("spare", "body"));
  CHECK_FALSE(registerFamilyAlias("self", "self"));

  REQUIRE(registerFamilyAlias("spare", "ttyp0"));
  REQUIRE(registerFamilyAlias("spare2", "ttyp0"));
  REQUIRE(registerFamilyAlias("spare3", "ttyp0"));
  REQUIRE(registerFamilyAlias("spare4", "ttyp0"));
  REQUIRE(registerFamilyAlias("spare5", "ttyp0"));
  CHECK_FALSE(registerFamilyAlias("spare6", "ttyp0"));
}

TEST_CASE("a family is refused a name, a cut or a size the module cannot hold") {
  // Two cases and no more register a family: the table is four entries and this one fills it, so
  // that the last refusal below is the table being full rather than a name being taken.
  static const Font probe = probeFace(2);
  const Font* one[] = {&probe};

  CHECK_FALSE(registerFamily(nullptr, one, 1));
  CHECK_FALSE(registerFamily("", one, 1));
  CHECK_FALSE(registerFamily("no cuts", nullptr, 1));
  CHECK_FALSE(registerFamily("no count", one, 0));
  const Font* withNull[] = {&probe, nullptr};
  CHECK_FALSE(registerFamily("a null cut", withNull, 2));

  // A name stands for one thing, so a family takes no name a design or another family already
  // answers, and a size is not a name at all.
  CHECK_FALSE(registerFamily("ttyp0", one, 1));
  CHECK_FALSE(registerFamily("adafruit", one, 1));
  CHECK_FALSE(registerFamily("probe design", one, 1));

  // More cuts than an entry holds is a refusal rather than a truncation, and so is a name that will
  // not fit the entry's own copy of it.
  const Font* tooMany[FamilyCuts + 1] = {};
  for (int at = 0; at <= FamilyCuts; at++)
    tooMany[at] = &probe;
  CHECK_FALSE(registerFamily("too many", tooMany, FamilyCuts + 1));
  CHECK_FALSE(registerFamily("a name too long to hold", one, 1));

  // Three more fill the four the table holds, and the next is refused for want of a slot.
  REQUIRE(registerFamily("second", one, 1));
  REQUIRE(registerFamily("third", one, 1));
  REQUIRE(registerFamily("fourth", one, 1));
  CHECK_FALSE(registerFamily("fifth", one, 1));
}

}  // namespace gardener::gfx::test
