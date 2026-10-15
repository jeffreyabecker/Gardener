#include "services/VacuumControl.h"

#include <math.h>

#include "core/Log.h"
#include "core/Math.h"
#include "hal/Pump.h"
#include "hal/PressureSensor.h"

namespace eggplan {
namespace {
constexpr float kDerivativeWindowMs = 1000.0f;
}

void VacuumControl::begin() {
  state_ = ControlState::Idle;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
}

float VacuumControl::minReachableKpa() const {
  float ambient = sensor_.ambientKpa();
  float floorKpa = ambient - cfg::PUMP_MAX_SUCTION_KPA;
  return floorKpa > cfg::CONTROL_MIN_TARGET_KPA ? floorKpa : cfg::CONTROL_MIN_TARGET_KPA;
}

void VacuumControl::setState(ControlState state) {
  if (state_ == state) return;
  LOGI("control: %s -> %s", controlStateName(state_), controlStateName(state));
  state_ = state;
}

void VacuumControl::setTarget(float kpa) {
  if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
    LOGW("control: refusing target %.2f kPa", kpa);
    return;
  }
  float reachable = minReachableKpa();
  if (kpa < reachable) {
    LOGW("control: target %.1f kPa is beyond the pump, floor is %.1f kPa", kpa, reachable);
  }
  targetKpa_ = kpa;
  targetActive_ = true;
  pid_.reset();
  if (fault_) clearFault();
  LOGI("control: target %.2f kPa", kpa);
}

void VacuumControl::disableTarget() {
  targetActive_ = false;
  pid_.reset();
  output_ = 0.0f;
  pump_.stop();
  setState(ControlState::Idle);
}

void VacuumControl::setManualDuty(float percent) {
  targetActive_ = false;
  pid_.reset();
  percent = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  if (percent <= 0.0f) {
    pump_.stop();
    output_ = 0.0f;
    setState(ControlState::Idle);
    return;
  }
  pump_.setDuty(percent);
  output_ = percent;
  setState(ControlState::Manual);
}

void VacuumControl::stop() {
  targetActive_ = false;
  pid_.reset();
  output_ = 0.0f;
  pump_.stop();
  setState(fault_ ? ControlState::Fault : ControlState::Idle);
}

void VacuumControl::clearFault() {
  fault_ = false;
  pump_.resetFault();
  if (state_ == ControlState::Fault) setState(ControlState::Idle);
}

void VacuumControl::update(uint32_t nowMs) {
  if (pump_.tripped()) {
    targetActive_ = false;
    setState(ControlState::Fault);
    return;
  }
  if (fault_) return;

  if (!targetActive_) {
    if (state_ == ControlState::Pumping || state_ == ControlState::Holding) {
      state_ = pump_.running() ? ControlState::Manual : ControlState::Idle;
    }
    return;
  }

  if (!sensor_.present() || !sensor_.readingValid()) {
    LOGE("control: pressure reading unavailable, stopping pump");
    pump_.emergencyStop("sensor", "pressure sensor unavailable");
    targetActive_ = false;
    fault_ = true;
    setState(ControlState::Fault);
    return;
  }

  if (nowMs - lastControlMs_ < cfg::CONTROL_PERIOD_MS) return;
  float dt = (float)(nowMs - lastControlMs_) / 1000.0f;
  lastControlMs_ = nowMs;
  if (dt <= 0.0f) return;

  float measured = sensor_.kpa();
  float error = measured - targetKpa_;  // positive means "still needs vacuum"
  lastError_ = error;

  if (error < -cfg::CONTROL_DEADBAND_KPA) {
    // Deeper than requested; the pump cannot vent, so coast and let it bleed up.
    pid_.reset();
    output_ = 0.0f;
    pump_.stop();
    setState(ControlState::Holding);
    lastMeasured_ = measured;
    lastDerivativeMs_ = nowMs;
    return;
  }

  float controlError = fabsf(error) <= cfg::CONTROL_DEADBAND_KPA ? 0.0f : error;

  float slew = 0.0f;
  uint32_t windowMs = nowMs - lastDerivativeMs_;
  if (windowMs > 0 && windowMs <= (uint32_t)kDerivativeWindowMs) {
    slew = (measured - lastMeasured_) / ((float)windowMs / 1000.0f);
  }

  float output = pid_.update(dt, controlError, slew);
  output_ = clampf(output, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  pump_.setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
  setState(output_ > 0.5f ? ControlState::Pumping : ControlState::Holding);
}

}  // namespace eggplan
