#pragma once

#include "Font.h"

namespace gardener::gfx {

// The faces the library ships are declared beside it, in fonts/: the Adafruit 5x7, all 256 byte
// values in a five-by-eight cell six pixels apart, and the cuts of ttyp0, ProFont and X11. They
// answer to the names written down with them, and the Adafruit family is what a page that names
// nothing draws in, so everything below is a question about those faces and the names they answer
// to.

// The face a page's font name stands for: a face the library ships, the face a caller registered
// under the name, and the default family's first cut for "" and for a name the module does not
// hold. A name nobody registered is a page drawing in the face it would have drawn in without the
// name, rather than a picture that does not arrive, so there is nothing here to check.
const Font* find(const char* name);

// --- the families -------------------------------------------------------------
// A design's cuts, shortest first: a page names the design and a height picks the cut, so a page
// written against one size of a design still draws when a shorter or a taller one is fitted. A
// family of one cut is a design with one size, which is what the Adafruit face is.
struct Family {
  const char* name;
  const Font* const* cuts;
  uint8_t count;
};

// The family a caller that names none draws and measures in. It is the Adafruit one unless
// something sets it, and setting nullptr puts that back: a caller that sets nothing draws as it
// always has.
const Family* defaultFamily();
void setDefaultFamily(const Family* family);

// The family a draw or a measurement uses: the one it was given, or the default when it was given
// none. Every leaf and every scene resolves a family here rather than repeating the fallback, so one
// answer serves the picture and the ink it measures with.
const Family* familyOr(const Family* family);

// The face a rasteriser draws through: the one it was given, or the first cut of the default
// family, which is what a caller that names nothing draws in. It is the null guard under the family
// fallback, not a way for a caller to name a face.
const Font* faceOr(const Font* face);

// The family a page's family name stands for: one the library ships, one a caller registered, and
// the default family for "" and for a name the module does not hold, so a page that names a family
// nobody has draws as it does without one rather than not drawing.
const Family* findFamily(const char* name);

// A family a caller registers, beside the ones the library ships: a name, and the cuts of the
// design it names. The entry is the module's own, so a caller keeps neither the array nor the faces
// alive for it, and the module orders the cuts by line height as it registers them, shortest first,
// so the order a caller writes is not a promise it can break.
constexpr int RegisteredFamilies = 4;
constexpr int FamilyCuts = 8;

// Registers `cuts` under `name`. False for an empty or too long name, a name the module already
// answers — a shipped design, a shipped face's, or a caller's — no cuts, more cuts than the table
// holds, or a cut that is null. A caller that has one face registers it as a family of one cut, so
// there is one table, one budget and one set of refusals for every name a caller adds.
constexpr int NameLength = 16;

bool registerFamily(const char* name, const Font* const* cuts, uint8_t count);

}  // namespace gardener::gfx