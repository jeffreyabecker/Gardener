#include "app/App.h"

#include <Wire.h>

#include "core/Text.h"
#include "hal/Pins.h"
#include "services/SnapshotBuilder.h"

namespace eggplan {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

const char* boolToken(bool value) { return value ? "1" : "0"; }

}  // namespace

App::App()
    : pump_(pumpConfig()),
      vent_(ventConfig()),
      battery_(batteryConfig()),
      display_(Wire, pins::OLED_I2C_ADDR, pins::OLED_WIDTH, pins::OLED_HEIGHT),
      control_(pump_, sensor_, vent_, pidConfig()),
      link_(),
      renderer_(display_),
      router_(control_, pump_, sensor_, link_),
      serial_(router_, Serial, serialAttached, link_, snapshot_) {}

PumpConfig App::pumpConfig() {
  PumpConfig config;
  config.pwmPin = pins::PUMP_PWM;
  config.tachPin = pins::TACH_ENABLED ? pins::PUMP_TACH : -1;
  config.tachPulsesPerRev = pins::TACH_PULSES_PER_REV;
  config.pwmFreqHz = pins::PUMP_PWM_FREQ_HZ;
  config.pwmResolution = pins::PUMP_PWM_RESOLUTION;
  return config;
}

VentValveConfig App::ventConfig() {
  VentValveConfig config;
  config.pin = pins::VENT_VALVE;
  config.activeHigh = true;
  return config;
}

BatteryConfig App::batteryConfig() {
  BatteryConfig config;
  config.adcPin = pins::BATTERY_ADC;
  config.dividerRatio = pins::BATTERY_DIVIDER_RATIO;
  config.emptyVolts = cfg::BATTERY_EMPTY_VOLTS;
  config.fullVolts = cfg::BATTERY_FULL_VOLTS;
  config.calibration = cfg::BATTERY_CALIBRATION;
  config.lowPercent = cfg::BATTERY_LOW_PERCENT;
  return config;
}

PidConfig App::pidConfig() {
  PidConfig config;
  config.kp = cfg::PID_KP;
  config.ki = cfg::PID_KI;
  config.kd = cfg::PID_KD;
  config.integralLimit = cfg::PID_INTEGRAL_LIMIT;
  return config;
}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  char line[96];
  text::Buf boot(line, sizeof(line));
  boot << "eggplan-vacuum " << cfg::FIRMWARE_VERSION << " booting";
  serial_.info(line);

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, pins::I2C_FREQ_HZ);

  bool displayOk = display_.begin();
  bool sensorOk = sensor_.begin(Wire);
  bool pumpOk = pump_.begin();
  vent_.begin();
  bool batteryOk = battery_.begin();
  control_.begin();
  renderer_.begin();
  device_ = readDeviceInfo();

  if (sensorOk) sensor_.captureAmbient();

  // What came up, and what did not. This is the composition root's job now: the drivers
  // report as values and one place decides what is worth saying.
  serial_.info(displayOk ? "oled: online" : "oled: not found");
  serial_.info(sensorOk ? "mprls: online" : "mprls: not found");
  serial_.info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  serial_.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");

  text::Buf ready(line, sizeof(line));
  ready << "app: ready, heap " << (uint32_t)ESP.getFreeHeap();
  serial_.info(line);

  refreshSnapshot();
  serial_.begin();
  lastState_ = control_.state();
  if (displayOk) renderer_.render(snapshot_);
  return true;
}

void App::refreshSnapshot() {
  snapshot_ = buildSnapshot(pump_, sensor_, control_, battery_, device_);
}

// The drivers stopped narrating, so the run is reported from here: one line per state
// change, timed when it leaves a vent, plus the fault code when one latches.
void App::logRunChanges(uint32_t nowMs) {
  RunState state = control_.state();
  if (state == lastState_) return;

  char line[96];
  text::Buf message(line, sizeof(line));
  message << runStateName(lastState_) << " -> " << runStateName(state);
  if (state == RunState::Fault) {
    message << " (fault " << pump_.faultCode() << ')';
  }
  if (lastState_ == RunState::Venting) {
    uint32_t ventMs = nowMs - ventStartMs_;
    message << " after " << ventMs << " ms";
    if (ventMs >= cfg::VENT_TIMEOUT_MS) {
      message << ", vent timed out";
      serial_.warn(line);
      lastState_ = state;
      return;
    }
  }
  if (state == RunState::Venting) ventStartMs_ = nowMs;
  serial_.info(line);
  lastState_ = state;
}

void App::update() {
  uint32_t now = millis();

  sensor_.update(now);
  pump_.update(now);
  battery_.update(now);
  control_.update(now);

  // Commands first, against the machine as the last control tick left it, so a reply
  // describes something that actually happened rather than a view built mid-command.
  serial_.poll(now);

  // A controller that goes away releases the chamber (Q4).
  if (serial_.consumeLinkLost()) {
    serial_.warn("app: controller left, stopping and venting");
    control_.stopRun();
  }

  logRunChanges(now);
  refreshSnapshot();
  renderer_.render(snapshot_);
}

}  // namespace eggplan
