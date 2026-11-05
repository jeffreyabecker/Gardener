// Pressure sensor check for an ESP32-S3 Super Mini.
//
// The firmware's libraries, one ADC pin and the USB serial port: no data image and no panel. The
// sensor is the firmware's own XgzpSensor, so what the bench reports is what the device reads - the
// pin in volts every second while the ambient reference settles, then the vacuum depth in kPa that
// the driver measures from it. The libraries are the sketch's own, which is why it is compiled the
// way the firmware is: `arduino-cli compile --libraries libraries firmware/test_pressure`.
//5611-XGZP6847A100KPGN33-ND
#include <Gardener.h>
#include <Gfx.h>
#include <Runtime.h>

#include "hal/impl/XgzpSensor.h"

namespace {

// The one pin this sketch reads.
constexpr uint8_t SensorPin = 7;

SensorTest sensor(SensorPin);

// constexpr gardener::PanelPins Board{
//     .machine = {.sensorAdc = 7, .pumpPwm = 6, .ventValve = 5},
//     .i2cSda = 17,
//     .i2cScl = 18,
//     .i2cHz = 400000,
//     .buzzerPwm = 12,
//     .batteryAdc = 4,
//     .batteryDivider = 2.0f,
//     .keyLatch = 10,
//     .keyClock = 11,
//     .keyData = 1,
// };

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  sensor.setup();
}

void loop() { sensor.loop(); }
