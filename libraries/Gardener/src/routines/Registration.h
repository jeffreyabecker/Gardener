#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <Arduino.h>

#include "ui/Drawing.h"
#include "ui/SettingsRows.h"

namespace gardener {

class Library;
class Logger;

// What a launch hands a routine's page: the key the routine was launched under, and one clean
// settings map read through the declaration and repaired by the rule.
struct RoutineActivation {
  String routineKey;
  gfx::ui::SettingValues values;
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
  // The rule that binds two settings to one another, answering the reason it refuses a set or
  // nullptr when every set is one a run can use.
  using Rule = std::function<const char*(const gfx::ui::SettingValues&)>;

  // Builds the routine's page from the activation a launch hands it.
  using Factory = std::function<std::unique_ptr<gfx::ui::Scene>(const RoutineActivation&)>;

  // `settings` is the declaration, in the order the settings list shows it, and the record owns it
  // from here, so the list and the store reach the rows by pointer. The title is the label the
  // browser draws; a record given none is known by its key.
  Registration(const String& routineKey, const String& title,
               std::vector<std::unique_ptr<gfx::ui::SettingsRow>> settings,
               Rule rule = nullptr, Factory factory = nullptr, gfx::ui::Glyph glyph = nullptr);

  Registration(const Registration&) = delete;
  Registration& operator=(const Registration&) = delete;
  Registration(Registration&&) = default;
  Registration& operator=(Registration&&) = default;

  const String& routineKey() const { return routineKey_; }
  const String& title() const { return title_; }
  const std::vector<gfx::ui::SettingsRow*>& settings() const { return settings_; }
  const Factory& factory() const { return factory_; }
  const gfx::ui::Glyph& glyph() const { return glyph_; }

  // The values the rule accepts. While the rule refuses the set, the settings are put back to their
  // defaults in the order the list shows them, each one tried in turn, and the reason that was
  // applied is answered for the caller to draw. The label the reason is logged under is the
  // routine's key for the global section and the section name for an alternate. Nothing here touches
  // a file: the caller decides what to write.
  const char* repair(const String& section, gfx::ui::SettingValues& values, Logger& logger) const;

  // One clean map: every setting the record declares, read from the section it names of the
  // routine's document, held to its kind, then repaired by the rule so the set is one a run accepts.
  // The document is opened once. An alternate also carries its section's `routine_title` on the map.
  // A record that declares nothing begins with an empty map, without opening a file.
  gfx::ui::SettingValues loadSettings(Library& library, Logger& logger,
                                      const String& section) const;

  // A settled set written back: the rule is applied first, so what lands is a set a run accepts, and
  // a setting the map says nothing about is written as nothing, which reads back as the default.
  // Every key goes into one document and the document is written once. False, writing nothing, when
  // the record declares no settings.
  bool saveSettings(Library& library, Logger& logger, const String& section,
                    gfx::ui::SettingValues& values) const;

 private:
  const char* problem(const gfx::ui::SettingValues& values) const;

  String routineKey_;
  String title_;
  std::vector<std::unique_ptr<gfx::ui::SettingsRow>> owned_;
  // The declaration as the list and the store take it, over the rows this record owns.
  std::vector<gfx::ui::SettingsRow*> settings_;
  Rule rule_;
  Factory factory_;
  gfx::ui::Glyph glyph_;
};

}  // namespace gardener
