#include "Fonts.h"

#include <string.h>

#include "fonts/AdafruitFont.h"
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

// The families the library ships: each a name and the cuts of one design, shortest first. The
// Adafruit family is a design of one cut, and it is the default a leaf that names no family draws
// in. A design's ladder is the rungs this panel's bands can tell apart, not every size the design
// has.
const Font* const AdafruitCuts[] = {&AdafruitFont};
const Font* const Ttyp0Cuts[] = {&Ttyp011, &Ttyp016, &Ttyp022};
const Font* const ProFontCuts[] = {&ProFont10, &ProFont12, &ProFont17, &ProFont29};
const Font* const X11Cuts[] = {&X11Cell5x7, &X11Cell6x10, &X11Cell7x14, &X11Cell10x20};

const Family Ttyp0Family = {"ttyp0", Ttyp0Cuts, 3};
const Family ProFontFamily = {"profont", ProFontCuts, 4};
const Family X11Family = {"x11", X11Cuts, 4};

const Family* const ShippedFamilies[] = {&AdafruitFamily, &Ttyp0Family, &ProFontFamily, &X11Family};

// One family per shipped face, so a name that is a face's answers a design of one cut: a page
// written against a size keeps drawing, and a caller has one kind of name to learn. The Adafruit
// face is the design above, which is a family of one cut already, so it is not named twice.
const Font* const Ttyp011Face[] = {&Ttyp011};
const Font* const Ttyp016Face[] = {&Ttyp016};
const Font* const Ttyp022Face[] = {&Ttyp022};
const Font* const ProFont10Face[] = {&ProFont10};
const Font* const ProFont12Face[] = {&ProFont12};
const Font* const ProFont17Face[] = {&ProFont17};
const Font* const ProFont29Face[] = {&ProFont29};
const Font* const X11Cell5x7Face[] = {&X11Cell5x7};
const Font* const X11Cell6x10Face[] = {&X11Cell6x10};
const Font* const X11Cell7x14Face[] = {&X11Cell7x14};
const Font* const X11Cell10x20Face[] = {&X11Cell10x20};

const Family ShippedFaceFamilies[] = {
    {"ttyp011", Ttyp011Face, 1},      {"ttyp016", Ttyp016Face, 1},
    {"ttyp022", Ttyp022Face, 1},      {"profont10", ProFont10Face, 1},
    {"profont12", ProFont12Face, 1},  {"profont17", ProFont17Face, 1},
    {"profont29", ProFont29Face, 1},  {"x11_5x7", X11Cell5x7Face, 1},
    {"x11_6x10", X11Cell6x10Face, 1}, {"x11_7x14", X11Cell7x14Face, 1},
    {"x11_10x20", X11Cell10x20Face, 1},
};

// The family a shipped face's name stands for, or nullptr when the module ships no face by it.
const Family* shippedFaceFamily(const char* name) {
  for (const Family& family : ShippedFaceFamilies) {
    if (strcmp(family.name, name) == 0) return &family;
  }
  return nullptr;
}

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

// The family a name stands for, or nullptr when nothing answers to it: a shipped design, a shipped
// face's name as a family of one cut, which is how a page that names a size keeps drawing, or a
// family a caller registered.
const Family* namedFamily(const char* name) {
  if (name == nullptr || name[0] == '\0') return nullptr;
  for (const Family* family : ShippedFamilies) {
    if (strcmp(family->name, name) == 0) return family;
  }
  if (const Family* face = shippedFaceFamily(name)) return face;
  for (const FamilyEntry& entry : families) {
    if (entry.named && strcmp(entry.name, name) == 0) return &entry.family;
  }
  return nullptr;
}

}  // namespace

const Family AdafruitFamily = {"adafruit", AdafruitCuts, 1};

const Family* findFamily(const char* name) { return namedFamily(name); }

bool registerFamily(const char* name, const Font* const* cuts, uint8_t count) {
  if (name == nullptr || name[0] == '\0' || cuts == nullptr) return false;
  if (count == 0 || count > FamilyCuts) return false;
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