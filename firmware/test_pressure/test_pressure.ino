// Pressure sensor check for an ESP32-S3 Super Mini.
//
// No data image and no board: one ADC pin, the USB serial port and the firmware's own sensor driver,
// which XgzpSensor.cpp beside this sketch compiles out of libraries/Runtime. It reports the pin in
// volts every second while the ambient reference settles, then the vacuum depth in kPa that the
// driver measures from it.
//5611-XGZP6847A100KPGN33-ND
#include <Arduino.h>

#include "hal/impl/XgzpSensor.h"

namespace {

// The one pin this sketch reads.
constexpr uint8_t SensorPin = 7;

constexpr uint32_t ReportMs = 1000;

gardener::runtime::XgzpSensor sensor(SensorPin);

uint32_t lastReportMs = 0;
bool reportedAmbient = false;

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  // begin() takes the pin and the first sample; the ambient reference is the mean of the first ten
  // seconds after it, so the part is left alone over that window.
  if (!sensor.begin()) Serial.println("sensor: no part on the pin");
}

void loop() {
  // Polled as the firmware's tick polls it, once every 20 ms, so the filtering in the report below
  // is the filtering the device reads.
  const uint32_t nowMs = millis();
  sensor.update(nowMs);

  if (nowMs - lastReportMs < ReportMs) return;
  lastReportMs = nowMs;

  if (!sensor.present()) {
    Serial.println("sensor: no part on the pin");
    return;
  }

  if (!sensor.settled()) {
    String line = "adc ";
    line += String(sensor.volts(), 3);
    line += " V, settling to an ambient ";
    line += String(sensor.ambientVolts(), 3);
    line += " V";
    Serial.println(line);
    return;
  }
  if (!reportedAmbient) {
    reportedAmbient = true;
    String line = "ambient ";
    line += String(sensor.ambientVolts(), 3);
    line += " V";
    Serial.println(line);
  }

  String line = "adc ";
  line += String(sensor.volts(), 3);
  line += " V, ";
  line += String(sensor.pressureKpa(), 2);
  line += " kPa below ambient";
  Serial.println(line);
}
