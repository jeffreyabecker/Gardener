#pragma once

#include <Arduino.h>

#include <vector>

#include "core/Library.h"
#include "storage/Config.h"
#include "storage/SettingKind.h"

namespace gardener {

// Where a routine's settings document lives: one document per routine.
constexpr const char* RoutinesConfigFolder = "/cfg/routines/";

// A routine's settings, as its document keeps them: the values for the keys a list's rows declare,
// and the settled set written back.
//
// The document is /cfg/routines/<routineKey>.ini and the values live in its global section. A key the
// file does not carry starts at the declaration's default, and a key the rows do not declare is
// carried through a write untouched.
//
// Each call reads the document once and writes it once: nothing read here is kept between a save and
// the next page, so a document the wire edited in between is what the next list opens on.
class RoutineSettingStore {
 public:
  // `library` is where the documents live: the device's filesystem, or the host's in-memory image.
  explicit RoutineSettingStore(Library& library);

  // One value per key the rows declare: the document's text read as the row's kind, and the row's
  // default where the document does not carry the key or carries it empty.
  SettingValues load(const String& routineKey, const std::vector<const SettingKind*>& rows) const;

  // The settled set written under the keys the rows declare, then the document once; a key the set
  // does not carry is left as the document has it. A value that is nothing is written as an empty one,
  // which reads back as the default. False when the store refused the write, and true without a write
  // when nothing changed.
  bool save(const String& routineKey, const std::vector<const SettingKind*>& rows,
            const SettingValues& values);

 private:
  Library& library_;
};

}  // namespace gardener
