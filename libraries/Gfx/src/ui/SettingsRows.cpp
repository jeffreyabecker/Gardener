#include "ui/SettingsRows.h"

#include <math.h>
#include <stdio.h>

#include <utility>

#include "ui/BoolScene.h"
#include "ui/Drawing.h"
#include "ui/DurationScene.h"

namespace gardener::gfx::ui {
namespace {

// A number as the list prints it: `%g` of it, which is what a number scene prints when the setting
// handed it no formatter of its own.
String printedNumber(float number) {
  char printed[16];
  snprintf(printed, sizeof(printed), "%g", static_cast<double>(number));
  return String(printed);
}

}  // namespace

// --- the bool ---------------------------------------------------------------

BoolRow::BoolRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                 bool defaultValue, bool nullable)
    : gestures_(gestures),
      ux_(ux),
      key_(key),
      label_(label),
      default_(defaultValue),
      nullable_(nullable) {}

String BoolRow::text(const std::any& value) const {
  if (!value.has_value()) return "--";
  return std::any_cast<bool>(value) ? "ON" : "OFF";
}

std::unique_ptr<Scene> BoolRow::scene(const std::any& value) {
  String current;
  if (value.has_value()) current = std::any_cast<bool>(value) ? "true" : "false";
  return std::make_unique<BoolScene>(gestures_, ux_, label_, current, nullable_);
}

// --- the choice -------------------------------------------------------------

ChoiceRow::ChoiceRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                     const String& defaultValue, const std::vector<ChoiceItem>& items,
                     bool nullable, std::function<void(String& current)> forward)
    : gestures_(gestures),
      ux_(ux),
      key_(key),
      label_(label),
      default_(defaultValue),
      items_(items),
      nullable_(nullable),
      forward_(std::move(forward)) {}

// The title the entry showing prints, which is what the list has room for: a key no entry names
// reads as the first entry, the way the scene shows it.
String ChoiceRow::text(const std::any& value) const {
  if (!value.has_value() || items_.empty()) return "--";
  const String key = std::any_cast<String>(value);
  for (const ChoiceItem& item : items_) {
    if (item.key == key) return item.title;
  }
  return items_.front().title;
}

std::unique_ptr<Scene> ChoiceRow::scene(const std::any& value) {
  const String current = value.has_value() ? std::any_cast<String>(value) : String();
  return std::make_unique<ChoiceScene>(gestures_, ux_, label_, current, items_, nullable_,
                                       forward_);
}

// --- the duration -----------------------------------------------------------

DurationRow::DurationRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                         int32_t defaultValue, int32_t step, int32_t min, int32_t max,
                         bool nullable)
    : gestures_(gestures),
      ux_(ux),
      key_(key),
      label_(label),
      default_(defaultValue),
      step_(step),
      min_(min),
      max_(max),
      nullable_(nullable) {}

String DurationRow::text(const std::any& value) const {
  char printed[16];
  durationText(DurationFormat::Minutes, value.has_value() ? std::any_cast<int32_t>(value) : 0,
               value.has_value(), printed, sizeof(printed));
  return String(printed);
}

std::unique_ptr<Scene> DurationRow::scene(const std::any& value) {
  const float current = value.has_value() ? static_cast<float>(std::any_cast<int32_t>(value)) : NAN;
  return std::make_unique<DurationScene>(gestures_, ux_, label_, current, static_cast<float>(step_),
                                         static_cast<float>(min_), static_cast<float>(max_),
                                         nullable_);
}

// --- the number -------------------------------------------------------------

NumberRow::NumberRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                     float defaultValue, float step, float min, float max, bool nullable,
                     NumberFormatter format)
    : gestures_(gestures),
      ux_(ux),
      label_(label),
      step_(step),
      min_(min),
      max_(max),
      nullable_(nullable),
      format_(std::move(format)),
      key_(key),
      default_(defaultValue) {}

// Nothing is NAN, which is what the number kind holds its nothing as, so the list never carries a
// NAN where it means `--`.
std::any NumberRow::defaultValue() const {
  return isnan(default_) ? std::any() : std::any(default_);
}

String NumberRow::text(const std::any& value) const {
  if (!value.has_value()) return "--";
  const float number = std::any_cast<float>(value);
  if (isnan(number)) return "--";
  return format_ ? format_(number) : printedNumber(number);
}

std::unique_ptr<Scene> NumberRow::scene(const std::any& value) {
  const float current = value.has_value() ? std::any_cast<float>(value) : NAN;
  return std::make_unique<NumberScene>(gestures_, ux_, label_, current, step_, min_, max_,
                                       nullable_, format_);
}

}  // namespace gardener::gfx::ui
