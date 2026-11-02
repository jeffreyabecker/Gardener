#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <Arduino.h>

#include "panel/settings/SettingsRows.h"

namespace gardener {

class Library;

// What a launch hands a routine's page: the key the routine was launched under, and the settings map
// read through the declaration.
struct RoutineActivation {
  String routineKey;
  SettingValues values;
};

// What a section of a routine's document says about the record it copies: the title it shows, the
// glyph it draws in place of the record's, the settings it keeps off the list (`<key>:hidden`) and
// the values it pins (`<key>:constant`), the latter already read through the rows' kinds. An empty
// title or glyph leaves the record's own.
struct SectionOverrides {
  String title;
  String glyph;
  std::vector<String> hidden;
  SettingValues constants;
};

// A routine's registration: the declaration the browser lists and the settings list edits, and
// nothing else. The key is the routine's identity — what the record is found by and what its
// document is called, `/cfg/routines/<routine_key>.ini` — so its set lives in that document's
// global section.
//
// An alternate is a copy of the record, made by copyAs(): it takes the section's key as its own, the
// section's title and glyph, its declaration made again with the settings the section hides or pins,
// and the record's factory. It reads and writes in the record's document, in the section its own key
// names.
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
               std::vector<std::unique_ptr<SettingsRow>> settings, Factory factory = nullptr,
               String glyph = String());

  Registration(const Registration&) = delete;
  Registration& operator=(const Registration&) = delete;
  Registration(Registration&&) = default;
  Registration& operator=(Registration&&) = default;

  // The record made again for a section of its document: its rows copied, the ones the section names
  // hidden left out of the settings list, the values it pins carried over whatever a document or a
  // caller states, and its own title and glyph unless the section gives others.
  Registration copyAs(const String& routineKey, const SectionOverrides& overrides) const;

  const String& routineKey() const { return routineKey_; }
  // The document the values live in: the registration's own key, or the record's when it was copied
  // from one.
  const String& documentKey() const { return documentKey_; }
  const String& title() const { return title_; }
  const std::vector<SettingsRow*>& settings() const { return settings_; }
  // The values the section pinned, by setting key: what a launch carries whatever its caller stated.
  const SettingValues& constants() const { return constants_; }
  const Factory& factory() const { return factory_; }
  const String& glyph() const { return glyph_; }

  // Every setting the record declares, read from its section of the document and held to its kind.
  // The document is opened once. An alternate also carries its section's `routine_title` on the map,
  // and its set is the section its own key names rather than the document's global one; a setting
  // the section pins answers the pinned value. A record that declares nothing begins with an empty
  // map, without opening a file.
  SettingValues loadSettings(Library& library) const;

  // A settled set written back: a setting the map says nothing about is written as nothing, which
  // reads back as the default, and a setting the section pinned is written as the pinned value
  // whatever the map holds. Every key goes into one document and the document is written once.
  // False, writing nothing, when the record declares no settings.
  bool saveSettings(Library& library, SettingValues& values) const;

 private:
  String routineKey_;
  String documentKey_;
  String title_;
  std::vector<std::unique_ptr<SettingsRow>> owned_;
  // The declaration as the list and the store take it, over the rows this record owns.
  std::vector<SettingsRow*> settings_;
  // The values a section pinned, by setting key: what the record reads and launches with whatever
  // the document or the caller says.
  SettingValues constants_;
  Factory factory_;
  String glyph_;
};

}  // namespace gardener