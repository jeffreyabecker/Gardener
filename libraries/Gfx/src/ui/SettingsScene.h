#pragma once

#include <stdint.h>

#include <any>
#include <map>
#include <memory>
#include <vector>

#include <Arduino.h>

#include "ui/Scene.h"
#include "ui/Ux.h"

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

  // A value of this kind as the row prints it, or `--` when it holds nothing: a bool is ON or OFF,
  // a number its digits, a duration its compact text.
  virtual String text(const std::any& value) const = 0;

  // The scene that edits `value`, or nullptr for a setting with no editor, where Forward opens
  // nothing. The stack owns what it is handed, so the row builds one and lets it go.
  virtual std::unique_ptr<Scene> scene(const std::any& value) = 0;
};

// The list a routine's settings are edited in, from settings_edit.be: the heading, one row per
// setting printed as its label and the value it holds now, and the row the keys are on inverted.
// Up and Down move the row, wrapping at the ends; Forward opens the row's editor and takes what it
// hands back; OK keeps the set it settled on and closes; Back and Cancel put the set back as it was
// opened and close.
//
// The list is what pops itself, so it holds the stack that pushed it. A row's value is written
// under the row's key when its editor pops, and an editor that was cancelled hands back the value
// it opened on, so a cancel needs no path of its own.
class SettingsScene : public Scene {
 public:
  SettingsScene(GestureQueue& gestures, Ux& ux, const String& title);

  // The rows the list shows, handed to it before it draws. The list holds them by pointer, as the
  // stack holds its scenes: whoever built a row keeps it alive. Their current values seed the set
  // the list carries and the set it opened on.
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
  void move(int8_t direction);
  void open();

  String title_;
  Ux& ux_;
  std::vector<SettingsRow*> rows_;
  SettingValues values_;
  SettingValues opened_;
  int16_t selected_ = 0;
};

}  // namespace gardener::gfx::ui
