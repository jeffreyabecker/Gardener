// The fonts module's two tables of names: the faces a caller registers and the families, which are
// a design's cuts ordered shortest first. The shipped names are the drawing cases' because they are
// what a page draws with; what a caller adds is here.

#include <string.h>

#include <doctest.h>

#include <Gfx.h>

namespace gardener::gfx::test {
namespace {

// A probe face of one column and the rows asked for, drawn from a blank bitmap: enough for a
// family's order, which reads only the line box.
Font probeFace(uint8_t rows) {
  static const uint8_t blank[256] = {};
  return {Font::Kind::Fixed, blank, 1, rows, nullptr, 0, 0, static_cast<uint8_t>(rows - 1), 1};
}

}  // namespace

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
  for (int at = 0; at <= FamilyCuts; at++) tooMany[at] = &probe;
  CHECK_FALSE(registerFamily("too many", tooMany, FamilyCuts + 1));
  CHECK_FALSE(registerFamily("a name too long to hold", one, 1));

  // Three more fill the four the table holds, and the next is refused for want of a slot.
  REQUIRE(registerFamily("second", one, 1));
  REQUIRE(registerFamily("third", one, 1));
  REQUIRE(registerFamily("fourth", one, 1));
  CHECK_FALSE(registerFamily("fifth", one, 1));
}

}  // namespace gardener::gfx::test
