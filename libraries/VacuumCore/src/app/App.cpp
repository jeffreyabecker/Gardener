#include "app/App.h"

#include <stdlib.h>

#include "core/Config.h"

namespace gardener {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

// The keys in the order a press is reported, which is the panel's order rather than the bit order.
constexpr Button kButtons[] = {Button::Up, Button::Down, Button::Ok, Button::Back};

}  // namespace

App::App(AppContext& context)
    : context_(context), link_(context, Serial, serialAttached), logger_(&link_) {}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger_->info(boot.c_str());

  // The panel is the one part with no fallback: nothing on the device can draw without it, so a
  // panel that does not answer is fatal rather than a degraded mode.
  bool displayOk = context_.display().begin();
  logger_->info(displayOk ? "oled: online" : "oled: not found, aborting");
  if (!displayOk) {
    // abort() panics and the ESP32 resets on a panic, so the device keeps trying rather than
    // coming up blind and silent.
    delay(50);
    abort();
  }

  bool sensorOk = context_.sensor().begin();
  bool pumpOk = context_.pump().begin();
  context_.vent().begin();
  bool batteryOk = context_.battery().begin();
  context_.vacuum().begin();

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger_->info(sensorOk ? "mprls: online" : "mprls: not found");
  logger_->info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger_->info(context_.vent().energiseWhenOpen() ? "vent: normally closed"
                                                   : "vent: normally open");
  logger_->info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger_->info("buttons: four keys on interrupt");

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger_->info(ready.c_str());

  link_.begin();
  lastState_ = context_.vacuum().runState();
  splash();
  return true;
}

// Drawn once, at startup, and again after a reboot because that command restarts the device and
// begin() runs again. The battery is on it because it flushes through the overlay - and it is a
// real reading, since App began the gauge before this point.
void App::splash() {
  Display& display = context_.display();
  display.clear();
  display.text(0, 0, "Gardener", 1, false);
  display.text(0, 12, String("fw ") + cfg::FIRMWARE_VERSION, 1, false);
  display.flush();
}

// The drivers stopped narrating, so the run is reported from here: one line per state
// change, timed when it leaves a vent, plus the fault code when one latches.
void App::logRunChanges(uint32_t nowMs) {
  RunState state = context_.vacuum().runState();
  if (state == lastState_) return;

  String message = runStateName(lastState_);
  message += " -> ";
  message += runStateName(state);
  if (state == RunState::Fault) {
    message += " (fault ";
    message += context_.vacuum().faultCode();
    message += ')';
  }
  if (lastState_ == RunState::Venting) {
    uint32_t ventMs = nowMs - ventStartMs_;
    message += " after ";
    message += String(ventMs);
    message += " ms";
    if (ventMs >= cfg::VENT_TIMEOUT_MS) {
      message += ", vent timed out";
      logger_->warn(message.c_str());
      lastState_ = state;
      return;
    }
  }
  if (state == RunState::Venting) ventStartMs_ = nowMs;
  logger_->info(message.c_str());
  lastState_ = state;
}

// The keys are a local surface with no action assigned yet, so reporting a press is the whole of
// what they do. One line per press, from here like every other report.
void App::logButtonPresses() {
  for (Button button : kButtons) {
    if (!context_.buttons().consumePress(button)) continue;
    String message = "btn: ";
    message += buttonName(button);
    logger_->info(message.c_str());
  }
}

void App::update() {
  uint32_t now = millis();

  context_.sensor().update(now);
  context_.pump().update(now);
  context_.battery().update(now);
  context_.vacuum().service(now);

  // Commands first, against the machine as the last control tick left it, so a reply
  // describes something that actually happened rather than a view built mid-command.
  link_.poll(now);

  // A controller that goes away releases the chamber (Q4).
  if (link_.consumeLinkLost()) {
    logger_->warn("app: controller left, stopping and venting");
    context_.vacuum().stop();
  }

  logRunChanges(now);
  logButtonPresses();
  // Nothing is drawn here. The panel belongs to the host from now on, and the device only puts
  // its own two things on it: the splash, once, and the battery, on the way to every flush.
}

}  // namespace gardener
