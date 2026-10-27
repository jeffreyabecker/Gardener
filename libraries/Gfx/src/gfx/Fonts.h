#pragma once

#include "gfx/Font.h"

namespace gardener::gfx::fonts {

// The compiled faces the library ships. The names match the panel's font table where the same face
// exists there; the oblique, italic and serif variants add their own. The data lives in Fonts.cpp,
// so a caller that only needs a pointer does not carry its own copy of it. A face is read-only, so
// the pointers are const; nullptr passed to Text still selects BuiltinFont.

// --- sans --------------------------------------------------------------------
extern const Font* const Sans9;
extern const Font* const SansBold9;
extern const Font* const SansOblique9;
extern const Font* const SansBoldOblique9;
extern const Font* const Sans12;
extern const Font* const SansBold12;
extern const Font* const SansOblique12;
extern const Font* const SansBoldOblique12;
extern const Font* const Sans18;
extern const Font* const SansBold18;
extern const Font* const SansOblique18;
extern const Font* const SansBoldOblique18;
extern const Font* const Sans24;
extern const Font* const SansBold24;
extern const Font* const SansOblique24;
extern const Font* const SansBoldOblique24;

// --- mono --------------------------------------------------------------------
extern const Font* const Mono9;
extern const Font* const MonoBold9;
extern const Font* const MonoOblique9;
extern const Font* const MonoBoldOblique9;
extern const Font* const Mono12;
extern const Font* const MonoBold12;
extern const Font* const MonoOblique12;
extern const Font* const MonoBoldOblique12;
extern const Font* const Mono18;
extern const Font* const MonoBold18;
extern const Font* const MonoOblique18;
extern const Font* const MonoBoldOblique18;
extern const Font* const Mono24;
extern const Font* const MonoBold24;
extern const Font* const MonoOblique24;
extern const Font* const MonoBoldOblique24;

// --- serif -------------------------------------------------------------------
extern const Font* const Serif9;
extern const Font* const SerifBold9;
extern const Font* const SerifItalic9;
extern const Font* const SerifBoldItalic9;
extern const Font* const Serif12;
extern const Font* const SerifBold12;
extern const Font* const SerifItalic12;
extern const Font* const SerifBoldItalic12;
extern const Font* const Serif18;
extern const Font* const SerifBold18;
extern const Font* const SerifItalic18;
extern const Font* const SerifBoldItalic18;
extern const Font* const Serif24;
extern const Font* const SerifBold24;
extern const Font* const SerifItalic24;
extern const Font* const SerifBoldItalic24;

// --- tiny and symbols --------------------------------------------------------
extern const Font* const TomThumbFace;
extern const Font* const PicopixelFace;
extern const Font* const Symbol12;
extern const Font* const Symbol18;
extern const Font* const Symbol24;

}  // namespace gardener::gfx::fonts
