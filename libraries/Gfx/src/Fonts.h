#pragma once

#include "Font.h"

namespace gardener::gfx {

// The faces the library ships are declared beside it, in fonts/: the Adafruit 5x7, all 256 byte
// values in a five-by-eight cell six pixels apart, and the three cuts of ttyp0. They answer to the
// names written down with them, and the Adafruit is what a page that names no font draws in, so
// everything below is a question about those faces and the names they answer to.

// The face a caller that names none draws and measures in. It is the Adafruit 5x7 unless something
// sets it, and setting nullptr puts that back: a caller that sets nothing draws as it always has.
const Font* defaultFace();
void setDefaultFace(const Font* face);

// The face a draw or a measurement uses: the one it was given, or the default when it was given
// none. Every reader resolves a face here rather than repeating the fallback, so one answer serves
// the draw and the ink it measures with.
const Font* faceOr(const Font* face);

// The face a page's font name stands for: a face the library ships, the face a caller registered
// under the name, and the default face for "" and for a name the module does not hold. A name
// nobody registered is a page drawing in the face it would have drawn in without the name, rather
// than a picture that does not arrive, so there is nothing here to check.
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

// The family a page's family name stands for: one the library ships, one a caller registered, and
// the Adafruit for "" and for a name the module does not hold, so a page that names a family nobody
// has draws as it does without one rather than not drawing.
const Family* findFamily(const char* name);

// A family a caller registers, beside the ones the library ships: a name, and the cuts of the
// design it names. The entry is the module's own, so a caller keeps neither the array nor the faces
// alive for it, and the module orders the cuts by line height as it registers them, shortest first,
// so the order a caller writes is not a promise it can break.
constexpr int RegisteredFamilies = 4;
constexpr int FamilyCuts = 8;

// Registers `cuts` under `name`. False for an empty or too long name, a name a face or a family
// already answers, no cuts, more cuts than the table holds, or a cut that is null. A name stands for
// one thing, so a family named for a face, or a face named for a family, is refused.
bool registerFamily(const char* name, const Font* const* cuts, uint8_t count);

// --- registering a face ------------------------------------------------------
// A face the library does not hold: a page, a host tool or a driver hands the module a name and a
// Font, and every draw or measurement that names it resolves through find. The table is static, so
// a registration costs no heap, and the name is copied into the entry, so a caller keeps nothing
// alive for it. The faces the library ships are not in this table: they are written down beside it,
// so a registration is a caller's and the slots are a caller's budget.
constexpr int RegisteredFaces = 8;
constexpr int NameLength = 16;

// Registers `font` under `name`. False when the table is full, the name is empty or too long, the
// name is one the module already answers — a face it ships, or a name a caller registered — or the
// face is null. A name stands for one face, so registering a name twice is refused rather than made
// a rule about which face wins.
bool registerFont(const char* name, const Font* font);

}  // namespace gardener::gfx