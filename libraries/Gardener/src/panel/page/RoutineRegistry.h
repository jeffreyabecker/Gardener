#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include <Arduino.h>

#include "panel/page/Registration.h"
#include "panel/page/Scene.h"

namespace gardener {

inline namespace core {
class Library;
class Logger;
}

namespace panel::page {

// One row the browser draws: the routine's glyph as the image wrote it, the title the row shows and
// the key the row stands for. An alternate carries its own title and its own key, and its record's
// glyph.
struct RoutineRow {
  String glyph;
  String title;
  String routineKey;
};

// The routines the panel offers: their records in the order they registered, and the alternates
// each one's document named.
//
// The key is the routine's identity — what the record is found by and what its document is called —
// and an alternate is a second key that shares its record's declaration, glyph and page but
// carries its own title and its own settings set, which lives in the section the key names of the
// record's document. The alternates are the registry's because a record is declared before its
// document is scanned, and nothing outside the registry reads them.
//
// The registry is built once, at boot, and used on the panel task only: a page draws its rows, opens
// a record's settings and launches its page through it, and nothing else reaches it.
class RoutineRegistry {
 public:
  RoutineRegistry(Library& library, Logger& logger);

  RoutineRegistry(const RoutineRegistry&) = delete;
  RoutineRegistry& operator=(const RoutineRegistry&) = delete;

  // One routine registered. A key already registered keeps its place; the record it had, its
  // alternates included, is replaced by this one. The document is scanned for alternates here: every
  // named section whose key nothing holds becomes one, and a name that is taken is skipped with one
  // line. False, changing nothing, for a key that is not a plain word.
  bool add(Registration registration);

  // Every registered key, in registration order, each alternate following its record.
  std::vector<String> keys() const;

  // The browser's rows: one per record and one per alternate, each alternate after its record.
  std::vector<RoutineRow> rows() const;

  // The record a key names: an alternate resolves to its record, so every lookup serves both. An
  // alternate is looked for before a record, so a key that is both answers the alternate's.
  const Registration* entry(const String& routineKey) const;

  // The record a routine's own key names, or nullptr. An alternate is not looked for here.
  const Registration* registration(const String& routineKey) const;

  // Where a record's own key sits in registration order, or -1 when it is not registered.
  int16_t indexOf(const String& routineKey) const;

  // The section a key's set lives in: the global section of the record's document for the record's
  // own key, and the key itself for an alternate.
  String section(const String& routineKey) const;

  // The label the browser draws, which an alternate takes from its section. Empty for a key that is
  // not registered.
  String title(const String& routineKey) const;

  // The routine's glyph, as the image wrote it, or nullptr for a key that is not registered or a
  // record that names none.
  const String* icon(const String& routineKey) const;

  // The settings a routine can edit, as its record declares them, in the order the list shows them;
  // nullptr when the key is not registered.
  const std::vector<SettingsRow*>* settingsRows(const String& routineKey) const;

  // The set a routine begins with, read through its record in the key's section. An unregistered key
  // begins with an empty map.
  SettingValues loadSettings(const String& routineKey) const;

  // A settled set written back through the record. False when the key is not registered or the
  // record declares nothing.
  bool saveSettings(const String& routineKey, SettingValues& values) const;

  // The page a key launches: the record the key names, and the activation its factory is handed —
  // the key as the caller named it and the caller's set held to the record's declaration, so a
  // setting the set leaves out or one a row refuses begins at its default. Nothing for a key nobody
  // registered, a record that names no page, or a page whose init refused the activation. The page
  // is not put on a stack here: the caller decides where it goes.
  std::unique_ptr<Scene> activate(const String& routineKey,
                                  const SettingValues& values) const;

 private:
  struct Alias {
    String routineKey;
    String title;
  };

  struct Entry {
    Registration registration;
    std::vector<Alias> aliases;
  };

  String documentPath(const String& routineKey) const;

  Library& library_;
  Logger& logger_;
  std::vector<Entry> entries_;
};

}  // namespace panel::page
}  // namespace gardener
