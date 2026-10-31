#pragma once

#include "Font.h"

namespace gardener::gfx {

// The library ships one face, the built-in 5x7: all 256 byte values, each in a five-by-eight cell,
// six pixels apart, declared in fonts/BuiltinFont.h. It is what a page that names no font draws in,
// so everything below is a question about that one face and the name it answers to.

// The face a caller that names none draws and measures in. It is the built-in 5x7 unless something
// sets it, and setting nullptr puts that back: a caller that sets nothing draws as it always has.
const Font* defaultFace();
void setDefaultFace(const Font* face);

// The face a draw or a measurement uses: the one it was given, or the default when it was given
// none. Every reader resolves a face here rather than repeating the fallback, so one answer serves
// the draw and the ink it measures with.
const Font* faceOr(const Font* face);

// The face a page's font name stands for: the face a caller registered under it, the built-in 5x7
// for "" and "builtin", and the default face for a name the module does not hold. A name nobody
// registered is a page drawing in the face it would have drawn in without the name, rather than a
// picture that does not arrive, so there is nothing here to check.
const Font* find(const char* name);

// --- registering a face ------------------------------------------------------
// A face the library does not hold: a page, a host tool or a driver hands the module a name and a
// Font, and every draw or measurement that names it resolves through find. The table is static, so
// a registration costs no heap, and the name is copied into the entry, so a caller keeps nothing
// alive for it.
constexpr int RegisteredFaces = 8;
constexpr int NameLength = 16;

// Registers `font` under `name`. False when the table is full, the name is empty or too long, the
// name is one the module already answers — "builtin" and "" are the built-in 5x7 — or the face is
// null. A name stands for one face, so registering a name twice is refused rather than made a rule
// about which face wins.
bool registerFont(const char* name, const Font* font);

}  // namespace gardener::gfx