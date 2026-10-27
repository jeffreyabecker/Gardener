#pragma once

#include <stdint.h>

#include <any>
#include <map>
#include <vector>

#include <Arduino.h>

#include "ui/Scene.h"

namespace gardener::gfx::ui {

// The values a settings list holds, keyed by setting key: what an editor wrote back and what OK
// hands on.
using SettingValues = std::map<String, std::any>;

// One row of a settings list: the key its value is stored under, the label it prints, the value it
// shows now and the scene that edits it. A row is a class of its own per setting kind — a number
// row builds a NumberScene, a bool row a BoolScene — so a kind states its rules once and the list
// knows none of them.
class SettingsRow {
 public:
  virtual ~SettingsRow() = default;

  virtual const String& key() const = 0;
  virtual const String& label() const = 0;

  // The value the row shows now, in whatever type the setting holds — a bool, a number, a key.
  virtual std::any currentValue() const = 0;

  // The scene that edits that value, or nullptr for a setting with no editor, where Forward opens
  // nothing. The scene has to outlive the push, so whoever builds it keeps it.
  virtual Scene* scene() = 0;
};

// The list a routine's settings are edited in, from settings_edit.be: a heading and one row per
// setting, with Up and Down moving the row, Forward opening the row's editor, OK handing back the
// settings as they were edited and Back or Cancel handing back the set as it was opened.
//
// Stubbed. The rows it holds, the heading it draws, the values and the row the keys are on are
// here, and the two overrides are named and empty: nothing steps the row yet, nothing opens an
// editor from it, and nothing draws. The rows are drawn here rather than through a list, since the
// list is not being ported.
class SettingsScene : public Scene {
 public:
  SettingsScene(GestureQueue& gestures, const String& title);

  // The rows the list shows, handed to it before it draws. The list holds them by pointer, as the
  // stack holds its scenes: whoever built a row keeps it alive. Their current values seed the
  // values the list carries, so a row's key reads back what the row was set to.
  void set(const std::vector<SettingsRow*>& rows);

  // The values the settings hold, keyed by setting key: what the rows were set to, what an editor
  // wrote back and what OK hands on.
  SettingValues& values() { return values_; }

  const String& title() const { return title_; }
  int16_t selected() const { return selected_; }

 protected:
  bool handle(const char* gesture) override;
  void drawPage(Canvas& canvas) override;

 private:
  String title_;
  std::vector<SettingsRow*> rows_;
  SettingValues values_;
  int16_t selected_ = 0;
};

}  // namespace gardener::gfx::ui
