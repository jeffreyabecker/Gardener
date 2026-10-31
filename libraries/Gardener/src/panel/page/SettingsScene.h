#pragma once

#include <math.h>
#include <stdint.h>

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <Arduino.h>

#include "panel/page/ChoiceScene.h"
#include "storage/SettingKind.h"
#include "panel/page/EditorScene.h"
#include "panel/page/Scene.h"
#include "panel/page/Ux.h"

namespace gardener::panel::page {

// The values a list holds and the declaration a row was built from are the document's vocabulary,
// in `storage/SettingKind.h`; a row is that and the page's half of it.
using storage::SettingDeclaration;
using storage::SettingValues;

class SettingsRow;

// The check a row's declaration gave it: the declaration, the set in force and the value an editor
// is about to keep, answering whether that value is accepted and, when it is not, the text that
// refuses it. The editor holds the same check closed over the declaration and the set, which is
// CurriedValidator.
using Validator = std::function<std::pair<bool, String>(
    const std::vector<SettingsRow*>& settings, const SettingValues& values,
    const std::any& proposed)>;

// One row of a settings list: the key its value is stored under, the label it prints, the default
// its declaration gives, the check it carries and the editor that edits it. A row is a class of its
// own per setting kind — a number row builds a NumberScene, a bool row a BoolScene — so a kind
// states its rules once and the list knows none of them.
//
// The value in force is the list's, not the row's: the row answers only what a setting starts as
// where the set the list is handed carries nothing for it. The check is the row's, but it needs the
// whole declaration and the set it edits to answer, which is what the list curries it with.
class SettingsRow : public storage::SettingKind {
 public:
  virtual ~SettingsRow() = default;

  virtual const String& label() const = 0;

  // A value of this kind as the row prints it, or `--` when it holds nothing: a bool is ON or OFF,
  // a number its digits, a duration its compact text.
  virtual String text(const std::any& value) const = 0;

  // The editor that edits `value`, or nullptr for a setting with no editor, where Forward opens
  // nothing. Every value editor is the page EditorScene draws, so the list can hand what a row
  // builds the check that goes with it. The stack owns what it is handed, so the row builds one and
  // lets it go.
  virtual std::unique_ptr<EditorScene> scene(const std::any& value) = 0;

  // The value held as this kind holds it, from a caller outside the list: a number is taken as the
  // kind's own numeric type, so a whole literal written for a pressure reads as the pressure, and
  // nothing is every kind's nothing. False, leaving the value as it was, for a value of a kind this
  // setting cannot hold — which is what a caller handing a set to the store is refused on rather
  // than cast to.
  virtual bool accept(std::any& value) const = 0;

  // The row's check closed over the declaration and the set in force, which is the form the editor
  // it builds holds; nothing for a row whose declaration gave it no check. The check borrows the
  // row, the declaration and the set, so whoever holds it keeps all three alive.
  CurriedValidator check(const std::vector<SettingsRow*>& settings,
                         const SettingValues& values) const;

 protected:
  // The check the row's declaration gave it, which a kind passes up from its constructor. A row
  // given none accepts every value.
  explicit SettingsRow(Validator validator = nullptr) : validator_(std::move(validator)) {}

 private:
  Validator validator_;
};

// The list a routine's settings are edited in, from settings_edit.be: the heading, one row per
// setting printed as its label and the value it holds now, and the row the keys are on inverted.
// Up and Down move the row, wrapping at the ends; Forward opens the row's editor and takes what it
// hands back; OK keeps the set it settled on and closes; Back and Cancel put the set back as it was
// opened and close.
//
// The list is what pops itself, so it holds the stack that pushed it. A row's value is written
// under the row's key when its editor pops, and an editor that was cancelled hands back the value
// it opened on, so a cancel needs no path of its own. The list is also the only holder of the
// declaration and the set in force, so the check it hands a row's editor is curried here.
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

  // What the list answers when it pops: the set it settled on. Back and Cancel put the opened set
  // back before closing, so this is never a set the list abandoned.
  std::any value() const override { return std::any(values_); }

  const String& title() const { return title_; }
  int16_t selected() const { return selected_; }

 protected:
  bool handle(const char* gesture) override;
  void drawPage(gfx::Canvas& canvas) override;

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

}  // namespace gardener::panel::page
