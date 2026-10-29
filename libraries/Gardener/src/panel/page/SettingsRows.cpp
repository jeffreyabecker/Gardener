#include "panel/page/SettingsRows.h"

#include <math.h>
#include <stdio.h>

#include <utility>

#include "panel/page/BoolScene.h"
#include "Drawing.h"
#include "panel/page/DurationScene.h"

namespace gardener::gfx::ui {
namespace {

// A number as the list prints it: `%g` of it, which is what a number scene prints when the setting
// handed it no formatter of its own.
String printedNumber(float number) {
  char printed[16];
  snprintf(printed, sizeof(printed), "%g", static_cast<double>(number));
  return String(printed);
}

// A value handed in from outside the list is asked for by the type the row holds rather than asked
// what type it is: the cast to a pointer answers nothing for the wrong one instead of throwing.
template <typename T>
bool isHeldAs(const std::any& value) {
  return !value.has_value() || std::any_cast<T>(&value) != nullptr;
}

// A number of either numeric type as the one the row holds, so a whole literal the file wrote for a
// pressure is the pressure. False, leaving the value alone, for anything that is not a number.
bool takeNumber(std::any& value, bool whole) {
  if (!value.has_value()) return true;
  if (whole) {
    if (isHeldAs<int32_t>(value)) return true;
    const float* real = std::any_cast<float>(&value);
    if (real == nullptr) return false;
    value = std::any(static_cast<int32_t>(*real));
    return true;
  }
  if (isHeldAs<float>(value)) return true;
  const int32_t* seconds = std::any_cast<int32_t>(&value);
  if (seconds == nullptr) return false;
  value = std::any(static_cast<float>(*seconds));
  return true;
}

}  // namespace

// --- the bool ---------------------------------------------------------------

BoolRow::BoolRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                 bool defaultValue, bool nullable, Validator validator)
    : SettingsRow(std::move(validator)),
      gestures_(gestures),
      ux_(ux),
      key_(key),
      label_(label),
      default_(defaultValue),
      nullable_(nullable) {}

// The switch declares its kind and nothing else: it has no step, no bound and no entries.
SettingDeclaration BoolRow::declaration() const { return {"bool"}; }

String BoolRow::text(const std::any& value) const {
  if (!value.has_value()) return "--";
  return std::any_cast<bool>(value) ? "ON" : "OFF";
}

std::unique_ptr<EditorScene> BoolRow::scene(const std::any& value) {
  String current;
  if (value.has_value()) current = std::any_cast<bool>(value) ? "true" : "false";
  return std::make_unique<BoolScene>(gestures_, ux_, label_, current, nullable_);
}

// The document's word for a switch, and the words a config reads as the same thing.
String BoolRow::stored(const std::any& value) const {
  if (!value.has_value()) return String();
  return std::any_cast<bool>(value) ? "on" : "off";
}

std::any BoolRow::parse(const String& stored) const {
  if (stored.equalsIgnoreCase("on") || stored.equalsIgnoreCase("true") ||
      stored.equalsIgnoreCase("1") || stored.equalsIgnoreCase("yes")) {
    return true;
  }
  if (stored.equalsIgnoreCase("off") || stored.equalsIgnoreCase("false") ||
      stored.equalsIgnoreCase("0") || stored.equalsIgnoreCase("no")) {
    return false;
  }
  return defaultValue();
}

bool BoolRow::accept(std::any& value) const { return isHeldAs<bool>(value); }

// --- the choice -------------------------------------------------------------

ChoiceRow::ChoiceRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                     const String& defaultValue, const std::vector<ChoiceItem>& items,
                     bool nullable, std::function<void(String& current)> forward,
                     Validator validator)
    : SettingsRow(std::move(validator)),
      gestures_(gestures),
      ux_(ux),
      key_(key),
      label_(label),
      default_(defaultValue),
      items_(items),
      nullable_(nullable),
      forward_(std::move(forward)) {}

// A choice declares its entries — the keys it offers, which are the titles it prints — and no step
// or bound, because the keys are the only thing it moves between.
SettingDeclaration ChoiceRow::declaration() const {
  SettingDeclaration out;
  out.kind = "choice";
  out.entries = items_;
  return out;
}

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

std::unique_ptr<EditorScene> ChoiceRow::scene(const std::any& value) {
  const String current = value.has_value() ? std::any_cast<String>(value) : String();
  return std::make_unique<ChoiceScene>(gestures_, ux_, label_, current, items_, nullable_,
                                       forward_);
}

// The document keeps the key, not the title the list prints.
String ChoiceRow::stored(const std::any& value) const {
  return value.has_value() ? std::any_cast<String>(value) : String();
}

// A key the document names that no entry offers is one the row cannot hold, so it answers the row's
// default instead, as an entry-less row answers it for every key.
std::any ChoiceRow::parse(const String& stored) const {
  if (stored.isEmpty()) return defaultValue();
  for (const ChoiceItem& item : items_) {
    if (item.key == stored) return std::any(stored);
  }
  return defaultValue();
}

bool ChoiceRow::accept(std::any& value) const { return isHeldAs<String>(value); }

// --- the duration -----------------------------------------------------------

DurationRow::DurationRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                         int32_t defaultValue, int32_t step, int32_t min, int32_t max,
                         bool nullable, Validator validator)
    : SettingsRow(std::move(validator)),
      gestures_(gestures),
      ux_(ux),
      key_(key),
      label_(label),
      default_(defaultValue),
      step_(step),
      min_(min),
      max_(max),
      nullable_(nullable) {}

// The bounds a duration was declared with are whole seconds; a caller reading the declaration back
// takes them as the numbers they are.
SettingDeclaration DurationRow::declaration() const {
  SettingDeclaration out;
  out.kind = "duration";
  out.step = static_cast<float>(step_);
  out.min = static_cast<float>(min_);
  out.max = static_cast<float>(max_);
  return out;
}

String DurationRow::text(const std::any& value) const {
  char printed[16];
  durationText(DurationFormat::Minutes, value.has_value() ? std::any_cast<int32_t>(value) : 0,
               value.has_value(), printed, sizeof(printed));
  return String(printed);
}

std::unique_ptr<EditorScene> DurationRow::scene(const std::any& value) {
  const float current = value.has_value() ? static_cast<float>(std::any_cast<int32_t>(value)) : NAN;
  return std::make_unique<DurationScene>(gestures_, ux_, label_, current, static_cast<float>(step_),
                                         static_cast<float>(min_), static_cast<float>(max_),
                                         nullable_);
}

// The document keeps whole seconds, which is the unit the kind already holds.
String DurationRow::stored(const std::any& value) const {
  if (!value.has_value()) return String();
  return String(static_cast<long>(std::any_cast<int32_t>(value)));
}

// Seconds outside the declared bounds answer the bound: a document cannot put a value in the set
// that the row's own editor could not reach.
std::any DurationRow::parse(const String& stored) const {
  if (stored.isEmpty()) return defaultValue();
  const int32_t seconds = static_cast<int32_t>(stored.toInt());
  if (seconds < min_) return min_;
  if (seconds > max_) return max_;
  return seconds;
}

bool DurationRow::accept(std::any& value) const { return takeNumber(value, true); }

// --- the number -------------------------------------------------------------

NumberRow::NumberRow(GestureQueue& gestures, Ux& ux, const String& key, const String& label,
                     float defaultValue, float step, float min, float max, bool nullable,
                     NumberFormatter format, uint8_t decimals, Validator validator)
    : SettingsRow(std::move(validator)),
      gestures_(gestures),
      ux_(ux),
      label_(label),
      step_(step),
      min_(min),
      max_(max),
      nullable_(nullable),
      format_(std::move(format)),
      key_(key),
      default_(defaultValue),
      decimals_(decimals) {}

// Nothing is NAN, which is what the number kind holds its nothing as, so the list never carries a
// NAN where it means `--`.
std::any NumberRow::defaultValue() const {
  return isnan(default_) ? std::any() : std::any(default_);
}

// The step and bounds the number was declared with, as the number's own units. A kind that edits a
// number its own way — a pressure — states its kind over these.
SettingDeclaration NumberRow::declaration() const {
  SettingDeclaration out;
  out.kind = "number";
  out.step = step_;
  out.min = min_;
  out.max = max_;
  return out;
}

String NumberRow::text(const std::any& value) const {
  if (!value.has_value()) return "--";
  const float number = std::any_cast<float>(value);
  if (isnan(number)) return "--";
  return format_ ? format_(number) : printedNumber(number);
}

std::unique_ptr<EditorScene> NumberRow::scene(const std::any& value) {
  const float current = value.has_value() ? std::any_cast<float>(value) : NAN;
  return std::make_unique<NumberScene>(gestures_, ux_, label_, current, step_, min_, max_,
                                       nullable_, format_);
}

// The document keeps the number at the row's decimals, which is the precision the setting writes with
// rather than the units the glass prints it in.
String NumberRow::stored(const std::any& value) const {
  if (!value.has_value()) return String();
  const float number = std::any_cast<float>(value);
  if (isnan(number)) return String();
  char printed[16];
  snprintf(printed, sizeof(printed), "%.*f", static_cast<int>(decimals_),
           static_cast<double>(number));
  return String(printed);
}

// A number outside the declared bounds answers the bound, as a duration does: the row holds what it
// is given to the same rules the editor steps within.
std::any NumberRow::parse(const String& stored) const {
  if (stored.isEmpty()) return defaultValue();
  float number = stored.toFloat();
  if (number < min_) number = min_;
  if (number > max_) number = max_;
  return std::any(number);
}

bool NumberRow::accept(std::any& value) const { return takeNumber(value, false); }

}  // namespace gardener::gfx::ui
