#pragma once

#include <stdint.h>

namespace eggplan {

struct PidConfig {
  float kp = 0.0f;
  float ki = 0.0f;
  float kd = 0.0f;
  float integralLimit = 0.0f;
};

// Pure controller math: no clock, no sensors, so the caller decides when a step
// happens and the output limits stay with the layer that owns the actuator.
class Pid {
 public:
  explicit Pid(const PidConfig& config) : config_(config) {}

  void reset() { integral_ = 0.0f; }
  float integral() const { return integral_; }

  // error           measured - target, positive when more vacuum is needed
  // measurementSlew d(measured)/dt, used as the derivative term
  float update(float dt, float error, float measurementSlew);

 private:
  PidConfig config_;
  float integral_ = 0.0f;
};

}  // namespace eggplan
