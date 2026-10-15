#pragma once

#include <Arduino.h>

#include "core/AppContext.h"
#include "core/VacuumConfig.h"

namespace eggplan {

enum class ControlState : uint8_t { Idle, Pumping, Holding, Manual, Fault };

// Closed loop vacuum control.
//
// The pump only pulls down, so the loop can never vent: when the measured
// pressure undershoots the target the output simply goes to zero and the
// natural leak pulls the chamber back up.
class VacuumController {
 public:
  void begin(AppContext& ctx);
  void update(uint32_t nowMs);

  void setTarget(float kpa);
  void disableTarget();
  void setManualDuty(float percent);
  void stop();
  void clearFault();

  ControlState state() const { return state_; }
  const char* stateText() const;
  bool targetActive() const { return targetActive_; }
  float targetKpa() const { return targetKpa_; }
  float error() const { return lastError_; }
  float output() const { return output_; }
  float duty() const;

 private:
  void setState(ControlState state);

  AppContext* ctx_ = nullptr;
  ControlState state_ = ControlState::Idle;
  bool targetActive_ = false;
  bool fault_ = false;
  float targetKpa_ = 0.0f;
  float integral_ = 0.0f;
  float lastError_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace eggplan
