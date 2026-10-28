#pragma once

#include <Arduino.h>

#include <vector>

#include "storage/Config.h"
#include "hal/Library.h"
#include "ui/SettingsRows.h"

namespace gardener {

// Where a routine's settings document lives: one document per routine, its aliases as named sections
// of it.
constexpr const char* RoutinesConfigFolder = "/cfg/routines/";

// A routine's settings, as its document keeps them: the values for the keys a list's rows declare, an
// alternate's `routine_title` beside them, and the settled set written back.
//
// The document is the parent routine's — /cfg/routines/<parentRoutineKey>.ini — because an alias is a
// named section of the routine it shares a factory with. The routine's own set is the global section;
// an alias's set is the section its key names. A key the routine's file does not carry starts at the
// declaration's default, and a key the rows do not declare — an alias's routine_title among them — is
// carried through a write untouched.
//
// Each call reads the document once and writes it once: nothing read here is kept between a save and
// the next page, so a document the wire edited in between is what the next list opens on.
class RoutineSettingStore {
 public:
  // `library` is where the documents live: the device's filesystem, or the host's in-memory image.
  explicit RoutineSettingStore(Library& library);

  // One value per key the rows declare: the document's text read as the row's kind, and the row's
  // default where the section does not carry the key or carries it empty. An alias's set also
  // carries its section's `routine_title`, which no row declares but which the browser shows.
  gfx::ui::SettingValues load(const String& parentRoutineKey, const String& routineKey,
                              const std::vector<gfx::ui::SettingsRow*>& rows) const;

  // The settled set written under the keys the rows declare, then the document once; a key the set
  // does not carry is left as the document has it. A value that is nothing is written as an empty one,
  // which reads back as the default. False when the store refused the write, and true without a write
  // when nothing changed.
  bool save(const String& parentRoutineKey, const String& routineKey,
            const std::vector<gfx::ui::SettingsRow*>& rows,
            const gfx::ui::SettingValues& values);

 private:
  Library& library_;
};

}  // namespace gardener
