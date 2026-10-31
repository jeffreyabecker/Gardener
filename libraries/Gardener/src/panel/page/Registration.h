#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <Arduino.h>

#include "panel/page/SettingsRows.h"

namespace gardener {
inline namespace core { class Library; }

namespace panel::page {

// What a launch hands a routine's page: the key the routine was launched under, and the settings map
// read through the declaration.
struct RoutineActivation {
  String routineKey;
  SettingValues values;
};

// A routine's registration: the declaration the browser lists and the settings list edits, and
// nothing else. The key is the routine's identity — what the record is found by and what its
// document is called, `/cfg/routines/<routine_key>.ini` — so its set lives in that document's
// global section.
//
// The alternates are not here. A record is declared before its document is scanned, so a record
// that carried its alternates could not be built; the registry keeps them.
class Registration {
 public:
  // Builds the routine's page from the activation a launch hands it.
  using Factory = std::function<std::unique_ptr<Scene>(const RoutineActivation&)>;

  // `settings` is the declaration, in the order the settings list shows it, and the record owns it
  // from here, so the list and the store reach the rows by pointer. The title is the label the
  // browser draws; a record given none is known by its key. The glyph is the hex digits that name
  // the packed bitmap, empty for a record that names none — the browser hands it to `screen.icon`,
  // which reads it when it draws the icon.
  Registration(const String& routineKey, const String& title,
               std::vector<std::unique_ptr<SettingsRow>> settings,
               Factory factory = nullptr, String glyph = String());

  Registration(const Registration&) = delete;
  Registration& operator=(const Registration&) = delete;
  Registration(Registration&&) = default;
  Registration& operator=(Registration&&) = default;

  const String& routineKey() const { return routineKey_; }
  const String& title() const { return title_; }
  const std::vector<SettingsRow*>& settings() const { return settings_; }
  const Factory& factory() const { return factory_; }
  const String& glyph() const { return glyph_; }

  // Every setting the record declares, read from the section it names of the routine's document and
  // held to its kind. The document is opened once. An alternate also carries its section's
  // `routine_title` on the map. A record that declares nothing begins with an empty map, without
  // opening a file.
  SettingValues loadSettings(Library& library, const String& section) const;

  // A settled set written back: a setting the map says nothing about is written as nothing, which
  // reads back as the default. Every key goes into one document and the document is written once.
  // False, writing nothing, when the record declares no settings.
  bool saveSettings(Library& library, const String& section,
                    SettingValues& values) const;

 private:
  String routineKey_;
  String title_;
  std::vector<std::unique_ptr<SettingsRow>> owned_;
  // The declaration as the list and the store take it, over the rows this record owns.
  std::vector<SettingsRow*> settings_;
  Factory factory_;
  String glyph_;
};

}  // namespace panel::page

}  // namespace gardener
