#pragma once
#include <Gardener.h>
#include <Gfx.h>
#include <Runtime.h>

#include "hal/impl/XgzpSensor.h"

class SensorTest {
 public:
  constexpr static uint32_t ReportMs = 1000;
  SensorTest(uint8_t pin) : sensor_(pin) {}

  void setup() {
    if (!sensor_.begin()) Serial.println("sensor: no part on the pin");
  }

  void loop() {
    const uint32_t nowMs = millis();
    sensor_.update(nowMs);

    if (nowMs - lastReportMs_ < ReportMs) return;
    lastReportMs_ = nowMs;

    if (!sensor_.present()) {
      Serial.println("sensor: no part on the pin");
      return;
    }

    if (!sensor_.settled()) {
      String line = "adc ";
      line += String(sensor_.volts(), 3);
      line += " V, settling to an ambient ";
      line += String(sensor_.ambientVolts(), 3);
      line += " V";
      Serial.println(line);
      return;
    }

    if (!reportedAmbient_) {
      String line = "ambient reference ";
      line += String(sensor_.ambientVolts(), 3);
      line += " V";
      Serial.println(line);
      reportedAmbient_ = true;
    }

    String line = "vacuum depth ";
    line += String(sensor_.pressureKpa(), 3);
    line += " kPa";
    Serial.println(line);
  }

 private:
  gardener::runtime::XgzpSensor sensor_;
  uint32_t lastReportMs_ = 0;
  bool reportedAmbient_ = false;
};