#pragma once

#include "Font.h"

namespace gardener::gfx {

// The faces the library ships are declared beside it, in fonts/: the Adafruit 5x7, all 256 byte
// values in a five-by-eight cell six pixels apart, the cuts of ttyp0, ProFont and X11, and the
// battery marks. They answer to the names written down with them, and a design's name answers the
// family of its cuts, so a caller names a design and a box chooses the size. A name is all a caller
// has to know.

// --- the families -------------------------------------------------------------
// A design's cuts, shortest first: a page names the design and a height picks the cut, so a page
// written against one size of a design still draws when a shorter or a taller one is fitted. A
// family of one cut is a design with one size, which is what the Adafruit face is.
struct Family {
  const char* name;
  const Font* const* cuts;
  uint8_t count;
};

// The family a caller's name stands for: one the library ships, a family a caller registered, or an
// alias a caller pointed at one. Null for a name nobody holds — or an alias whose family nobody
// holds yet — so a lookup answers what the module holds and nothing more; a leaf that names no
// family draws in the default family, which is the resolver's own business and no caller's.
const Family* findFamily(const char* name);

// The design a leaf or a measurement that names no family draws in. It is the Adafruit family until
// a composition sets another, and nullptr puts that back, so a composition names the panel's design
// once instead of at every leaf. Nothing reads it back: the resolver applies it where the fit is
// made, so a caller sets it and a leaf draws.
void setDefaultFamily(const Family* family);

// A family a caller registers, beside the ones the library ships: a name, and the cuts of the
// design it names. The entry is the module's own, so a caller keeps neither the array nor the faces
// alive for it, and the module orders the cuts by line height as it registers them, shortest first,
// so the order a caller writes is not a promise it can break.
constexpr int RegisteredFamilies = 4;
constexpr int FamilyCuts = 8;

// Registers `cuts` under `name`. False for an empty or too long name, a name the module already
// answers — a shipped design, a caller's family, or a caller's alias — no cuts, more cuts than the
// table holds, or a cut that is null. A caller that has one face registers it as a family of one
// cut, so there is one table, one budget and one set of refusals for every name a caller adds.
constexpr int NameLength = 16;

bool registerFamily(const char* name, const Font* const* cuts, uint8_t count);

// --- the aliases -------------------------------------------------------------
// A generic name a page can use for a design: an alias answers whatever family its target names, so
// one page names the same word while a composition chooses the design behind it. The target is a
// family's name and may be registered before or after the alias; until a family holds it the alias
// answers nothing, as a name no family holds does. An alias names a family, not another alias, and
// the two share one namespace: a family takes no name an alias holds.
constexpr int RegisteredAliases = 8;

// Registers `alias` as a name for `family`. True when it was registered or repointed, so a second
// registration of the same alias moves it to the new family. False for an empty or too long name, an
// alias a family already answers, a target that is another alias or the alias itself, or a full
// table.
bool registerFamilyAlias(const char* alias, const char* family);

}  // namespace gardener::gfx