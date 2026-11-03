#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include <Arduino.h>

#include "panel/page/Registration.h"
#include "panel/page/Scene.h"

namespace gardener {
class Library;
class Logger;

// One row the browser draws: the routine's glyph as the image wrote it, the title the row shows and
// the key the row stands for. An alternate carries its own title and its own key, and its record's
// glyph.
struct RoutineRow {
  String glyph;
  String title;
  String routineKey;
};

// The routines the panel offers, in the order they registered.
//
// The key is a registration's identity — what it is found by and what its settings list edits — and
// an alternate is a second key that shares its record's declaration and page but carries its own
// title, its own glyph and its own settings set, which lives in the section the key names of the
// record's document. The registry makes one by copying the record it has just registered: the
// document names its alternates as named sections, and each section whose key nothing holds becomes
// a registration of its own, placed directly behind the record it was copied from. A section may
// also name the glyph it draws as `routine_icon`, keep a setting off the list as
// `<key>:hidden=true`, pin one to a value as `<key>:constant=<value>`, and state the floor and
// ceiling it moves within as `<key>:min=<value>` and `<key>:max=<value>`.
//
// The registry is built once, at boot, and used on the panel task only: a page draws its rows, opens
// a record's settings and launches its page through it, and nothing else reaches it.
class RoutineRegistry {
 public:
  RoutineRegistry(Library& library, Logger& logger);

  RoutineRegistry(const RoutineRegistry&) = delete;
  RoutineRegistry& operator=(const RoutineRegistry&) = delete;

  // One routine registered. A key already registered keeps its place; the record it had, its copies
  // included, is replaced by this one. The document is scanned for alternates here: every named
  // section whose key nothing holds becomes a copy of this record, carrying the title, glyph, hidden
  // settings, bounds and pinned values the section names, and a name that is taken is skipped with
  // one line. False, changing nothing, for a key that is not a plain word.
  bool add(Registration registration);

  // Every registered key, in registration order, each copy following the record it was made from.
  std::vector<String> keys() const;

  // The browser's rows: one per registration, in the order keys() lists them.
  std::vector<RoutineRow> rows() const;

  // The registration a key names, or nullptr. Every key names one, an alternate being a registration
  // of its own, so a lookup serves both.
  const Registration* registration(const String& routineKey) const;

  // The label the browser draws. Empty for a key that is not registered.
  String title(const String& routineKey) const;

  // The routine's glyph, as the image wrote it, or nullptr for a key that is not registered or a
  // record that names none.
  const String* icon(const String& routineKey) const;

  // The settings a routine can edit, as its registration declares them, in the order the list shows
  // them; nullptr when the key is not registered.
  const std::vector<SettingConfig*>* settingsRows(const String& routineKey) const;

  // The set a routine begins with, read through its registration. An unregistered key begins with an
  // empty map.
  SettingValues loadSettings(const String& routineKey) const;

  // A settled set written back through the registration. False when the key is not registered or the
  // record declares nothing.
  bool saveSettings(const String& routineKey, SettingValues& values) const;

  // The page a key launches: the registration the key names, and the activation its factory is
  // handed — the key as the caller named it and the caller's set held to the record's declaration, so
  // a setting the set leaves out or one a row refuses begins at its default. Nothing for a key nobody
  // registered, a record that names no page, or a page whose init refused the activation. The page is
  // not put on a stack here: the caller decides where it goes.
  std::unique_ptr<Scene> activate(const String& routineKey, const SettingValues& values) const;

 private:
  String documentPath(const String& routineKey) const;
  int16_t indexOf(const String& routineKey) const;

  Library& library_;
  Logger& logger_;
  std::vector<Registration> entries_;
};

}  // namespace gardener