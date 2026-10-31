#include "Fonts.h"

#include <string.h>

#include "fonts/BuiltinFont.h"
#include "fonts/Ttyp011.h"
#include "fonts/Ttyp016.h"
#include "fonts/Ttyp022.h"

namespace gardener::gfx::fonts {

namespace {

// What a caller that names no face draws in. The built-in 5x7 is what such a caller has always
// drawn in, so it is where this starts; setDefaultFace is the only writer, and it never stores a
// null, so a reader has nothing to check.
const Font* chosenFace = &BuiltinFont;

// One name a caller registered and the face it stands for. The name is the entry's own copy, so the
// caller's string need not outlive the call, and `named` is how a slot that was never filled reads.
struct Entry {
  bool named = false;
  char name[NameLength] = {};
  const Font* font = nullptr;
};

Entry entries[RegisteredFaces];

// Copies a name into an entry, or answers false when it does not fit.
bool copyName(char* into, const char* from) {
  int at = 0;
  for (; from[at] != '\0'; at++) {
    if (at + 1 >= NameLength) return false;
    into[at] = from[at];
  }
  into[at] = '\0';
  return true;
}

}  // namespace

const Font* defaultFace() { return chosenFace; }

void setDefaultFace(const Font* face) { chosenFace = face != nullptr ? face : &BuiltinFont; }

const Font* faceOr(const Font* face) { return face != nullptr ? face : chosenFace; }

bool registerFont(const char* name, const Font* font) {
  if (name == nullptr || name[0] == '\0' || font == nullptr) return false;
  if (strcmp(name, "builtin") == 0) return false;
  for (const Entry& entry : entries) {
    if (entry.named && strcmp(entry.name, name) == 0) return false;
  }
  for (Entry& entry : entries) {
    if (entry.named) continue;
    if (!copyName(entry.name, name)) return false;
    entry.font = font;
    entry.named = true;
    return true;
  }
  return false;
}

const Font* find(const char* name) {
  if (name == nullptr || name[0] == '\0') return defaultFace();
  if (strcmp(name, "builtin") == 0) return &BuiltinFont;
  for (const Entry& entry : entries) {
    if (entry.named && strcmp(entry.name, name) == 0) return entry.font;
  }
  return defaultFace();
}

namespace {

// The faces the library compiles in are registered as the module comes up, so a page or a record can
// name one without the composition registering anything. The table is declared above this, so the
// order between the two is the file's own.
struct Shipped {
  Shipped() {
    registerFont("ttyp011", &Ttyp011);
    registerFont("ttyp016", &Ttyp016);
    registerFont("ttyp022", &Ttyp022);
  }
} shippedFaces;

}  // namespace

}  // namespace gardener::gfx::fonts
