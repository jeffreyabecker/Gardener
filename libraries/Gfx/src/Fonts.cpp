#include "Fonts.h"

#include <string.h>

#include "fonts/BuiltinFont.h"
#include "fonts/ProFont10.h"
#include "fonts/ProFont12.h"
#include "fonts/ProFont17.h"
#include "fonts/ProFont29.h"
#include "fonts/Ttyp011.h"
#include "fonts/Ttyp016.h"
#include "fonts/Ttyp022.h"
#include "fonts/X11Cell10x20.h"
#include "fonts/X11Cell5x7.h"
#include "fonts/X11Cell6x10.h"
#include "fonts/X11Cell7x14.h"

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
    {"builtin", &BuiltinFont},  {"ttyp011", &Ttyp011},      {"ttyp016", &Ttyp016},
    {"ttyp022", &Ttyp022},      {"profont10", &ProFont10},  {"profont12", &ProFont12},
    {"profont17", &ProFont17},  {"profont29", &ProFont29},  {"x11_5x7", &X11Cell5x7},
    {"x11_6x10", &X11Cell6x10}, {"x11_7x14", &X11Cell7x14}, {"x11_10x20", &X11Cell10x20},
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
// built-in face is a family of one cut, and it is the answer a name nobody holds is given. A
// design's ladder is the rungs this panel's bands can tell apart, not every size the design has.
const Font* const BuiltinCuts[] = {&BuiltinFont};
const Font* const Ttyp0Cuts[] = {&Ttyp011, &Ttyp016, &Ttyp022};
const Font* const ProFontCuts[] = {&ProFont10, &ProFont12, &ProFont17, &ProFont29};
const Font* const X11Cuts[] = {&X11Cell5x7, &X11Cell6x10, &X11Cell7x14, &X11Cell10x20};

const Family BuiltinFamily = {"builtin", BuiltinCuts, 1};
const Family Ttyp0Family = {"ttyp0", Ttyp0Cuts, 3};
const Family ProFontFamily = {"profont", ProFontCuts, 4};
const Family X11Family = {"x11", X11Cuts, 4};

const Family* const ShippedFamilies[] = {&BuiltinFamily, &Ttyp0Family, &ProFontFamily, &X11Family};

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

// One family a caller registered: the entry's own name, its own run of cuts and the Family a reader
// is handed, which points at those two rather than at the caller's array.
struct FamilyEntry {
  bool named = false;
  char name[NameLength] = {};
  const Font* cuts[FamilyCuts] = {};
  Family family = {};
};

FamilyEntry families[RegisteredFamilies];

// The face a name stands for among the ones a caller registered, or nullptr when none holds it.
const Font* registeredFace(const char* name) {
  for (const Entry& entry : entries) {
    if (entry.named && strcmp(entry.name, name) == 0) return entry.font;
  }
  return nullptr;
}

// The family a name stands for, or nullptr when nothing answers to it. findFamily is this with the
// built-in as the answer, and the two callers that must tell a name nobody holds from the built-in
// ask this instead.
const Family* namedFamily(const char* name) {
  if (name == nullptr || name[0] == '\0') return nullptr;
  for (const Family* family : ShippedFamilies) {
    if (strcmp(family->name, name) == 0) return family;
  }
  for (const FamilyEntry& entry : families) {
    if (entry.named && strcmp(entry.name, name) == 0) return &entry.family;
  }
  return nullptr;
}

}  // namespace

const Font* defaultFace() { return chosenFace; }

void setDefaultFace(const Font* face) { chosenFace = face != nullptr ? face : &BuiltinFont; }

const Font* faceOr(const Font* face) { return face != nullptr ? face : chosenFace; }

bool registerFont(const char* name, const Font* font) {
  if (name == nullptr || name[0] == '\0' || font == nullptr) return false;
  if (shippedFace(name) != nullptr) return false;
  if (registeredFace(name) != nullptr) return false;
  if (namedFamily(name) != nullptr) return false;
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
  if (const Font* registered = registeredFace(name)) return registered;
  return defaultFace();
}

const Family* findFamily(const char* name) {
  const Family* family = namedFamily(name);
  return family != nullptr ? family : &BuiltinFamily;
}

bool registerFamily(const char* name, const Font* const* cuts, uint8_t count) {
  if (name == nullptr || name[0] == '\0' || cuts == nullptr) return false;
  if (count == 0 || count > FamilyCuts) return false;
  if (shippedFace(name) != nullptr) return false;
  if (registeredFace(name) != nullptr) return false;
  if (namedFamily(name) != nullptr) return false;
  for (uint8_t at = 0; at < count; at++) {
    if (cuts[at] == nullptr) return false;
  }
  for (FamilyEntry& entry : families) {
    if (entry.named) continue;
    if (!copyName(entry.name, name)) return false;
    for (uint8_t at = 0; at < count; at++) {
      uint8_t where = at;
      while (where > 0 && lineHeight(*entry.cuts[where - 1]) > lineHeight(*cuts[at])) {
        entry.cuts[where] = entry.cuts[where - 1];
        where--;
      }
      entry.cuts[where] = cuts[at];
    }
    entry.family.name = entry.name;
    entry.family.cuts = entry.cuts;
    entry.family.count = count;
    entry.named = true;
    return true;
  }
  return false;
}

}  // namespace gardener::gfx