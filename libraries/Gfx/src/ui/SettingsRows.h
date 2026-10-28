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
// kind — a number's step and bounds, a choice's entries — and the value it was handed, which seeds
// the set the list carries. The key is the setting's: what an editor hands back is written under
// it.

// --- the bool ---------------------------------------------------------------

// The row of an on/off setting: OFF or ON as the list prints it, and the switch the bool scene
// draws to change it. A nullable setting's nothing prints `--` and opens the switch on nothing.
class BoolRow : public SettingsRow {
 public:
  BoolRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label, bool current,
          bool nullable = false);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any currentValue() const override { return current_; }
  String text(const std::any& value) const override;
  std::unique_ptr<Scene> scene(const std::any& value) override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any current_;
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
            const String& current, const std::vector<ChoiceItem>& items, bool nullable = false,
            std::function<void(String& current)> forward = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any currentValue() const override { return current_; }
  String text(const std::any& value) const override;
  std::unique_ptr<Scene> scene(const std::any& value) override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any current_;
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
              int32_t current, int32_t step, int32_t min, int32_t max, bool nullable = false);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any currentValue() const override { return current_; }
  String text(const std::any& value) const override;
  std::unique_ptr<Scene> scene(const std::any& value) override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any current_;
  int32_t step_;
  int32_t min_;
  int32_t max_;
  bool nullable_;
};

// --- the number -------------------------------------------------------------

// The row of a number: the digits as the list prints them, through the setting's own formatter
// where it reads in its units and as `%g` where it passed none, and the number scene over its step
// and bounds. A kind that edits this number its own way extends the row and builds its scene from
// the same rules.
class NumberRow : public SettingsRow {
 public:
  NumberRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label, float current,
            float step, float min, float max, bool nullable, NumberFormatter format = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any currentValue() const override { return current_; }
  String text(const std::any& value) const override;
  std::unique_ptr<Scene> scene(const std::any& value) override;

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
  std::any current_;
};

}  // namespace gardener::gfx::ui
