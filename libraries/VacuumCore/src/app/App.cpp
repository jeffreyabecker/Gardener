#include "app/App.h"

#include <Wire.h>

#include "hal/Pins.h"

namespace gardener {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

App::App()
    : panel_(pins::OLED_WIDTH, pins::OLED_HEIGHT, &Wire, -1),
      display_(panel_),
      overlay_(display_, snapshot_),
      system_(pump_, sensor_, vent_),
      link_(system_, overlay_, Serial, serialAttached, snapshot_) {}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  link_.info(boot.c_str());

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, pins::I2C_FREQ_HZ);

  // The STEMMA QT breakout has no reset line, so do not let the library poke one.
  displayOk_ = display_.begin();
  bool sensorOk = sensor_.begin(Wire);
  bool pumpOk = pump_.begin();
  vent_.begin();
  bool batteryOk = battery_.begin();
  system_.begin();

  if (sensorOk) sensor_.captureAmbient();

  // What came up, and what did not. This is the composition root's job now: the drivers
  // report as values and one place decides what is worth saying.
  link_.info(displayOk_ ? "oled: online" : "oled: not found");
  link_.info(sensorOk ? "mprls: online" : "mprls: not found");
  link_.info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  link_.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  link_.info(ready.c_str());

  refreshSnapshot();
  link_.begin();
  lastState_ = system_.state();
  if (displayOk_) splash();
  return true;
}

// Drawn once, at startup, and again after a reboot because that command restarts the device and
// begin() runs again. The battery is on it because it flushes through the overlay - and it is a
// real reading, since battery_.begin() takes a sample before this point.
void App::splash() {
  overlay_.clear();
  overlay_.text(0, 0, "Gardener", 1, false);
  overlay_.text(0, 12, String("fw ") + cfg::FIRMWARE_VERSION, 1, false);
  overlay_.flush();
}

void App::refreshSnapshot() {
  snapshot_.kpa = system_.kpa();
  snapshot_.vacuumKpa = system_.vacuumKpa();
  snapshot_.vacuumPercent = system_.vacuumPercent();
  snapshot_.ambientKpa = system_.ambientKpa();
  snapshot_.sensorOk = system_.sensorOk();
  snapshot_.readingValid = system_.readingValid();

  // faulted() is the trip as the plant sees it: it latches when the trip is observed and
  // clears with resetFault, so it says the same thing pump.tripped() said.
  snapshot_.duty = system_.duty();
  snapshot_.pumpTripped = system_.faulted();
  snapshot_.faultCode = system_.faultCode();

  snapshot_.state = system_.state();
  snapshot_.targetActive = system_.targetActive();
  snapshot_.targetKpa = system_.targetKpa();
  snapshot_.floorKpa = system_.minReachableKpa();

  snapshot_.batteryPresent = battery_.present();
  snapshot_.batteryLow = battery_.low();
  snapshot_.batteryVolts = battery_.volts();
  snapshot_.batteryPercent = battery_.percent();

  snapshot_.uptimeMs = millis();
}

// The drivers stopped narrating, so the run is reported from here: one line per state
// change, timed when it leaves a vent, plus the fault code when one latches.
void App::logRunChanges(uint32_t nowMs) {
  RunState state = system_.state();
  if (state == lastState_) return;

  String message = runStateName(lastState_);
  message += " -> ";
  message += runStateName(state);
  if (state == RunState::Fault) {
    message += " (fault ";
    message += pump_.faultCode();
    message += ')';
  }
  if (lastState_ == RunState::Venting) {
    uint32_t ventMs = nowMs - ventStartMs_;
    message += " after ";
    message += String(ventMs);
    message += " ms";
    if (ventMs >= cfg::VENT_TIMEOUT_MS) {
      message += ", vent timed out";
      link_.warn(message.c_str());
      lastState_ = state;
      return;
    }
  }
  if (state == RunState::Venting) ventStartMs_ = nowMs;
  link_.info(message.c_str());
  lastState_ = state;
}

void App::update() {
  uint32_t now = millis();

  sensor_.update(now);
  pump_.update(now);
  battery_.update(now);
  system_.service(now);

  // Commands first, against the machine as the last control tick left it, so a reply
  // describes something that actually happened rather than a view built mid-command.
  link_.poll(now);

  // A controller that goes away releases the chamber (Q4).
  if (link_.consumeLinkLost()) {
    link_.warn("app: controller left, stopping and venting");
    system_.stop();
  }

  logRunChanges(now);
  refreshSnapshot();
  // Nothing is drawn here. The panel belongs to the host from now on, and the device only puts
  // its own two things on it: the splash, once, and the battery, on the way to every flush.
}

}  // namespace gardener
