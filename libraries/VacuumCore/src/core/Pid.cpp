#include "core/Pid.h"

#include "core/Math.h"

namespace eggplan {

float Pid::update(float dt, float error, float measurementSlew) {
  if (dt <= 0.0f) return config_.kp * error + integral_;

  integral_ = clampf(integral_ + config_.ki * error * dt, -config_.integralLimit,
                     config_.integralLimit);

  return config_.kp * error + integral_ - config_.kd * measurementSlew;
}

}  // namespace eggplan
