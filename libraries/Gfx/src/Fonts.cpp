#include "Fonts.h"

#include <string.h>

#include "fonts/BuiltinFont.h"
#include "fonts/Ttyp011.h"
#include "fonts/Ttyp016.h"
#include "fonts/Ttyp022.h"

namespace gardener::gfx {

namespace {

// What a caller that names no face draws in. The built-in 5x7 is what such a caller has always
// drawn in, so it is where this starts; setDefaultFace is the only writer, and it never stores a
// null, so a reader has nothing to check.
const Font* chosenFace = &BuiltinFont;

// One face the library ships and the name it answers to. The library writes these down rather than
// registering them, so the registration table below is a caller's and its slots are not spent on
// faces every build carries anyway.
struct NamedFace {
  const char* name;
  const Font* face;
};

const NamedFace ShippedFaces[] = {
    {"builtin", &BuiltinFont},
    {"ttyp011", &Ttyp011},
    {"ttyp016", &Ttyp016},
    {"ttyp022", &Ttyp022},
};

// The face a shipped name stands for, or nullptr when the module ships no face by that name.
const Font* shippedFace(const char* name) {
  for (const NamedFace& shipped : ShippedFaces) {
    if (strcmp(shipped.name, name) == 0) return shipped.face;
  }
  return nullptr;
}

// One name a caller registered and the face it stands for. The name is the entry's own copy, so the
// caller's string need not outlive the call, and `named` is how a slot that was never filled reads.
struct Entry {
  bool named = false;
  char name[NameLength] = {};
  const Font* font = nullptr;
};

Entry entries[RegisteredFaces];

// The families the library ships: each a name and the cuts of one design, shortest first. The
// built-in face is a family of one cut, and it is the answer a name nobody holds is given.
const Font* const BuiltinCuts[] = {&BuiltinFont};
const Font* const Ttyp0Cuts[] = {&Ttyp011, &Ttyp016, &Ttyp022};

const Family BuiltinFamily = {"builtin", BuiltinCuts, 1};
const Family Ttyp0Family = {"ttyp0", Ttyp0Cuts, 3};

const Family* const ShippedFamilies[] = {&BuiltinFamily, &Ttyp0Family};

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
  if (shippedFace(name) != nullptr) return false;
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
  if (const Font* shipped = shippedFace(name)) return shipped;
  for (const Entry& entry : entries) {
    if (entry.named && strcmp(entry.name, name) == 0) return entry.font;
  }
  return defaultFace();
}

const Family* findFamily(const char* name) {
  if (name != nullptr && name[0] != '\0') {
    for (const Family* family : ShippedFamilies) {
      if (strcmp(family->name, name) == 0) return family;
    }
  }
  return &BuiltinFamily;
}

}  // namespace gardener::gfx