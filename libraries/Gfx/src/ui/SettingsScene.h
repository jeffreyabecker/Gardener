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

// One row of a settings list: the key its value is stored under, the label it prints, the default
// its declaration gives and the scene that edits it. A row is a class of its own per setting kind —
// a number row builds a NumberScene, a bool row a BoolScene — so a kind states its rules once and
// the list knows none of them.
//
// The value in force is the list's, not the row's: the row answers only what a setting starts as
// where the set the list is handed carries nothing for it.
class SettingsRow {
 public:
  virtual ~SettingsRow() = default;

  virtual const String& key() const = 0;
  virtual const String& label() const = 0;

  // The default the row's declaration gives, in whatever type the setting holds — a bool, a number,
  // a key. The list starts a key its set does not carry from this, and nothing at all is an empty
  // any, which the list prints as `--`.
  virtual std::any defaultValue() const = 0;

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
  // The rows the list shows and the set the document holds. The list holds the rows by pointer, as
  // the stack holds its scenes: whoever built a row keeps it alive. A key the set carries takes its
  // value from it, an empty any included — a setting the document holds nothing for reads `--` —
  // and a key it does not falls back to the row's default. The set that goes in is the set the list
  // opened on, which is what Back and Cancel put back.
  SettingsScene(GestureQueue& gestures, Ux& ux, const String& title,
                const std::vector<SettingsRow*>& rows, const SettingValues& values);

  // The values the settings hold, keyed by setting key: what the set was handed, what an editor
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
