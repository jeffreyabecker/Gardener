#include "app/App.h"

#include <Wire.h>
#include <stdlib.h>

#include "app/Asset.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

// The keys in the order a press is reported, which is the panel's order rather than the bit order.
constexpr Button kButtons[] = {Button::Up, Button::Down, Button::Ok, Button::Back};

// The local stop. It is a safety path rather than a preference, so it is a constant here and not a
// setting: holding Back for this long ends the run and vents, whatever the controller is doing.
constexpr uint32_t kBackStopMs = 3000;

}  // namespace

App::App(AppContext context) : context_(context) {
  logger_ = new StreamLogger(Serial, context_.logEnabled());
  link_ = new SerialLink(&context_, Serial, serialAttached);
  connectedUi_ = new ConnectedUI(context_);
  disconnectedUi_ = new DisconnectedUI(context_, *logger_);
}

UserInterface* App::screenFor(bool attached) {
  if (attached) return connectedUi_;
  return disconnectedUi_;
}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger_->info(boot.c_str());

  // The panel is the one part with no fallback: nothing on the device can draw without it, so a
  // panel that does not answer is fatal rather than a degraded mode. Both buses are the app's to
  // bring up, because each driver takes a TwoWire it does not own; they are separate so one part
  // cannot stall the other.
  Wire.begin(cfg::SENSOR_I2C_SDA, cfg::SENSOR_I2C_SCL, cfg::SENSOR_I2C_FREQ_HZ);
  Wire1.begin(cfg::PANEL_I2C_SDA, cfg::PANEL_I2C_SCL, cfg::PANEL_I2C_FREQ_HZ);
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
  bool buzzerOk = context_.buzzer().begin();
  context_.vacuum().begin();

  // The device's files are mounted once, before anything that reads a preference. A partition with
  // no filesystem is formatted, which is reported rather than assumed: whatever was there is gone.
  Storage::Mount mount = context_.storage().begin();
  context_.settings().begin();

  // The mark is an image on the device rather than an array in a header. Every page draws it from
  // there when it needs it, so a data image without it is worth one line and nothing else — the pages
  // simply have no mark.
  if (context_.storage().size(kMarkAsset) == 0) {
    logger_->warn("ui: /assets/eggplant.rle missing, the pages have no mark");
  }

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  bool vmOk = context_.routines().begin(context_, context_.storage(), *logger_);

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger_->info(sensorOk ? "mprls: online" : "mprls: not found");
  logger_->info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger_->info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger_->info(context_.vent().energiseWhenOpen() ? "vent: normally closed"
                                                   : "vent: normally open");
  logger_->info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger_->info("buttons: four keys on interrupt");

  switch (mount) {
    case Storage::Mount::Mounted:
      logger_->info("fs: littlefs mounted");
      break;
    case Storage::Mount::Formatted:
      logger_->info("fs: littlefs formatted, no filesystem found");
      break;
    case Storage::Mount::Failed:
      logger_->warn("fs: littlefs mount failed, settings will not persist");
      break;
  }
  if (mount != Storage::Mount::Failed) {
    String usage = "fs: ";
    usage += String((uint32_t)context_.storage().usedBytes());
    usage += " of ";
    usage += String((uint32_t)context_.storage().totalBytes());
    usage += " bytes used";
    logger_->info(usage.c_str());
  }
  String stored = "settings: unit=";
  stored += displayUnitInfo(context_.settings().unit()).name;
  logger_->info(stored.c_str());

  String vm = vmOk ? "vm: ready, heap " : "vm: no memory, routines off, heap ";
  vm += String((uint32_t)ESP.getFreeHeap());
  logger_->info(vm.c_str());

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger_->info(ready.c_str());

  link_->begin();
  lastState_ = context_.vacuum().runState();
  hostAttached_ = serialAttached();
  ui_ = screenFor(hostAttached_);
  if (ui_) ui_->begin();
  return true;
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

// The keys are a local surface with no action assigned yet, so reporting the gesture is the whole
// of what they do. One line per completed press, from here like every other report.
// The keys belong to whoever owns the page: an active routine takes the presses, and with none the
// loop reports them as it always has. A press is consumed either way, so it is never delivered
// twice.
void App::logButtonPresses() {
  const bool toRoutine = !hostAttached_ && context_.routines().active();
  for (Button button : kButtons) {
    const Press press = context_.buttons().consumePress(button);
    if (press == Press::None) continue;
    if (toRoutine) {
      context_.routines().pushPress(button, press);
      continue;
    }
    String message = "btn: ";
    message += buttonName(button);
    message += ' ';
    message += pressName(press);
    logger_->info(message.c_str());
  }
}

// Back is a local stop, not a press with an action: held past the mark it stops the pump and vents,
// and the latch keeps it to one stop per hold. Releasing it arms the next one.
void App::watchBackHold() {
  const uint32_t held = context_.buttons().heldMs(Button::Back);
  if (held == 0) {
    backStopFired_ = false;
    return;
  }
  if (backStopFired_ || held < kBackStopMs) return;
  backStopFired_ = true;
  logger_->warn("app: back held, stopping and venting");
  context_.vacuum().stop();
}

void App::update() {
  uint32_t now = millis();

  context_.sensor().update(now);
  context_.pump().update(now);
  context_.battery().update(now);
  context_.vacuum().service(now);

  // Commands first, against the machine as the last control tick left it, so a reply
  // describes something that actually happened rather than a view built mid-command.
  link_->poll(now);

  // The buzzer's schedule advances once per pass. A command handled above starts a tone now; a
  // routine's poll at the end of the pass starts one that is serviced on the next pass.
  context_.buzzer().update(now);

  // A controller that goes away releases the chamber (Q4).
  if (link_->consumeLinkLost()) {
    logger_->warn("app: controller left, stopping and venting");
    context_.vacuum().stop();
  }

  // The keys are the device's own way out, independent of the link.
  watchBackHold();

  logRunChanges(now);
  logButtonPresses();
  // The screen follows the link: the readout while a controller is attached, the plain mark when it
  // is not. A swap paints its own first frame.
  const bool attached = serialAttached();
  if (attached != hostAttached_) {
    hostAttached_ = attached;
    ui_ = screenFor(attached);
    if (ui_) ui_->begin();
  }
  // The overlay reports the ways in, and the loop is the only thing that can see the serial link.
  // Bluetooth is not fitted, so it stays false.
  context_.overlay().setConnections(attached, false);
  // Last, because a frame is a whole buffer over I2C: the commands and the run log come first, and
  // the screen is never allowed to hold up the loop in front of them.
  if (ui_) ui_->update();
}

}  // namespace gardener
