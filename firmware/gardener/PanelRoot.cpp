#include "PanelRoot.h"

#include <LittleFS.h>
#include <Wire.h>

#include "Fonts.h"
#include "core/Gestures.h"
#include "core/Knob.h"
#include "core/Units.h"

namespace gardener {
namespace {

// The fitted panel: a 1.3" 128x64 monochrome SH1106 on the STEMMA QT bus. Its geometry is the part's
// own, which is why it is named here with the part rather than in the board's pin map — the 128x32
// alternative is the same wiring with different numbers.
GARDENER_KNOB_VALID(uint8_t, PanelAddress, GARDENER_PANEL_ADDRESS, 0x3C,
                    GARDENER_PANEL_ADDRESS >= 0x08 && GARDENER_PANEL_ADDRESS <= 0x77,
                    "the panel's address on the STEMMA QT bus");
GARDENER_KNOB_VALID(int16_t, PanelWidth, GARDENER_PANEL_WIDTH, 128,
                    GARDENER_PANEL_WIDTH > 0 && GARDENER_PANEL_WIDTH <= 320,
                    "the glass's width, in pixels");
GARDENER_KNOB_VALID(int16_t, PanelHeight, GARDENER_PANEL_HEIGHT, 64,
                    GARDENER_PANEL_HEIGHT > 0 && GARDENER_PANEL_HEIGHT <= 240,
                    "the glass's height, in pixels");

}  // namespace

PanelRoot::PanelRoot(const PanelPins& pins)
    : pins_(pins), logger_(Serial), files_(LittleFS),
      parts_(runtime::buildMachineParts(pins.machine)),
      machine_(logger_, parts_.pump, parts_.sensor, parts_.valve),
      battery_(pins.batteryAdc, pins.batteryDivider),
      keys_(pins.keyLatch, pins.keyClock, pins.keyData, pins.keyMask), buzzerPart_(pins.buzzerPwm),
      buzzer_(buzzerPart_), display_(Wire1, PanelAddress, PanelWidth, PanelHeight),
      settings_(&files_, state_), jingles_(&files_, logger_),
      systemSettings_(presses_, ux_, settings_, jingles_, buzzer_), registry_(files_, logger_),
      bindings_(display_, state_, settings_, systemSettings_, buzzer_, commands_, presses_, ux_,
                registry_, logger_),
      interpreter_(bindings_),
      routines_(interpreter_, display_, state_, presses_, ux_, registry_, logger_, files_) {
  // The words a gesture is made of are interned from more than one task once a link can press, so the
  // device installs the table that holds a lock across an insert. It seeds itself with the words
  // every panel answers, so a press of one of them allocates nothing.
  Gestures::InternTable::setInstance(new runtime::RtosInternTable());
}

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
  jingles_.begin();

  // The panel's own look: everything that names no family draws in ttyp0, and a row shorter than any
  // ttyp0 cut names the Adafruit face for itself.
  gfx::setDefaultFamily(gfx::findFamily("ttyp0"));

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
