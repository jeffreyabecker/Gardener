#include "Fonts.h"

#include <string.h>

#include "fonts/AdafruitFont.h"
#include "fonts/Battery19.h"
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
const Font* const BatteryCuts[] = {&Battery19};

const Family Ttyp0Family = {"ttyp0", Ttyp0Cuts, 3};
const Family ProFontFamily = {"profont", ProFontCuts, 4};
const Family X11Family = {"x11", X11Cuts, 4};
const Family BatteryFamily = {"battery", BatteryCuts, 1};

const Family* const ShippedFamilies[] = {&AdafruitFamily, &Ttyp0Family, &ProFontFamily, &X11Family,
                                        &BatteryFamily};

// Copies a name into an entry, or answers false when it does not fit. The length is checked before
// anything is written, so a refused name leaves the entry it was aimed at as it was.
bool copyName(char* into, const char* from) {
  const size_t length = strlen(from);
  if (length >= static_cast<size_t>(NameLength)) return false;
  memcpy(into, from, length + 1);
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

// One generic name a caller registered: the entry's own copy of the alias and of the family's name.
// The target is a name rather than a pointer because an alias may be registered before its family
// and follows whatever family holds that name when it is looked up.
struct AliasEntry {
  bool named = false;
  char alias[NameLength] = {};
  char family[NameLength] = {};
};

AliasEntry aliases[RegisteredAliases];

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

// The entry an alias name is held by, or nullptr. Only aliases are searched here: an alias names a
// family, and a name a family holds is refused to an alias at registration.
const AliasEntry* namedAlias(const char* name) {
  if (name == nullptr || name[0] == '\0') return nullptr;
  for (const AliasEntry& entry : aliases) {
    if (entry.named && strcmp(entry.alias, name) == 0) return &entry;
  }
  return nullptr;
}

// Whether a name already stands for something: a design the library ships, a family a caller
// registered, or a generic name a caller pointed at one.
bool nameTaken(const char* name) {
  return namedFamily(name) != nullptr || namedAlias(name) != nullptr;
}

// Whether some alias already names this family, whether or not a family holds the name yet: a name
// an alias points at is kept for the family it stands for and cannot become an alias itself.
bool aliasTarget(const char* name) {
  for (const AliasEntry& entry : aliases) {
    if (entry.named && strcmp(entry.family, name) == 0) return true;
  }
  return false;
}

}  // namespace

const Family AdafruitFamily = {"adafruit", AdafruitCuts, 1};

namespace {

// The design a leaf that names no family draws in: the Adafruit family until a composition sets
// another, and never null, so the resolver has nothing to check.
const Family* chosenFamily = &AdafruitFamily;

}  // namespace

const Family* defaultFamily() { return chosenFamily; }

void setDefaultFamily(const Family* family) {
  chosenFamily = family != nullptr ? family : &AdafruitFamily;
}

const Family* findFamily(const char* name) {
  if (const Family* family = namedFamily(name)) return family;
  const AliasEntry* alias = namedAlias(name);
  return alias != nullptr ? namedFamily(alias->family) : nullptr;
}

bool registerFamily(const char* name, const Font* const* cuts, uint8_t count) {
  if (name == nullptr || name[0] == '\0' || cuts == nullptr) return false;
  if (count == 0 || count > FamilyCuts) return false;
  if (nameTaken(name)) return false;
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

bool registerFamilyAlias(const char* alias, const char* family) {
  if (alias == nullptr || alias[0] == '\0' || family == nullptr || family[0] == '\0') return false;
  if (strcmp(alias, family) == 0) return false;
  // A family's name is not an alias's to take, and an alias names a family rather than another
  // alias: a chain would make one name answer by way of a second, which nothing here needs.
  if (namedFamily(alias) != nullptr) return false;
  if (namedAlias(family) != nullptr) return false;

  // A generic name already pointed at one design is moved to another rather than refused: that is
  // the indirection a composition changes when it chooses a different family for the app's word.
  for (AliasEntry& entry : aliases) {
    if (entry.named && strcmp(entry.alias, alias) == 0) return copyName(entry.family, family);
  }

  if (aliasTarget(alias)) return false;

  for (AliasEntry& entry : aliases) {
    if (entry.named) continue;
    if (!copyName(entry.alias, alias)) return false;
    if (!copyName(entry.family, family)) return false;
    entry.named = true;
    return true;
  }
  return false;
}

}  // namespace gardener::gfx