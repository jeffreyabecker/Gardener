// The fonts module's two tables of names: the faces a caller registers and the families, which are
// a design's cuts ordered shortest first. The shipped names are the drawing cases' because they are
// what a page draws with; what a caller adds is here.

#include <string.h>

#include <doctest.h>

#include "suite.h"

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

TEST_CASE("the two designs answer to a name per rung and a family per design") {
  // Every rung is a face a page can name, and its line box is the one its source declares — not the
  // size in its name, which is the design's nominal em.
  const struct {
    const char* name;
    const Font* face;
    uint8_t line;
  } rungs[] = {
      {"profont10", &ProFont10, 8},   {"profont12", &ProFont12, 10},
      {"profont17", &ProFont17, 14},  {"profont29", &ProFont29, 24},
      {"x11_5x7", &X11Cell5x7, 7},    {"x11_6x10", &X11Cell6x10, 9},
      {"x11_7x14", &X11Cell7x14, 12}, {"x11_10x20", &X11Cell10x20, 17},
  };
  for (const auto& rung : rungs) {
    CHECK_EQ(find(rung.name), rung.face);
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
  text(Box::whole(), "A", &X11Cell10x20, 1)->draw(box);
  CHECK(display.anyInk(0, 0, 8, 1));
  CHECK_EQ(display.inkCount(0, 13, 64, 11), 0);
}

TEST_CASE("a height resolves to the tallest cut it can hold and the scale it spends") {
  const Family* ttyp0 = findFamily("ttyp0");

  // A height under the shortest cut is that cut at one scale: the answer is the smallest thing the
  // family has rather than nothing.
  Fit fit = resolve(*ttyp0, 5);
  CHECK_EQ(fit.face, &Ttyp011);
  CHECK_EQ(fit.scale, 1);

  // Each cut at one scale where the band holds it exactly, and the tallest cut that fits at all
  // where the band is between two of them: 24 rows is the 22 px design at one scale, not the 16 px
  // one at one.
  CHECK_EQ(resolve(*ttyp0, 10).face, &Ttyp011);
  CHECK_EQ(resolve(*ttyp0, 13).face, &Ttyp016);
  CHECK_EQ(resolve(*ttyp0, 17).face, &Ttyp022);
  CHECK_EQ(resolve(*ttyp0, 24).face, &Ttyp022);
  CHECK_EQ(resolve(*ttyp0, 24).scale, 1);

  // A band that holds two of a cut's lines is spent at two, and one that holds three at three.
  fit = resolve(*ttyp0, 34);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 2);
  fit = resolve(*ttyp0, 64);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 3);

  // Two designs answer the same band differently, because the ladder is the design's.
  CHECK_EQ(resolve(*findFamily("x11"), 45).face, &X11Cell10x20);
  CHECK_EQ(resolve(*findFamily("x11"), 45).scale, 2);
  CHECK_EQ(resolve(*findFamily("profont"), 45).face, &ProFont29);
  CHECK_EQ(resolve(*findFamily("profont"), 45).scale, 1);
}

TEST_CASE("a width narrows the cut and the scale the height chose") {
  const Family* ttyp0 = findFamily("ttyp0");

  // The home page's timer is five characters in a cell 64 across and 48 down: two lines of the 22 px
  // design are 110 pixels wide, so the timer is that design at one scale.
  Fit fit = resolve(*ttyp0, "4m30s", 64, 48);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 1);

  // The same string across a whole page keeps the taller answer.
  fit = resolve(*ttyp0, "4m30s", 128, 48);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 2);

  // A box too narrow for the tallest cut steps down through its scales and then to the next cut:
  // five characters in 40 pixels is the 16 px design at one, which is exactly 40 wide.
  fit = resolve(*ttyp0, "123.4", 40, 45);
  CHECK_EQ(fit.face, &Ttyp016);
  CHECK_EQ(fit.scale, 1);

  // A string no cut fits is drawn at the height's own answer, and the box clips it.
  fit = resolve(*ttyp0, "123.4", 10, 45);
  CHECK_EQ(fit.face, &Ttyp022);
  CHECK_EQ(fit.scale, 2);
}

TEST_CASE("a caller registers a family and its cuts come back ordered") {
  const Font small = probeFace(2);
  const Font large = probeFace(8);
  const Font* cuts[] = {&large, &small};  // written tallest first on purpose

  REQUIRE(registerFamily("probe design", cuts, 2));
  const Family* family = findFamily("probe design");
  REQUIRE(family != nullptr);
  CHECK_EQ(strcmp(family->name, "probe design"), 0);
  CHECK_EQ(family->count, 2);
  CHECK_EQ(family->cuts[0], &small);
  CHECK_EQ(family->cuts[1], &large);
}

TEST_CASE("a family is refused a name, a cut or a size the module cannot hold") {
  // Two cases and no more register a family: the table is four entries and this one fills it, so
  // that the last refusal below is the table being full rather than a name being taken.
  const Font probe = probeFace(2);
  const Font* one[] = {&probe};

  CHECK_FALSE(registerFamily(nullptr, one, 1));
  CHECK_FALSE(registerFamily("", one, 1));
  CHECK_FALSE(registerFamily("no cuts", nullptr, 1));
  CHECK_FALSE(registerFamily("no count", one, 0));
  const Font* withNull[] = {&probe, nullptr};
  CHECK_FALSE(registerFamily("a null cut", withNull, 2));

  // A name stands for one thing, so a family takes no name a face answers and no name a family
  // already does, and a face takes none of a family's either.
  CHECK_FALSE(registerFamily("ttyp0", one, 1));
  CHECK_FALSE(registerFamily("builtin", one, 1));
  CHECK_FALSE(registerFamily("ttyp016", one, 1));
  CHECK_FALSE(registerFamily("probe design", one, 1));
  CHECK_FALSE(registerFont("ttyp0", &probe));
  CHECK_FALSE(registerFont("probe design", &probe));

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
