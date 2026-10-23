#include "storage/MachineSettings.h"

#include "Machine.h"

namespace gardener {
namespace {

constexpr const char* kFlowKey = "flow";
constexpr const char* kVolumeKey = "volume";
constexpr const char* kFloorKey = "floor";
constexpr const char* kPumpMaxKey = "pump_max";
constexpr const char* kDeadbandKey = "deadband";
constexpr const char* kValveKey = "valve";

}  // namespace

// Reading is best effort: a value that is missing, will not parse or is out of bounds keeps the
// compile-time default.
void MachineSettings::load() {
  pumpFlowLpm_ = getFloat(kFlowKey, PUMP_FLOW_LPM);
  if (!(pumpFlowLpm_ > 0.0f)) pumpFlowLpm_ = PUMP_FLOW_LPM;

  chamberVolumeL_ = getFloat(kVolumeKey, CHAMBER_VOLUME_L);
  if (!(chamberVolumeL_ > 0.0f)) chamberVolumeL_ = CHAMBER_VOLUME_L;

  floorPressureKpa_ = getFloat(kFloorKey, CONTROL_FLOOR_KPA);
  if (!(floorPressureKpa_ >= CONTROL_MIN_TARGET_KPA)) floorPressureKpa_ = CONTROL_FLOOR_KPA;

  pumpMaxPercent_ = getFloat(kPumpMaxKey, PUMP_DUTY_MAX_PERCENT);
  if (!(pumpMaxPercent_ > 0.0f && pumpMaxPercent_ <= PUMP_DUTY_MAX_PERCENT)) {
    pumpMaxPercent_ = PUMP_DUTY_MAX_PERCENT;
  }

  deadbandPressureKpa_ = getFloat(kDeadbandKey, CONTROL_DEADBAND_KPA);
  if (!(deadbandPressureKpa_ > 0.0f)) deadbandPressureKpa_ = CONTROL_DEADBAND_KPA;

  // "open" is the default and the fail-safe; anything that is not "closed" reads as open.
  valveNormallyOpen_ = !get(kValveKey, "open").equalsIgnoreCase("closed");
}

void MachineSettings::setPumpFlowLpm(float lpm) {
  if (!(lpm > 0.0f) || lpm == pumpFlowLpm_) return;
  pumpFlowLpm_ = lpm;
  revision_++;
  setFloat(kFlowKey, lpm, 2);
}

void MachineSettings::setChamberVolumeL(float litres) {
  if (!(litres > 0.0f) || litres == chamberVolumeL_) return;
  chamberVolumeL_ = litres;
  revision_++;
  setFloat(kVolumeKey, litres, 3);
}

void MachineSettings::setFloorPressureKpa(float pressureKpa) {
  if (!(pressureKpa >= CONTROL_MIN_TARGET_KPA) || pressureKpa == floorPressureKpa_) return;
  floorPressureKpa_ = pressureKpa;
  setFloat(kFloorKey, pressureKpa, 1);
}

void MachineSettings::setPumpMaxPercent(float percent) {
  if (!(percent > 0.0f && percent <= PUMP_DUTY_MAX_PERCENT) || percent == pumpMaxPercent_) return;
  pumpMaxPercent_ = percent;
  revision_++;
  setFloat(kPumpMaxKey, percent, 1);
}

void MachineSettings::setDeadbandPressureKpa(float pressureKpa) {
  if (!(pressureKpa > 0.0f) || pressureKpa == deadbandPressureKpa_) return;
  deadbandPressureKpa_ = pressureKpa;
  revision_++;
  setFloat(kDeadbandKey, pressureKpa, 2);
}

void MachineSettings::setValveNormallyOpen(bool normallyOpen) {
  if (normallyOpen == valveNormallyOpen_) return;
  valveNormallyOpen_ = normallyOpen;
  set(kValveKey, normallyOpen ? "open" : "closed");
}

}  // namespace gardener
