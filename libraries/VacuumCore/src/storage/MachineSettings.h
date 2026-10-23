#pragma once

#include "storage/Settings.h"

namespace gardener {

// The machine's own preferences: the two numbers the control loop is configured from. They are
// compile-time defaults, restored from /settings.cfg, and overridable over the wire like `unit`.
// The panel layers its own preferences on top in PanelSettings.
class MachineSettings : public Settings {
 public:
  explicit MachineSettings(SettingsSource* source) : Settings(source) {}

  float pumpFlowLpm() const { return pumpFlowLpm_; }
  void setPumpFlowLpm(float lpm);

  float chamberVolumeL() const { return chamberVolumeL_; }
  void setChamberVolumeL(float litres);

  // Bumped whenever either value changes, so the loop can tell a retune apart and reset its
  // integral without holding a second copy of the values.
  uint8_t revision() const { return revision_; }

 protected:
  void load() override;

 private:
  float pumpFlowLpm_ = 0.0f;
  float chamberVolumeL_ = 0.0f;
  uint8_t revision_ = 0;
};

}  // namespace gardener
