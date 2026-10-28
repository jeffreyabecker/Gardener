#pragma once

#include <stdint.h>

#include <any>
#include <functional>
#include <memory>
#include <vector>

#include <Arduino.h>

#include "ui/ChoiceScene.h"
#include "ui/NumberScene.h"
#include "ui/SettingsScene.h"

namespace gardener::gfx::ui {

// The rows a settings list is built from, one class per value kind: each prints the kind's value
// the way the list prints a row and builds the scene that edits it. A row carries the rules of its
// kind — a number's step and bounds, a choice's entries — the default it was handed, which the list
// falls back to for a key the set it is given does not carry, and the check its declaration gave.
// The key is the setting's: what an editor hands back is written under it.

// --- the bool ---------------------------------------------------------------

// The row of an on/off setting: OFF or ON as the list prints it, and the switch the bool scene
// draws to change it. A nullable setting's nothing prints `--` and opens the switch on nothing.
class BoolRow : public SettingsRow {
 public:
  BoolRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
          bool defaultValue, bool nullable = false, Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override { return default_; }
  String text(const std::any& value) const override;
  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any default_;
  bool nullable_;
};

// --- the choice -------------------------------------------------------------

// The row of a setting that stores a key out of a fixed list: the title of the entry showing as the
// list prints it, and the choice scene over the entries the row was handed. `forward` is the page's
// own work on the entry showing — a jingle row is built with one that sounds the entry — and a row
// given none drops Forward.
class ChoiceRow : public SettingsRow {
 public:
  ChoiceRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
            const String& defaultValue, const std::vector<ChoiceItem>& items,
            bool nullable = false, std::function<void(String& current)> forward = nullptr,
            Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override { return default_; }
  String text(const std::any& value) const override;
  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any default_;
  std::vector<ChoiceItem> items_;
  bool nullable_;
  std::function<void(String& current)> forward_;
};

// --- the duration -----------------------------------------------------------

// The row of a duration held in whole seconds: its compact text — `1m30s` — as the list prints it,
// and the two-field scene that steps the minutes and the seconds.
class DurationRow : public SettingsRow {
 public:
  DurationRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
              int32_t defaultValue, int32_t step, int32_t min, int32_t max,
              bool nullable = false, Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override { return default_; }
  String text(const std::any& value) const override;
  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any default_;
  int32_t step_;
  int32_t min_;
  int32_t max_;
  bool nullable_;
};

// --- the number -------------------------------------------------------------

// The row of a number: the digits as the list prints them, through the setting's own formatter
// where it reads in its units and as `%g` where it passed none, and the number scene over its step
// and bounds. A kind that edits this number its own way extends the row and builds its scene from
// the same rules. A default of nothing is NAN, the number the scene holding none already is, which
// the row answers as an empty any and prints `--`.
class NumberRow : public SettingsRow {
 public:
  NumberRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
            float defaultValue, float step, float min, float max, bool nullable,
            NumberFormatter format = nullptr, uint8_t decimals = 2,
            Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override;
  String text(const std::any& value) const override;
  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;

 protected:
  // What a kind that edits its number its own way reads: the pressure row builds the unit-aware
  // scene over the same step, bounds and formatter.
  GestureQueue& gestures_;
  Ux& ux_;
  String label_;
  float step_;
  float min_;
  float max_;
  bool nullable_;
  NumberFormatter format_;

 private:
  String key_;
  float default_;
  uint8_t decimals_;
};

}  // namespace gardener::gfx::ui
