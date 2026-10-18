#pragma once

#include <stdint.h>
#include <strings.h>

namespace gardener {

// What the panel prints pressure in. The machine and the wire stay in kPa — a target is a depth
// below ambient in kPa and `vacuum get` reports it that way — so this only decides how the same
// number is shown and stepped.
enum class DisplayUnit : uint8_t { Kpa, Psi, InHg, MmHg };

// One unit: its wire token, what the glass prints, and how it steps.
//
// `perKpa` is the whole conversion. The reading is a vacuum depth, so every unit is a straight
// scale of it rather than an absolute pressure. `wholeStep` and `subStep` are increments in display
// units for the target control the UI will grow; nothing reads them yet.
struct DisplayUnitInfo {
  const char* name;    // the wire token, and what `device get` answers
  const char* suffix;  // what the panel prints after the number
  float perKpa;
  float wholeStep;
  float subStep;
};

// In DisplayUnit order, kPa first because it is the unit the machine works in.
constexpr DisplayUnitInfo kDisplayUnits[] = {
    {"kpa", "kPa", 1.0f, 1.0f, 0.1f},
    {"psi", "psi", 0.14503774f, 1.0f, 0.1f},
    {"inhg", "inHg", 0.29529991f, 1.0f, 0.1f},
    {"mmhg", "mmHg", 7.50061683f, 10.0f, 1.0f},
};

constexpr uint8_t kDisplayUnitCount =
    static_cast<uint8_t>(sizeof(kDisplayUnits) / sizeof(kDisplayUnits[0]));

inline const DisplayUnitInfo& displayUnitInfo(DisplayUnit unit) {
  return kDisplayUnits[static_cast<uint8_t>(unit)];
}

// The wire's token for a unit, case insensitive; false leaves `out` alone.
inline bool parseDisplayUnit(const char* token, DisplayUnit& out) {
  for (uint8_t i = 0; i < kDisplayUnitCount; ++i) {
    if (strcasecmp(token, kDisplayUnits[i].name) == 0) {
      out = static_cast<DisplayUnit>(i);
      return true;
    }
  }
  return false;
}

// The panel prints what the fine step can express: a unit that steps in whole millimetres has no
// use for a decimal, and one that steps in tenths needs exactly one.
inline uint8_t displayDecimals(DisplayUnit unit) {
  float step = displayUnitInfo(unit).subStep;
  if (step >= 1.0f) return 0;
  if (step >= 0.1f) return 1;
  return 2;
}

// The same depth, in the unit the glass prints.
inline float displayValue(float kpa, DisplayUnit unit) {
  return kpa * displayUnitInfo(unit).perKpa;
}

}  // namespace gardener
