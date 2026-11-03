#include "panel/settings/SettingsRows.h"

#include <math.h>
#include <stdio.h>

#include <utility>

#include "core/Units.h"
#include "layout/Drawing.h"
#include "machine/Machine.h"
#include "panel/settings/BoolScene.h"
#include "panel/settings/DurationScene.h"
#include "panel/settings/PressureScene.h"

namespace gardener {
namespace {

// A value handed in from outside the list is asked for by the type the row holds rather than asked
// what type it is: the cast to a pointer answers nothing for the wrong one instead of throwing.
template <typename T> bool isHeldAs(const std::any& value) {
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

BoolSettingCfg::BoolSettingCfg(GestureQueue& gestures, Ux& ux, const String& key,
                               const String& label, bool defaultValue, bool nullable,
                               Validator validator)
    : SettingConfig(std::move(validator)), gestures_(gestures), ux_(ux), key_(key), label_(label),
      default_(defaultValue), nullable_(nullable) {}

std::unique_ptr<EditorScene> BoolSettingCfg::scene(const std::any& value) {
  String current;
  if (value.has_value()) current = std::any_cast<bool>(value) ? "true" : "false";
  return std::make_unique<BoolScene>(gestures_, ux_, label_, current, nullable_);
}

// The document's word for a switch, and the words a config reads as the same thing.
String BoolSettingCfg::stored(const std::any& value) const {
  if (!value.has_value()) return String();
  return std::any_cast<bool>(value) ? "on" : "off";
}

std::any BoolSettingCfg::parse(const String& stored) const {
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

bool BoolSettingCfg::accept(std::any& value) const { return isHeldAs<bool>(value); }

std::unique_ptr<SettingConfig> BoolSettingCfg::clone() const {
  return std::make_unique<BoolSettingCfg>(*this);
}

// --- the choice -------------------------------------------------------------

ChoiceSettingCfg::ChoiceSettingCfg(GestureQueue& gestures, Ux& ux, const String& key,
                                   const String& label, const String& defaultValue,
                                   const std::vector<gfx::RowList::Row>& items, bool nullable,
                                   const char* icon, std::function<void(String& current)> forward,
                                   Validator validator)
    : SettingConfig(std::move(validator)), gestures_(gestures), ux_(ux), key_(key), label_(label),
      default_(defaultValue), items_(items), nullable_(nullable), forward_(std::move(forward)),
      icon_(icon != nullptr ? icon : "") {}

// The editor's rows: each entry's title as the list's own title row, which is the picture the scene
// hands its model.
std::unique_ptr<EditorScene> ChoiceSettingCfg::scene(const std::any& value) {
  const String current = value.has_value() ? std::any_cast<String>(value) : String();
  std::vector<gfx::RowList::Row> rows;
  rows.reserve(items_.size());
  for (const gfx::RowList::Row& item : items_)
    rows.push_back(item);
  return std::make_unique<ChoiceScene>(gestures_, ux_, label_, current, std::move(rows), nullable_,
                                       forward_);
}

// The document keeps the key, not the title the list prints.
String ChoiceSettingCfg::stored(const std::any& value) const {
  return value.has_value() ? std::any_cast<String>(value) : String();
}

// A key the document names that no entry offers is one the row cannot hold, so it answers the row's
// default instead, as an entry-less row answers it for every key.
std::any ChoiceSettingCfg::parse(const String& stored) const {
  if (stored.isEmpty()) return defaultValue();
  for (const gfx::RowList::Row& item : items_) {
    if (item.value == stored) return std::any(stored);
  }
  return defaultValue();
}

bool ChoiceSettingCfg::accept(std::any& value) const { return isHeldAs<String>(value); }

std::unique_ptr<SettingConfig> ChoiceSettingCfg::clone() const {
  return std::make_unique<ChoiceSettingCfg>(*this);
}

// --- the duration -----------------------------------------------------------

DurationSettingCfg::DurationSettingCfg(GestureQueue& gestures, Ux& ux, const String& key,
                                       const String& label, int32_t defaultValue, int32_t step,
                                       int32_t min, int32_t max, bool nullable, Validator validator)
    : NumericSettingCfg(gestures, ux, key, label, static_cast<float>(defaultValue),
                        static_cast<float>(step), static_cast<float>(min), static_cast<float>(max),
                        nullable, nullptr, 0, std::move(validator)) {}

// The kind holds whole seconds, so a default the number keeps as a float is answered as the second it
// names.
std::any DurationSettingCfg::defaultValue() const {
  const std::any held = NumericSettingCfg::defaultValue();
  return held.has_value() ? std::any(static_cast<int32_t>(std::any_cast<float>(held))) : held;
}

std::unique_ptr<EditorScene> DurationSettingCfg::scene(const std::any& value) {
  const float current = value.has_value() ? static_cast<float>(std::any_cast<int32_t>(value)) : NAN;
  return std::make_unique<DurationScene>(gestures_, ux_, label_, current, step(), min_, max_,
                                         nullable_);
}

// The document keeps whole seconds, which is the unit the kind already holds.
String DurationSettingCfg::stored(const std::any& value) const {
  if (!value.has_value()) return String();
  return String(static_cast<long>(std::any_cast<int32_t>(value)));
}

// Seconds outside the declared bounds answer the bound: a document cannot put a value in the set
// that the row's own editor could not reach.
std::any DurationSettingCfg::parse(const String& stored) const {
  if (stored.isEmpty()) return defaultValue();
  const int32_t seconds = static_cast<int32_t>(stored.toInt());
  const int32_t floor = static_cast<int32_t>(min_);
  const int32_t ceiling = static_cast<int32_t>(max_);
  if (seconds < floor) return floor;
  if (seconds > ceiling) return ceiling;
  return seconds;
}

bool DurationSettingCfg::accept(std::any& value) const { return takeNumber(value, true); }

std::unique_ptr<SettingConfig> DurationSettingCfg::clone() const {
  return std::make_unique<DurationSettingCfg>(*this);
}

// A section's pair replaces the declaration's, rounded to the whole second the kind holds. A pair
// with no second between the two is a typo and leaves the declaration's in place.
void DurationSettingCfg::limit(float min, float max) {
  const float floor = isnan(min) ? min_ : roundf(min);
  const float ceiling = isnan(max) ? max_ : roundf(max);
  if (floor > ceiling) return;
  min_ = floor;
  max_ = ceiling;
}

// --- the number -------------------------------------------------------------

NumericSettingCfg::NumericSettingCfg(GestureQueue& gestures, Ux& ux, const String& key,
                                     const String& label, float defaultValue, float step, float min,
                                     float max, bool nullable, NumberFormatter format,
                                     uint8_t decimals, Validator validator)
    : SettingConfig(std::move(validator)), gestures_(gestures), ux_(ux), label_(label), step_(step),
      min_(min), max_(max), nullable_(nullable), format_(std::move(format)), key_(key),
      default_(defaultValue), decimals_(decimals) {}

// Nothing is NAN, which is what the number kind holds its nothing as, so the list never carries a
// NAN where it means `--`.
std::any NumericSettingCfg::defaultValue() const {
  return isnan(default_) ? std::any() : std::any(default_);
}

float NumericSettingCfg::step() const { return step_; }

std::unique_ptr<EditorScene> NumericSettingCfg::scene(const std::any& value) {
  const float current = value.has_value() ? std::any_cast<float>(value) : NAN;
  return std::make_unique<NumberScene>(gestures_, ux_, label_, current, step(), min_, max_,
                                       nullable_, format_);
}

// The document keeps the number at the row's decimals, which is the precision the setting writes with
// rather than the units the glass prints it in.
String NumericSettingCfg::stored(const std::any& value) const {
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
std::any NumericSettingCfg::parse(const String& stored) const {
  if (stored.isEmpty()) return defaultValue();
  float number = stored.toFloat();
  if (number < min_) number = min_;
  if (number > max_) number = max_;
  return std::any(number);
}

bool NumericSettingCfg::accept(std::any& value) const { return takeNumber(value, false); }

std::unique_ptr<SettingConfig> NumericSettingCfg::clone() const {
  return std::make_unique<NumericSettingCfg>(*this);
}

// A section's pair replaces the declaration's, and a bound it does not name keeps the one it was
// declared with. A pair with no number between the two is a typo and leaves the declaration's.
void NumericSettingCfg::limit(float min, float max) {
  const float floor = isnan(min) ? min_ : min;
  const float ceiling = isnan(max) ? max_ : max;
  if (floor > ceiling) return;
  if (!isnan(min)) min_ = min;
  if (!isnan(max)) max_ = max;
}

// --- the pressure -----------------------------------------------------------

PressureSettingCfg::PressureSettingCfg(GestureQueue& gestures, Ux& ux, const String& key,
                                       const String& label, const SystemState& state,
                                       float currentKpa, float wholeSteps, bool nullable,
                                       Validator validator)
    : NumericSettingCfg(
          gestures, ux, key, label, currentKpa,
          wholeSteps * displayUnitInfo(state.unit).wholeStep / displayUnitInfo(state.unit).perKpa,
          CONTROL_MIN_TARGET_KPA, CONTROL_FLOOR_KPA, nullable, nullptr, 2, std::move(validator)),
      state_(state), wholeSteps_(wholeSteps) {}

// What a keypress is worth is the unit the panel shows, so it is read when the list or the editor
// asks: a unit changed in the device's own list reaches every pressure the image declared, not only
// the ones declared after it.
float PressureSettingCfg::step() const {
  const DisplayUnitInfo& info = displayUnitInfo(state_.unit);
  return wholeSteps_ * info.wholeStep / info.perKpa;
}

std::unique_ptr<EditorScene> PressureSettingCfg::scene(const std::any& value) {
  const float currentKpa = value.has_value() ? std::any_cast<float>(value) : NAN;
  return std::make_unique<PressureScene>(gestures_, ux_, label_, currentKpa, state_.unit,
                                         wholeSteps_, min_, max_, nullable_);
}

// The machine's shallowest and deepest target are the outermost a pressure may sit at, so a section's
// pair that reaches past them is held to them before it becomes the row's.
void PressureSettingCfg::limit(float min, float max) {
  if (!isnan(min) && min < CONTROL_MIN_TARGET_KPA) min = CONTROL_MIN_TARGET_KPA;
  if (!isnan(max) && max > CONTROL_FLOOR_KPA) max = CONTROL_FLOOR_KPA;
  NumericSettingCfg::limit(min, max);
}

std::unique_ptr<SettingConfig> PressureSettingCfg::clone() const {
  return std::make_unique<PressureSettingCfg>(*this);
}

}  // namespace gardener