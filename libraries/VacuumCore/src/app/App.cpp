#include "app/App.h"

#include <Adafruit_SH110X.h>
#include <Wire.h>
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

App::App()
    : sensor_(Wire),
      panel_(nullptr),
      display_(panel_),
      overlay_(display_, snapshot_),
      system_(pump_, sensor_, vent_),
      link_(system_, overlay_, Serial, serialAttached, snapshot_),
      logger_(&link_) {}

// The one place the fitted panel is named, and the only place its concrete type appears. The
// driver is new'd and never deleted: it lives for the life of the device, whose destructor never
// runs, so nothing owns it. This runs once the I2C bus is up, which is why the panel is attached
// from begin() and not from the constructor.
Adafruit_GrayOLED* App::attachPanel() {
  // Named `driver` rather than `panel` so the panel:: constants below stay reachable.
  Adafruit_SH1106G* driver = new Adafruit_SH1106G(panel::WIDTH, panel::HEIGHT, &Wire, -1);

  // The STEMMA QT breakout has no reset line, so the library must not poke one.
  if (!driver->begin(panel::I2C_ADDR, false)) {
    // No panel, no readout: abort() panics and the ESP32 resets on a panic, so the device keeps
    // trying rather than coming up blind and silent.
    logger_->warn("oled: not found, aborting");
    delay(50);
    abort();
  }
  return driver;
}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger_->info(boot.c_str());

  Wire.begin(cfg::I2C_SDA, cfg::I2C_SCL, cfg::I2C_FREQ_HZ);

  // The panel is the one part with no fallback: nothing on the device can draw without it, so a
  // panel that does not answer is fatal rather than a degraded mode.
  panel_ = attachPanel();
  bool sensorOk = sensor_.begin();
  bool pumpOk = pump_.begin();
  vent_.begin();
  bool batteryOk = battery_.begin();
  system_.begin();

  if (sensorOk) sensor_.captureAmbient();

  // What came up, and what did not. This is the composition root's job now: the drivers report as
  // values and one place decides what is worth saying.
  logger_->info("oled: online");
  logger_->info(sensorOk ? "mprls: online" : "mprls: not found");
  logger_->info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger_->info(vent_.energiseWhenOpen() ? "vent: normally closed" : "vent: normally open");
  logger_->info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger_->info("buttons: four keys on interrupt");

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger_->info(ready.c_str());

  refreshSnapshot();
  link_.begin();
  lastState_ = system_.state().runState;
  splash();
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
  snapshot_.vacuum = system_.state();

  snapshot_.batteryPresent = battery_.present();
  snapshot_.batteryLow = battery_.low();
  snapshot_.batteryVolts = battery_.volts();
  snapshot_.batteryPercent = battery_.percent();

  snapshot_.uptimeMs = millis();
}

// The drivers stopped narrating, so the run is reported from here: one line per state
// change, timed when it leaves a vent, plus the fault code when one latches.
void App::logRunChanges(uint32_t nowMs) {
  RunState state = system_.state().runState;
  if (state == lastState_) return;

  String message = runStateName(lastState_);
  message += " -> ";
  message += runStateName(state);
  if (state == RunState::Fault) {
    message += " (fault ";
    message += system_.state().faultCode;
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
    if (!buttons_.consumePress(button)) continue;
    String message = "btn: ";
    message += buttonName(button);
    logger_->info(message.c_str());
  }
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
    logger_->warn("app: controller left, stopping and venting");
    system_.stop();
  }

  logRunChanges(now);
  logButtonPresses();
  refreshSnapshot();
  // Nothing is drawn here. The panel belongs to the host from now on, and the device only puts
  // its own two things on it: the splash, once, and the battery, on the way to every flush.
}

}  // namespace gardener
