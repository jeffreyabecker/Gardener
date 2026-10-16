#include "app/VacuumSystem.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "core/Math.h"

namespace gardener {
namespace {
constexpr uint32_t kDerivativeWindowMs = 1000;
constexpr uint32_t kMaxPumpStepMs = 250;  // a long gap is not a reason to slam the duty
constexpr uint8_t kMedianWindow = 5;
constexpr float kFilterAlpha = 0.3f;
}  // namespace

// A latched fault owns the state until it is reset: a command can still make the plant safe,
// but none of them can move the state off Fault, so state() == Fault and faulted() agree.
void VacuumSystem::setState(RunState state) {
  if (faulted() && state != RunState::Fault) return;
  state_ = state;
}

// The sensor first, because it is what the loop regulates on and the only part with nothing to
// fall back on. Then the pump and the valve, and finally the safe resting state, which is where
// a plant that has just come up has to be: pump off, valve released.
bool VacuumSystem::begin(TwoWire& wire) {
  ambientKpa_ = cfg::DEFAULT_AMBIENT_KPA;
  sensorPresent_ = sensor_.begin(pins::MPRLS_I2C_ADDR, &wire);
  if (sensorPresent_) {
    delay(15);  // first conversion after power up
    float kpa = 0.0f;
    if (readNow(kpa)) {
      filteredKpa_ = kpa;
      readingValid_ = true;
    }
  }

  pinMode(pins::PUMP_PWM, OUTPUT);
  digitalWrite(pins::PUMP_PWM, LOW);
  pumpReady_ = ledcAttach(pins::PUMP_PWM, pins::PUMP_PWM_FREQ_HZ, pins::PUMP_PWM_RESOLUTION);
  if (pumpReady_) writeDuty(0.0f);

  // Released with no drive: a normally open valve needs nothing to be safe.
  pinMode(pins::VENT_VALVE, OUTPUT);
  valveOpen_ = true;
  writeValve(false);

  rest();
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;

  // The reference is taken from the sensor and never dialled in. Off a part that never answered
  // there is nothing to capture, so the default stays until one does.
  if (sensorPresent_) captureAmbient();

  return sensorPresent_;
}

PlantState VacuumSystem::state() const {
  PlantState plant;
  plant.runState = state_;
  plant.kpa = filteredKpa_;
  plant.vacuumKpa = currentKpa();
  plant.vacuumPercent = vacuumPercent();
  plant.ambientKpa = ambientKpa_;
  plant.targetKpa = targetKpa_;
  plant.duty = pumpDuty_;
  plant.sensorOk = sensorPresent_;
  plant.faultCode = faultCode_;
  return plant;
}

float VacuumSystem::currentKpa() const {
  if (!readingValid_) return 0.0f;
  float deficit = ambientKpa_ - filteredKpa_;
  return deficit > 0.0f ? deficit : 0.0f;
}

float VacuumSystem::vacuumPercent() const {
  if (!readingValid_ || ambientKpa_ <= 0.0f) return 0.0f;
  return clampf(currentKpa() / ambientKpa_ * 100.0f, 0.0f, 100.0f);
}

// The reference is captured and never dialled in: one source for it, so nothing can leave the
// rig regulating against a number nobody measured.
void VacuumSystem::captureAmbient() {
  if (!readingValid_) return;
  setAmbientKpa(filteredKpa_);
}

bool VacuumSystem::setAmbientKpa(float kpa) {
  if (kpa < 10.0f || kpa > pins::MPRLS_MAX_KPA) return false;
  ambientKpa_ = kpa;
  return true;
}

bool VacuumSystem::readNow(float& outKpa) {
  float kpa = sensor_.readPressure() * 0.1f;  // the library reports hPa
  if (isnan(kpa) || kpa <= pins::MPRLS_MIN_KPA || kpa > pins::MPRLS_MAX_KPA) return false;
  outKpa = kpa;
  return true;
}

void VacuumSystem::pollSensor(uint32_t nowMs) {
  if (!sensorPresent_) return;
  if (nowMs - lastPollMs_ < pins::MPRLS_POLL_MS) return;
  lastPollMs_ = nowMs;

  float kpa = 0.0f;
  if (!readNow(kpa)) return;

  window_[windowIndex_] = kpa;
  windowIndex_ = (uint8_t)((windowIndex_ + 1) % kMedianWindow);
  if (windowCount_ < kMedianWindow) windowCount_++;

  // Median of the last few samples kills the occasional MPRLS outlier.
  float sorted[kMedianWindow];
  memcpy(sorted, window_, sizeof(sorted));
  for (uint8_t i = 1; i < windowCount_; i++) {
    float key = sorted[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  float median = sorted[windowCount_ / 2];

  if (!readingValid_) {
    filteredKpa_ = median;
    readingValid_ = true;
  } else {
    filteredKpa_ += kFilterAlpha * (median - filteredKpa_);
  }
}

// The ramp and the cap are what keeps a step from 0 to full away from a diaphragm pump. Only a
// duty the loop asked for ramps: fail() writes zero straight out.
void VacuumSystem::setDuty(float percent) {
  pumpTarget_ = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
}

void VacuumSystem::updatePump(uint32_t nowMs) {
  if (lastPumpMs_ == 0) lastPumpMs_ = nowMs;
  uint32_t dt = nowMs - lastPumpMs_;
  if (dt > kMaxPumpStepMs) dt = kMaxPumpStepMs;
  lastPumpMs_ = nowMs;

  if (pumpDuty_ == pumpTarget_) return;

  float maxStep = cfg::PUMP_RAMP_PERCENT_PER_SEC * ((float)dt / 1000.0f);
  if (maxStep < 0.5f) maxStep = 0.5f;
  pumpDuty_ += clampf(pumpTarget_ - pumpDuty_, -maxStep, maxStep);
  if (fabsf(pumpDuty_ - pumpTarget_) < 0.05f) pumpDuty_ = pumpTarget_;
  writeDuty(pumpDuty_);
}

void VacuumSystem::writeDuty(float percent) {
  const uint32_t maxDuty = (1u << pins::PUMP_PWM_RESOLUTION) - 1u;
  float limited = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  uint32_t duty = (uint32_t)((limited / 100.0f) * (float)maxDuty + 0.5f);
  ledcWrite(pins::PUMP_PWM, duty);
}

void VacuumSystem::openValve() {
  if (valveOpen_) return;
  valveOpen_ = true;
  writeValve(false);
}

void VacuumSystem::closeValve() {
  if (!valveOpen_) return;
  valveOpen_ = false;
  writeValve(true);
}

void VacuumSystem::writeValve(bool energise) {
  digitalWrite(pins::VENT_VALVE, energise ? HIGH : LOW);
}

void VacuumSystem::rest() {
  setDuty(0.0f);
  openValve();  // normally open, but assert the safe resting state when asked
  pidReset();
  output_ = 0.0f;
  setState(RunState::Idle);
}

void VacuumSystem::vent() {
  setDuty(0.0f);
  openValve();
  pidReset();
  output_ = 0.0f;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

bool VacuumSystem::start() {
  if (faulted()) return false;
  if (targetKpa_ <= 0.0f) return false;
  if (!sensorPresent_ || !readingValid_) return false;

  // Close before pumping, so the pump never pulls against an open port.
  closeValve();
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  setState(RunState::Pulling);
  return true;
}

void VacuumSystem::stop() {
  // An idle plant is already released, so there is nothing to vent.
  if (state_ == RunState::Idle) {
    rest();
    return;
  }
  vent();
}

bool VacuumSystem::vented() const {
  bool settled =
      sensorPresent_ && readingValid_ && currentKpa() <= cfg::VENT_MARGIN_KPA;
  return settled || (millis() - ventStartMs_) >= cfg::VENT_TIMEOUT_MS;
}

void VacuumSystem::setTarget(float kpa) {
  // Shallower than the loop's floor is not a target. Deeper than the pump can reach still is
  // one: the duty pins at the cap and the run simply never gets there.
  if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) return;
  targetKpa_ = kpa;
  pidReset();
}

void VacuumSystem::clearTarget() {
  targetKpa_ = 0.0f;
  pidReset();
  output_ = 0.0f;

  // A run without a target is not a run, so clearing one releases the chamber. An idle plant
  // is already at rest, and a fault owns the state until it is reset.
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    vent();
  } else if (state_ == RunState::Idle) {
    rest();
  }
}

bool VacuumSystem::atTarget() const {
  return currentKpa() >= targetKpa_ - cfg::CONTROL_DEADBAND_KPA;
}

void VacuumSystem::resetFault() {
  faultCode_ = "";
  // The latch is what held the state, so clearing it has to put the machine somewhere: a
  // fault is cleared by releasing the chamber, which is where one leaves it anyway.
  if (state_ == RunState::Fault) vent();
}

// Both the pump's "output off" latch and the run's Fault state, because with the pump in here
// there is nobody else to latch one: cutting the output is part of failing, not a thing the pump
// does to the plant. Nothing writes a duty again while the code is set.
void VacuumSystem::fail(const char* code) {
  pumpTarget_ = 0.0f;
  pumpDuty_ = 0.0f;
  writeDuty(0.0f);
  openValve();  // release the coil: the safe direction
  pidReset();
  output_ = 0.0f;
  faultCode_ = code ? code : "unknown";
  setState(RunState::Fault);
}

void VacuumSystem::service(uint32_t nowMs) {
  pollSensor(nowMs);
  updatePump(nowMs);

  // A latched fault owns the state until it is reset: the plant is already safe, and the code
  // is what `status` reports.
  if (faulted()) return;

  const bool regulating = state_ == RunState::Pulling || state_ == RunState::Holding;

  // Regulating on a reading nobody trusts is how the loop runs away.
  if (regulating && (!sensorPresent_ || !readingValid_)) {
    fail("sensor");
    return;
  }

  // The two transitions that are facts about the chamber rather than orders: the target has
  // been reached, and the release is over. A latched fault owns the state, so neither can
  // fire while one is held.
  if (state_ == RunState::Pulling && atTarget()) setState(RunState::Holding);
  if (state_ == RunState::Venting && vented()) {
    rest();
    return;
  }

  if (regulating) regulate(nowMs);
}

// error is how far the chamber still is from the target, so positive means "still needs
// vacuum"; the derivative term uses the measurement slew rather than the error slope, which
// keeps a setpoint change from kicking the output.
float VacuumSystem::pidUpdate(float dt, float error, float measurementSlew) {
  if (dt <= 0.0f) return cfg::PID_KP * error + pidIntegral_;
  pidIntegral_ = clampf(pidIntegral_ + cfg::PID_KI * error * dt, -cfg::PID_INTEGRAL_LIMIT,
                        cfg::PID_INTEGRAL_LIMIT);
  return cfg::PID_KP * error + pidIntegral_ - cfg::PID_KD * measurementSlew;
}

void VacuumSystem::regulate(uint32_t nowMs) {
  if (nowMs - lastControlMs_ < cfg::CONTROL_PERIOD_MS) return;

  float dt = (float)(nowMs - lastControlMs_) / 1000.0f;
  lastControlMs_ = nowMs;
  if (dt <= 0.0f) return;

  // The slew comes off the raw reading: the deficit is that reading offset by the reference, so
  // the two move in opposite directions and PID_KD is signed against this one.
  float measured = filteredKpa_;
  float error = targetKpa_ - currentKpa();  // positive means "still needs vacuum"

  if (error < -cfg::CONTROL_DEADBAND_KPA) {
    // Deeper than requested; the pump cannot vent, so coast and let it bleed up.
    pidReset();
    output_ = 0.0f;
    setDuty(0.0f);
    lastMeasured_ = measured;
    lastDerivativeMs_ = nowMs;
    return;
  }

  float controlError = fabsf(error) <= cfg::CONTROL_DEADBAND_KPA ? 0.0f : error;

  float slew = 0.0f;
  uint32_t windowMs = nowMs - lastDerivativeMs_;
  if (windowMs > 0 && windowMs <= kDerivativeWindowMs) {
    slew = (measured - lastMeasured_) / ((float)windowMs / 1000.0f);
  }

  output_ = clampf(pidUpdate(dt, controlError, slew), 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
}

}  // namespace gardener
