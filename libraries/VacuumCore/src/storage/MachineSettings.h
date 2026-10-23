#pragma once

#include "storage/Settings.h"

namespace gardener {

// The machine's own preferences: the numbers the control loop and the fitted parts are configured
// from. They are compile-time defaults, restored from /settings.cfg, and overridable over the wire
// like `unit`. The panel layers its own preferences on top in PanelSettings.
class MachineSettings : public Settings {
 public:
  explicit MachineSettings(Library* store) : Settings(store) {}

  float pumpFlowLpm() const { return pumpFlowLpm_; }
  void setPumpFlowLpm(float lpm);

  float chamberVolumeL() const { return chamberVolumeL_; }
  void setChamberVolumeL(float litres);

  float floorKpa() const { return floorKpa_; }
  void setFloorKpa(float kpa);

  float pumpMaxPercent() const { return pumpMaxPercent_; }
  void setPumpMaxPercent(float percent);

  float deadbandKpa() const { return deadbandKpa_; }
  void setDeadbandKpa(float kpa);

  // True when the fitted valve is normally open (de-energised vents). This is a fitted-part fact
  // read once at boot, not a live knob: the machine builds the matching valve from it.
  bool valveNormallyOpen() const { return valveNormallyOpen_; }
  void setValveNormallyOpen(bool normallyOpen);

  // Bumped whenever a loop input changes, so the loop can tell a retune apart and reset its
  // integral without holding a second copy of the values.
  uint8_t revision() const { return revision_; }

 protected:
  void load() override;

 private:
  float pumpFlowLpm_ = 0.0f;
  float chamberVolumeL_ = 0.0f;
  float floorKpa_ = 0.0f;
  float pumpMaxPercent_ = 0.0f;
  float deadbandKpa_ = 0.0f;
  bool valveNormallyOpen_ = true;
  uint8_t revision_ = 0;
};

}  // namespace gardener
