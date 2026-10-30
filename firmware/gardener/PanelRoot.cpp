#include "PanelRoot.h"

#include <LittleFS.h>
#include <Wire.h>

#include "core/Units.h"

namespace gardener {
namespace {

// The fitted panel: a 1.3" 128x64 monochrome SH1106 on the STEMMA QT bus. Its geometry is the part's
// own, which is why it is named here with the part rather than in the board's pin map — the 128x32
// alternative is the same wiring with different numbers.
constexpr uint8_t PanelAddress = 0x3C;
constexpr int16_t PanelWidth = 128;
constexpr int16_t PanelHeight = 64;

}  // namespace

PanelRoot::PanelRoot(const PanelPins& pins)
    : pins_(pins),
      logger_(Serial),
      files_(LittleFS),
      parts_(buildMachineParts(pins.machine)),
      machine_(logger_, parts_.pump, parts_.sensor, parts_.valve),
      outbox_(commands_),
      battery_(pins.batteryAdc, pins.batteryDivider),
      keys_(pins.keyLatch, pins.keyClock, pins.keyData),
      buzzerPart_(pins.buzzerPwm),
      buzzer_(buzzerPart_),
      display_(Wire1, PanelAddress, PanelWidth, PanelHeight),
      settings_(&files_, state_),
      registry_(files_, logger_),
      bindings_(display_, state_, buzzer_, outbox_, presses_, ux_, registry_, logger_),
      interpreter_(bindings_),
      routines_(interpreter_, display_, state_, presses_, ux_, registry_, logger_, files_) {}

// The device's bring-up, in the order the parts depend on each other. The store comes first because
// the image and every routine document are read through it; the bus before the glass because the
// driver takes a bus it does not own; the document before the interpreter because its modules read
// it when they are installed.
bool PanelRoot::begin() {
  bool mounted = LittleFS.begin(false);
  bool formatted = false;
  if (!mounted) {
    formatted = LittleFS.format() && LittleFS.begin(false);
    mounted = formatted;
  }
  if (mounted) {
    logger_.info(formatted ? "fs: littlefs formatted, no filesystem found"
                           : "fs: littlefs mounted");
    String usage = "fs: ";
    usage += String((uint32_t)LittleFS.usedBytes());
    usage += " of ";
    usage += String((uint32_t)LittleFS.totalBytes());
    usage += " bytes used";
    logger_.info(usage.c_str());
  } else {
    logger_.warn("fs: littlefs mount failed, settings will not persist");
  }

  Wire1.begin(pins_.i2cSda, pins_.i2cScl, pins_.i2cHz);

  machine_.boot();
  settings_.begin();

  const bool displayOk = display_.begin();
  logger_.info(displayOk ? "oled: online" : "oled: not found");

  const bool batteryOk = battery_.begin();
  const bool buzzerOk = buzzer_.begin();

  // The VM came up with the interpreter; the image is loaded into it here, and a device with no
  // routines simply never enters it.
  const bool vmOk = routines_.begin();

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger_.info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger_.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger_.info("buttons: four keys on interrupt");

  String stored = "settings: unit=";
  stored += displayUnitInfo(settings_.unit()).name;
  logger_.info(stored.c_str());

  logger_.info(vmOk ? "vm: ready" : "vm: no memory, routines off");

  return displayOk;
}

}  // namespace gardener
