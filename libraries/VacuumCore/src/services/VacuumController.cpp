#include "services/VacuumController.h"

#include "core/Log.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "core/Util.h"

namespace eggplan {

void VacuumController::begin(AppContext& ctx) {
  ctx_ = &ctx;
  state_ = ControlState::Idle;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
}

const char* VacuumController::stateText() const {
  switch (state_) {
    case ControlState::Idle: return "IDLE";
    case ControlState::Pumping: return "PUMP";
    case ControlState::Holding: return "HOLD";
    case ControlState::Manual: return "MAN";
    case ControlState::Fault: return "FAULT";
    default: return "?";
  }
}

void VacuumController::setState(ControlState state) {
  if (state_ == state) return;
  LOGI("control: %s -> %s", stateText(), state == ControlState::Pumping ? "PUMP" :
       state == ControlState::Holding ? "HOLD" :
       state == ControlState::Manual ? "MAN" :
       state == ControlState::Fault ? "FAULT" : "IDLE");
  state_ = state;
}

float VacuumController::duty() const {
  return ctx_ && ctx_->pump ? ctx_->pump->dutyPercent() : 0.0f;
}

void VacuumController::setTarget(float kpa) {
  if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
    LOGW("control: refusing target %.2f kPa", kpa);
    return;
  }
  if (kpa > cfg::WORKING_RANGE_MAX_KPA) {
    LOGW("control: target %.1f kPa is above the %.0f kPa working range", kpa,
         cfg::WORKING_RANGE_MAX_KPA);
  }
  targetKpa_ = kpa;
  targetActive_ = true;
  integral_ = 0.0f;
  if (fault_) clearFault();
  LOGI("control: target %.2f kPa", kpa);
}

void VacuumController::disableTarget() {
  targetActive_ = false;
  integral_ = 0.0f;
  if (ctx_ && ctx_->pump) ctx_->pump->stop();
  setState(ControlState::Idle);
}

void VacuumController::setManualDuty(float percent) {
  if (!ctx_ || !ctx_->pump) return;
  targetActive_ = false;
  integral_ = 0.0f;
  percent = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  if (percent <= 0.0f) {
    ctx_->pump->stop();
    setState(ControlState::Idle);
    return;
  }
  ctx_->pump->setDuty(percent);
  setState(ControlState::Manual);
}

void VacuumController::stop() {
  targetActive_ = false;
  integral_ = 0.0f;
  output_ = 0.0f;
  if (ctx_ && ctx_->pump) ctx_->pump->stop();
  setState(fault_ ? ControlState::Fault : ControlState::Idle);
}

void VacuumController::clearFault() {
  fault_ = false;
  if (ctx_ && ctx_->pump) ctx_->pump->resetFault();
  if (state_ == ControlState::Fault) setState(ControlState::Idle);
}

void VacuumController::update(uint32_t nowMs) {
  if (!ctx_ || !ctx_->pump) return;

  if (ctx_->pump->tripped()) {
    targetActive_ = false;
    setState(ControlState::Fault);
    return;
  }
  if (fault_) return;

  if (!targetActive_) {
    if (state_ == ControlState::Pumping || state_ == ControlState::Holding) {
      setState(ctx_->pump->running() ? ControlState::Manual : ControlState::Idle);
    }
    return;
  }

  if (!ctx_->sensor || !ctx_->sensor->present() || !ctx_->sensor->readingValid()) {
    LOGE("control: pressure reading unavailable, stopping pump");
    ctx_->pump->emergencyStop("pressure sensor unavailable");
    targetActive_ = false;
    fault_ = true;
    setState(ControlState::Fault);
    return;
  }

  if (nowMs - lastControlMs_ < cfg::CONTROL_PERIOD_MS) return;
  float dt = (float)(nowMs - lastControlMs_) / 1000.0f;
  lastControlMs_ = nowMs;
  if (dt <= 0.0f) return;

  float measured = ctx_->sensor->kpa();
  float error = measured - targetKpa_;  // positive means "still needs vacuum"
  lastError_ = error;

  if (error < -cfg::CONTROL_DEADBAND_KPA) {
    // Deeper than requested; the pump cannot vent, so coast and bleed up.
    integral_ = 0.0f;
    output_ = 0.0f;
    ctx_->pump->stop();
    setState(ControlState::Holding);
    lastMeasured_ = measured;
    lastDerivativeMs_ = nowMs;
    return;
  }

  float controlError = (fabsf(error) <= cfg::CONTROL_DEADBAND_KPA) ? 0.0f : error;

  float derivative = 0.0f;
  uint32_t derivativeDt = nowMs - lastDerivativeMs_;
  if (derivativeDt > 0) {
    derivative = -(measured - lastMeasured_) / ((float)derivativeDt / 1000.0f);
  }

  integral_ = clampf(integral_ + cfg::PID_KI * controlError * dt,
                     -cfg::PID_INTEGRAL_LIMIT, cfg::PID_INTEGRAL_LIMIT);

  float output = cfg::PID_KP * controlError + integral_ + cfg::PID_KD * derivative;
  output_ = clampf(output, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  ctx_->pump->setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;

  if (output_ > 0.5f) {
    setState(ControlState::Pumping);
  } else {
    setState(ControlState::Holding);
  }
}

}  // namespace eggplan
