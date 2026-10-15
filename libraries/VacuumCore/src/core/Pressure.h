#pragma once

// Pressure domain: kPa is the primary unit everywhere. The MPRLS library reports hPa
// and hal/PressureSensor converts once, at that boundary.

namespace eggplan {

inline float kpaToHpa(float kpa) { return kpa * 10.0f; }
inline float kpaToBar(float kpa) { return kpa * 0.01f; }
inline float kpaToPsi(float kpa) { return kpa * 0.14503774f; }
inline float kpaToInHg(float kpa) { return kpa * 0.29529988f; }
inline float kpaToMmHg(float kpa) { return kpa * 7.50061683f; }

}  // namespace eggplan
