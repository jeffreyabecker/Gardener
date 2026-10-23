#include "storage/MachineSettings.h"

#include "Machine.h"

namespace gardener {
namespace {

constexpr const char* kFlowKey = "flow";
constexpr const char* kVolumeKey = "volume";

}  // namespace

// Reading is best effort: a value that is missing or will not parse keeps the compile-time
// default, and a non-positive one is not a rig, so it keeps the default too.
void MachineSettings::load() {
  pumpFlowLpm_ = getFloat(kFlowKey, PUMP_FLOW_LPM);
  if (!(pumpFlowLpm_ > 0.0f)) pumpFlowLpm_ = PUMP_FLOW_LPM;

  chamberVolumeL_ = getFloat(kVolumeKey, CHAMBER_VOLUME_L);
  if (!(chamberVolumeL_ > 0.0f)) chamberVolumeL_ = CHAMBER_VOLUME_L;
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

}  // namespace gardener
