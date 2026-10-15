#include "app/App.h"

#include <Wire.h>

#include "hal/Pins.h"

namespace eggplan {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

App::App()
    : panel_(pins::OLED_WIDTH, pins::OLED_HEIGHT, &Wire, -1),
      system_(pump_, sensor_, vent_),
      control_(system_),
      page_(panel_),
      link_(control_, system_, Serial, serialAttached, snapshot_) {}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  String boot = "eggplan-vacuum ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  link_.info(boot.c_str());

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, pins::I2C_FREQ_HZ);

  // The STEMMA QT breakout has no reset line, so do not let the library poke one.
  displayOk_ = panel_.begin(pins::OLED_I2C_ADDR, false);
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
  lastState_ = control_.state();
  if (displayOk_) {
    page_.splash();
    panel_.display();
  }
  return true;
}

void App::refreshSnapshot() {
  buildSnapshot(system_, control_, battery_, snapshot_);
}

// The drivers stopped narrating, so the run is reported from here: one line per state
// change, timed when it leaves a vent, plus the fault code when one latches.
void App::logRunChanges(uint32_t nowMs) {
  RunState state = control_.state();
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
  control_.update(now);

  // Commands first, against the machine as the last control tick left it, so a reply
  // describes something that actually happened rather than a view built mid-command.
  link_.poll(now);

  // A controller that goes away releases the chamber (Q4).
  if (link_.consumeLinkLost()) {
    link_.warn("app: controller left, stopping and venting");
    control_.stopRun();
  }

  logRunChanges(now);
  refreshSnapshot();
  if (displayOk_) {
    page_.render(snapshot_);
    panel_.display();
  }
}

}  // namespace eggplan
