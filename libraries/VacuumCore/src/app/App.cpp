#include "app/App.h"

#include <Wire.h>

#include "core/Log.h"
#include "hal/Pins.h"
#include "services/SnapshotBuilder.h"

namespace eggplan {
namespace {

void serialLogSink(const char* text, size_t length) {
  Serial.write((const uint8_t*)text, length);
}

}  // namespace

App::App()
    : pump_(pumpConfig()),
      vent_(ventConfig()),
      battery_(batteryConfig()),
      display_(Wire, pins::OLED_I2C_ADDR, pins::OLED_WIDTH, pins::OLED_HEIGHT),
      control_(pump_, sensor_, vent_, pidConfig()),
      renderer_(display_),
      router_(control_, pump_, sensor_, telemetry_),
      ble_(router_, telemetry_, replies_, inbound_, snapshot_) {}

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
  Serial.begin(115200);
  logBegin(&serialLogSink);
  delay(200);
  LOGI("eggplan-vacuum %s booting", cfg::FIRMWARE_VERSION);

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, pins::I2C_FREQ_HZ);

  bool displayOk = display_.begin();
  bool sensorOk = sensor_.begin(Wire);
  pump_.begin();
  vent_.begin();
  battery_.begin();
  control_.begin();
  renderer_.begin();
  device_ = readDeviceInfo();

  if (sensorOk) sensor_.captureAmbient();
  refreshSnapshot();
  ble_.begin();

  LOGI("app: ready, heap %u bytes", (unsigned)ESP.getFreeHeap());
  if (displayOk) renderer_.render(snapshot_);
  return true;
}

void App::refreshSnapshot() {
  snapshot_ = buildSnapshot(pump_, sensor_, control_, battery_, device_);
  snapshot_.bleConnected = ble_.connected();
}

void App::update() {
  uint32_t now = millis();

  sensor_.update(now);
  pump_.update(now);
  battery_.update(now);
  control_.update(now);

  // A central that leaves or goes quiet releases the chamber (Q4).
  if (ble_.consumeLinkLost()) {
    LOGW("app: link lost, stopping and venting");
    control_.stopRun();
  }

  refreshSnapshot();
  renderer_.render(snapshot_);
  ble_.poll(now);
}

}  // namespace eggplan
