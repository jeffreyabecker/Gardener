// Pressure sensor and key check for an ESP32-S3 Super Mini.
//
// The firmware's libraries, one ADC pin, the key register's three lines and the USB serial port: no
// data image and no display. The sensor is the firmware's own XgzpSensor, so what the bench reports
// is what the device reads - the pin in volts every second while the ambient reference settles, then
// the vacuum depth in kPa that the driver measures from it. The keys come through the firmware's own
// ShiftRegisterButtons and GestureClassifier, so the presses and chords printed here are the ones the
// device would act on. The libraries are the sketch's own, which is why it is compiled the way the
// firmware is: `arduino-cli compile --libraries libraries firmware/test_pressure`.
//5611-XGZP6847A100KPGN33-ND
#include <Gardener.h>
#include <Gfx.h>
#include <Runtime.h>

#include "ButtonsTest.h"
#include "SensorTest.h"
#include "hal/impl/XgzpSensor.h"

namespace {

// The one pin this sketch reads.
constexpr uint8_t SensorPin = 7;

// The key register's three lines: /load, clk and qh. A mask's bit number is its place in the read,
// so bit 0 is the level seen before the first clock.
constexpr uint8_t KeyLoadPin = 10;
constexpr uint8_t KeyClkPin = 11;
constexpr uint8_t KeyQhPin = 1;

// Which bit each key sits on, indexed by Button: up, down, forward and back on the top four bits and
// ok and cancel on the bottom two. Select and start are on no input, so they never report a press.
constexpr uint8_t KeyMask[gardener::ButtonCount] = {128, 64, 16, 32, 1, 2, 0, 0};

SensorTest sensor(SensorPin);
ButtonsTest keys(KeyLoadPin, KeyClkPin, KeyQhPin, KeyMask);

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
//     .keyMask = {128, 64, 16, 32, 1, 2, 0, 0},
// };

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  sensor.setup();
  keys.setup();
}

void loop() {
  sensor.loop();
  keys.loop();
}
