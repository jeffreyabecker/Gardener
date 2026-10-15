#include "services/VacuumControl.h"

#include <math.h>

#include "core/Log.h"
#include "core/Math.h"
#include "hal/Pump.h"
#include "hal/PressureSensor.h"
#include "hal/VentValve.h"

namespace eggplan {
namespace {
constexpr uint32_t kDerivativeWindowMs = 1000;
}

void VacuumControl::begin() {
  state_ = RunState::Idle;
  pump_.stop();
  vent_.open();  // normally open, but assert the safe resting state once
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
}

float VacuumControl::minReachableKpa() const {
  float ambient = sensor_.ambientKpa();
  float floorKpa = ambient - cfg::PUMP_MAX_SUCTION_KPA;
  return floorKpa > cfg::CONTROL_MIN_TARGET_KPA ? floorKpa : cfg::CONTROL_MIN_TARGET_KPA;
}

void VacuumControl::setState(RunState state) {
  if (state_ == state) return;
  LOGI("control: %s -> %s", runStateName(state_), runStateName(state));
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
  LOGI("control: target %.2f kPa", kpa);
}

void VacuumControl::clearTarget() {
  targetActive_ = false;
  pid_.reset();
  output_ = 0.0f;
  remainingSeconds_ = 0;
  // A run without a target is not a run, so clearing one releases the chamber.
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    beginVent();
  } else if (state_ == RunState::Idle) {
    pump_.stop();
    vent_.open();
  }
}

void VacuumControl::setDurationSeconds(uint32_t seconds) {
  durationSeconds_ = seconds;
  if (state_ == RunState::Holding) {
    // A timer set while holding counts from now.
    countdownEndMs_ = millis() + seconds * 1000UL;
    remainingSeconds_ = seconds;
  }
}

bool VacuumControl::startRun() {
  if (faultLatched_ || state_ == RunState::Fault) {
    LOGW("control: start refused, fault is latched (send reset)");
    return false;
  }
  if (!targetActive_) {
    LOGW("control: start refused, no target set");
    return false;
  }
  if (!sensor_.present() || !sensor_.readingValid()) {
    LOGW("control: start refused, no pressure reading");
    return false;
  }

  // Close before pumping, so the pump never pulls against an open port.
  vent_.close();
  pid_.reset();
  output_ = 0.0f;
  remainingSeconds_ = durationSeconds_;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  setState(RunState::Pulling);
  LOGI("control: run started, target %.2f kPa, timer %us", targetKpa_,
       (unsigned)durationSeconds_);
  return true;
}

void VacuumControl::stopRun() {
  if (state_ == RunState::Idle) {
    pump_.stop();
    vent_.open();
    return;
  }
  beginVent();
}

void VacuumControl::clearFault() {
  pump_.resetFault();
  faultLatched_ = false;
  if (state_ == RunState::Fault) beginVent();
}

bool VacuumControl::consumeCompleted() {
  bool completed = completed_;
  completed_ = false;
  return completed;
}

void VacuumControl::beginVent() {
  pump_.stop();
  vent_.open();
  pid_.reset();
  output_ = 0.0f;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

void VacuumControl::fault(const char* code, const char* text) {
  pump_.emergencyStop(code, text);
  vent_.open();  // release the coil: the safe direction
  pid_.reset();
  output_ = 0.0f;
  remainingSeconds_ = 0;
  faultLatched_ = true;
  setState(RunState::Fault);
}

float VacuumControl::step(uint32_t nowMs) {
  if (nowMs - lastControlMs_ < cfg::CONTROL_PERIOD_MS) return sensor_.kpa();

  float dt = (float)(nowMs - lastControlMs_) / 1000.0f;
  lastControlMs_ = nowMs;
  if (dt <= 0.0f) return sensor_.kpa();

  float measured = sensor_.kpa();
  float error = measured - targetKpa_;  // positive means "still needs vacuum"
  lastError_ = error;

  if (error < -cfg::CONTROL_DEADBAND_KPA) {
    // Deeper than requested; the pump cannot vent, so coast and let it bleed up.
    pid_.reset();
    output_ = 0.0f;
    pump_.stop();
    lastMeasured_ = measured;
    lastDerivativeMs_ = nowMs;
    return measured;
  }

  float controlError = fabsf(error) <= cfg::CONTROL_DEADBAND_KPA ? 0.0f : error;

  float slew = 0.0f;
  uint32_t windowMs = nowMs - lastDerivativeMs_;
  if (windowMs > 0 && windowMs <= kDerivativeWindowMs) {
    slew = (measured - lastMeasured_) / ((float)windowMs / 1000.0f);
  }

  output_ = clampf(pid_.update(dt, controlError, slew), 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  pump_.setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
  return measured;
}

void VacuumControl::update(uint32_t nowMs) {
  // A latched pump fault is terminal until the controller resets it.
  if (pump_.tripped()) {
    if (state_ != RunState::Fault) fault(pump_.faultCode(), pump_.faultText());
    return;
  }

  switch (state_) {
    case RunState::Idle:
    case RunState::Fault:
      return;

    case RunState::Pulling:
    case RunState::Holding: {
      if (!sensor_.present() || !sensor_.readingValid()) {
        LOGE("control: pressure reading lost, stopping and venting");
        fault("sensor", "pressure sensor unavailable");
        return;
      }

      float measured = step(nowMs);

      if (state_ == RunState::Pulling &&
          measured <= targetKpa_ + cfg::CONTROL_DEADBAND_KPA) {
        // The countdown starts here (Q1), so the number means "hold time remaining".
        countdownEndMs_ = nowMs + durationSeconds_ * 1000UL;
        remainingSeconds_ = durationSeconds_;
        LOGI("control: target reached at %.2f kPa", measured);
        setState(RunState::Holding);
        return;
      }

      if (state_ == RunState::Holding) {
        if (durationSeconds_ == 0) {
          remainingSeconds_ = 0;
          return;
        }
        uint32_t leftMs = countdownEndMs_ > nowMs ? countdownEndMs_ - nowMs : 0;
        remainingSeconds_ = (leftMs + 999) / 1000;
        if (leftMs == 0) {
          LOGI("control: timer expired, venting");
          completed_ = true;
          beginVent();
        }
      }
      return;
    }

    case RunState::Venting: {
      bool settled = sensor_.present() && sensor_.readingValid() &&
                     sensor_.kpa() >= sensor_.ambientKpa() - cfg::VENT_MARGIN_KPA;
      bool timedOut = (nowMs - ventStartMs_) >= cfg::VENT_TIMEOUT_MS;
      if (!settled && !timedOut) return;

      if (timedOut && !settled) {
        LOGW("control: vent timed out after %ums, releasing anyway",
             (unsigned)cfg::VENT_TIMEOUT_MS);
      }
      remainingSeconds_ = 0;
      setState(RunState::Idle);
      return;
    }
  }
}

}  // namespace eggplan
