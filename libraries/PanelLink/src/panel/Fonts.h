#pragma once

#include <Adafruit_GFX.h>

namespace gardener {
namespace fonts {

// The compiled faces a Berry routine can name. The font data lives in Fonts.cpp, so a caller that
// only needs the pointer does not carry its own copy of it.
extern const GFXfont* const Sans9;
extern const GFXfont* const Sans12;
extern const GFXfont* const Sans18;
extern const GFXfont* const Sans24;
extern const GFXfont* const SansBold9;
extern const GFXfont* const SansBold12;
extern const GFXfont* const SansBold18;
extern const GFXfont* const SansBold24;
extern const GFXfont* const Mono9;
extern const GFXfont* const Mono12;
extern const GFXfont* const Mono18;
extern const GFXfont* const Mono24;
extern const GFXfont* const MonoBold9;
extern const GFXfont* const MonoBold12;
extern const GFXfont* const MonoBold18;
extern const GFXfont* const MonoBold24;
extern const GFXfont* const TomThumbFace;
extern const GFXfont* const PicopixelFace;

// The UI symbol faces, vendored from gilesp1729/FontCollection in panel/fonts/ under MPL-2.0 with
// its license beside them. Codes 0-31 carry the icons - play, pause, the arrows, a tick, a cross -
// and no letters at all, so they are for the panel's own glyphs rather than a routine's text, and
// they are deliberately not names find() answers.
extern const GFXfont* const Symbol12;
extern const GFXfont* const Symbol18;
extern const GFXfont* const Symbol24;

// The face for a routine's font name; nullptr is the built-in 5x7. False when the name is not one
// of the faces above, which is a mistake in the routine rather than a face to fall back to.
bool find(const char* name, const GFXfont*& face);

}  // namespace fonts
}  // namespace gardener
