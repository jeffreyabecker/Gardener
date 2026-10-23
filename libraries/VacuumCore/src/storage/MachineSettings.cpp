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

  floorKpa_ = getFloat(kFloorKey, CONTROL_FLOOR_KPA);
  if (!(floorKpa_ >= CONTROL_MIN_TARGET_KPA)) floorKpa_ = CONTROL_FLOOR_KPA;

  pumpMaxPercent_ = getFloat(kPumpMaxKey, PUMP_DUTY_MAX_PERCENT);
  if (!(pumpMaxPercent_ > 0.0f && pumpMaxPercent_ <= PUMP_DUTY_MAX_PERCENT)) {
    pumpMaxPercent_ = PUMP_DUTY_MAX_PERCENT;
  }

  deadbandKpa_ = getFloat(kDeadbandKey, CONTROL_DEADBAND_KPA);
  if (!(deadbandKpa_ > 0.0f)) deadbandKpa_ = CONTROL_DEADBAND_KPA;

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

void MachineSettings::setFloorKpa(float kpa) {
  if (!(kpa >= CONTROL_MIN_TARGET_KPA) || kpa == floorKpa_) return;
  floorKpa_ = kpa;
  setFloat(kFloorKey, kpa, 1);
}

void MachineSettings::setPumpMaxPercent(float percent) {
  if (!(percent > 0.0f && percent <= PUMP_DUTY_MAX_PERCENT) || percent == pumpMaxPercent_) return;
  pumpMaxPercent_ = percent;
  revision_++;
  setFloat(kPumpMaxKey, percent, 1);
}

void MachineSettings::setDeadbandKpa(float kpa) {
  if (!(kpa > 0.0f) || kpa == deadbandKpa_) return;
  deadbandKpa_ = kpa;
  revision_++;
  setFloat(kDeadbandKey, kpa, 2);
}

void MachineSettings::setValveNormallyOpen(bool normallyOpen) {
  if (normallyOpen == valveNormallyOpen_) return;
  valveNormallyOpen_ = normallyOpen;
  set(kValveKey, normallyOpen ? "open" : "closed");
}

}  // namespace gardener
