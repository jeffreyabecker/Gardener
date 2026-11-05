#pragma once
#include <Gardener.h>
#include <Gfx.h>
#include <Runtime.h>

#include "hal/impl/PwmBuzzer.h"

class BuzzerTest {
 public:
  constexpr static uint32_t ReportMs = 1000;
  BuzzerTest(uint8_t pin) : buzzer_(pin) {}

  void setup() {
    if (!buzzer_.begin()) Serial.println("buzzer: no part on the pin");
  }

  void loop() {
    const uint32_t nowMs = millis();
    if (nowMs - lastReportMs_ < ReportMs) return;
    lastReportMs_ = nowMs;

    if (!buzzer_.sounding()) {
      buzzer_.tone(440);
      Serial.println("buzzer: tone on");
    } else {
      buzzer_.off();
      Serial.println("buzzer: tone off");
    }
  }

 private:
  gardener::runtime::PwmBuzzer buzzer_;
  uint32_t lastReportMs_ = 0;
};