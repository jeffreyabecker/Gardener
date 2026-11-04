#pragma once

#include <stdint.h>

#include <any>
#include <functional>
#include <memory>
#include <vector>

#include <Arduino.h>

#include "core/SystemState.h"
#include "core/Units.h"
#include "panel/settings/ChoiceScene.h"
#include "panel/settings/NumberScene.h"
#include "panel/settings/SettingsScene.h"

namespace gardener {

// The rows a settings list is built from, one class per value kind: each builds the scene that edits
// its value. A row carries the rules of its kind — a number's step and bounds, a choice's entries —
// the default it was handed, which the list falls back to for a key the set it is given does not
// carry, and the check its declaration gave. The key is the setting's: what an editor hands back is
// written under it.

// --- the bool ---------------------------------------------------------------

// The row of an on/off setting and the switch the bool scene draws to change it. A nullable
// setting's nothing opens the switch on nothing.
class BoolSettingCfg : public SettingConfig {
 public:
  BoolSettingCfg(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                 bool defaultValue, bool nullable = false, Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override { return default_; }

  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;
  bool accept(std::any& value) const override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any default_;
  bool nullable_;
};

// --- the choice -------------------------------------------------------------

// The row of a setting that stores a key out of a fixed list, and the choice scene over the entries
// the row was handed. `icon` is the packed bitmap printed before the label, in the hex dialect
// `gfx::Bitmap` reads; a list draws it in the columns the bitmap names, and a row given none prints
// its label at the left of the row. `forward` is the page's own work on the entry showing — a jingle
// row is built with one that sounds the entry — and a row given none drops Forward.
class ChoiceSettingCfg : public SettingConfig {
 public:
  ChoiceSettingCfg(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                   const String& defaultValue, const std::vector<gfx::RowList::Row>& items,
                   bool nullable = false, const char* icon = nullptr,
                   std::function<void(String& current)> forward = nullptr,
                   Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  const String& icon() const override { return icon_; }
  std::any defaultValue() const override { return default_; }

  // The entries the row offers, each the key it stores and the title the list prints for it.
  const std::vector<gfx::RowList::Row>& entries() const { return items_; }

  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;
  bool accept(std::any& value) const override;

 private:
  GestureQueue& gestures_;
  Ux& ux_;
  String key_;
  String label_;
  std::any default_;
  std::vector<gfx::RowList::Row> items_;
  bool nullable_;
  std::function<void(String& current)> forward_;
  String icon_;
};

// --- the number -------------------------------------------------------------

// The row of a number: the number scene over its step and bounds, printing through the setting's own
// formatter where it reads in its units. A kind that edits this number its own way extends the row
// and builds its scene from the same rules. A default of nothing is NAN, the number the scene
// holding none already is, which the row answers as an empty any.
class NumericSettingCfg : public SettingConfig {
 public:
  NumericSettingCfg(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                    float defaultValue, float step, float min, float max, bool nullable,
                    NumberFormatter format = nullptr, uint8_t decimals = 2,
                    Validator validator = nullptr);

  const String& key() const override { return key_; }
  const String& label() const override { return label_; }
  std::any defaultValue() const override;

  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;
  bool accept(std::any& value) const override;

  // The step a keypress makes, as the list and the editor read it. A kind whose number is shown in a
  // unit the device can change answers it from the state each time it is asked, so a row a routine
  // declared when the image loaded still steps by the unit the panel is showing now.
  virtual float step() const;

  // The bounds the row was declared with, in the kind's own units.
  float min() const { return min_; }
  float max() const { return max_; }

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

// --- the duration -----------------------------------------------------------

// The row of a duration held in whole seconds, and the two-field scene that steps the minutes and
// the seconds. It is a number row over the seconds it stores, so the step and the bounds are the
// number's, answered in the whole seconds the kind holds.
class DurationSettingCfg : public NumericSettingCfg {
 public:
  DurationSettingCfg(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                     int32_t defaultValue, int32_t step, int32_t min, int32_t max,
                     bool nullable = false, Validator validator = nullptr);

  std::any defaultValue() const override;

  std::unique_ptr<EditorScene> scene(const std::any& value) override;
  String stored(const std::any& value) const override;
  std::any parse(const String& stored) const override;
  bool accept(std::any& value) const override;
};

// --- the pressure -----------------------------------------------------------

// The row of the target pressure: a number row over the depth in the kPa the machine stores, whose
// editor is the pressure scene — a keypress is one step of the unit the glass shows. The bounds are
// the machine's, so a keypress cannot leave the editor on a target the loop would refuse. The unit
// is read from the device's state rather than kept, so a unit changed in the device's own list
// reaches a row the image declared at load.
class PressureSettingCfg : public NumericSettingCfg {
 public:
  PressureSettingCfg(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                     const SystemState& state, float currentKpa, float wholeSteps,
                     bool nullable = false, Validator validator = nullptr);

  std::unique_ptr<EditorScene> scene(const std::any& value) override;

  float step() const override;

 private:
  const SystemState& state_;
  float wholeSteps_;
};

}  // namespace gardener
