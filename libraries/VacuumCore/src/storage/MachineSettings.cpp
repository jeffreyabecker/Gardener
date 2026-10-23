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
// compile-time default. The bounds are Machine.h's, the same ones the setters and both links use.
void MachineSettings::load() {
  pumpFlowLpm_ = getFloat(kFlowKey, PUMP_FLOW_LPM);
  if (!(pumpFlowLpm_ >= PUMP_FLOW_MIN_LPM && pumpFlowLpm_ <= PUMP_FLOW_MAX_LPM)) {
    pumpFlowLpm_ = PUMP_FLOW_LPM;
  }

  chamberVolumeL_ = getFloat(kVolumeKey, CHAMBER_VOLUME_L);
  if (!(chamberVolumeL_ >= CHAMBER_VOLUME_MIN_L && chamberVolumeL_ <= CHAMBER_VOLUME_MAX_L)) {
    chamberVolumeL_ = CHAMBER_VOLUME_L;
  }

  floorPressureKpa_ = getFloat(kFloorKey, CONTROL_FLOOR_KPA);
  if (!(floorPressureKpa_ >= CONTROL_MIN_TARGET_KPA &&
        floorPressureKpa_ <= CONTROL_FLOOR_MAX_KPA)) {
    floorPressureKpa_ = CONTROL_FLOOR_KPA;
  }

  pumpMaxPercent_ = getFloat(kPumpMaxKey, PUMP_DUTY_MAX_PERCENT);
  if (!(pumpMaxPercent_ > 0.0f && pumpMaxPercent_ <= PUMP_DUTY_MAX_PERCENT)) {
    pumpMaxPercent_ = PUMP_DUTY_MAX_PERCENT;
  }

  deadbandPressureKpa_ = getFloat(kDeadbandKey, CONTROL_DEADBAND_KPA);
  if (!(deadbandPressureKpa_ >= CONTROL_DEADBAND_MIN_KPA &&
        deadbandPressureKpa_ <= CONTROL_DEADBAND_MAX_KPA)) {
    deadbandPressureKpa_ = CONTROL_DEADBAND_KPA;
  }

  // "open" is the default and the fail-safe; anything that is not "closed" reads as open.
  valveNormallyOpen_ = !get(kValveKey, "open").equalsIgnoreCase("closed");
}

void MachineSettings::setPumpFlowLpm(float lpm) {
  if (!(lpm >= PUMP_FLOW_MIN_LPM && lpm <= PUMP_FLOW_MAX_LPM) || lpm == pumpFlowLpm_) return;
  pumpFlowLpm_ = lpm;
  revision_++;
  setFloat(kFlowKey, lpm, 2);
}

void MachineSettings::setChamberVolumeL(float litres) {
  if (!(litres >= CHAMBER_VOLUME_MIN_L && litres <= CHAMBER_VOLUME_MAX_L) ||
      litres == chamberVolumeL_) {
    return;
  }
  chamberVolumeL_ = litres;
  revision_++;
  setFloat(kVolumeKey, litres, 3);
}

void MachineSettings::setFloorPressureKpa(float pressureKpa) {
  if (!(pressureKpa >= CONTROL_MIN_TARGET_KPA && pressureKpa <= CONTROL_FLOOR_MAX_KPA) ||
      pressureKpa == floorPressureKpa_) {
    return;
  }
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
  if (!(pressureKpa >= CONTROL_DEADBAND_MIN_KPA && pressureKpa <= CONTROL_DEADBAND_MAX_KPA) ||
      pressureKpa == deadbandPressureKpa_) {
    return;
  }
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
