#pragma once

#include "Font.h"

namespace gardener::gfx::fonts {

// The compiled faces the library ships, one for each name `find` accepts: the upright sans and mono
// faces at four sizes with the bold weight, and the two tiny faces. A face is read-only, so the
// pointers are const; nullptr passed to Text still selects BuiltinFont. The data lives in Fonts.cpp,
// so a caller that only needs a pointer does not carry its own copy of it.

// --- sans --------------------------------------------------------------------
extern const Font* const Sans9;
extern const Font* const SansBold9;
extern const Font* const Sans12;
extern const Font* const SansBold12;
extern const Font* const Sans18;
extern const Font* const SansBold18;
extern const Font* const Sans24;
extern const Font* const SansBold24;

// --- mono --------------------------------------------------------------------
extern const Font* const Mono9;
extern const Font* const MonoBold9;
extern const Font* const Mono12;
extern const Font* const MonoBold12;
extern const Font* const Mono18;
extern const Font* const MonoBold18;
extern const Font* const Mono24;
extern const Font* const MonoBold24;

// --- tiny --------------------------------------------------------------------
extern const Font* const TomThumbFace;
extern const Font* const PicopixelFace;

// The face for a routine's font name; nullptr is the built-in 5x7. False when the name is not one of
// the named faces, which is a mistake in the routine rather than a face to fall back to.
bool find(const char* name, const Font*& face);

}  // namespace gardener::gfx::fonts
