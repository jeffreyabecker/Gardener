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

// The family a name stands for, or nullptr when nothing answers to it: one of the designs the
// library ships, or a family a caller registered under it. A size is not a name — a page names a
// design and the box it draws in chooses the cut — so a caller that wants one cut of a design
// registers it as a family of one.
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