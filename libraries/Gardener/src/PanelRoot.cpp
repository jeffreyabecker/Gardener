#include "Build.h"

#if GARDENER_PANEL

#include "PanelRoot.h"

#include <Adafruit_SH110X.h>
#include <Wire.h>

#include "BuzzerPlayer.h"
#include "Panel.h"
#include "routines/Routines.h"
#include "storage/PanelSettings.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/PwmBuzzer.h"

namespace gardener {
namespace {

// The fitted panel: an Adafruit 1.3" 128x64 monochrome SH1106 on the STEMMA QT bus. Its geometry
// is the part's own, which is why it is named here with the part rather than in the board's pin
// map — the 128x32 alternative is the same wiring with different numbers.
constexpr uint16_t PanelWidth = 128;
constexpr uint16_t PanelHeight = 64;
constexpr uint8_t PanelAddress = 0x3C;
// The STEMMA QT breakout has no reset line, and the library spells that as a pin of -1.
constexpr int16_t PanelNoResetLine = -1;

}  // namespace

// The full device's composition: the panel parts on top of the machine. The panel's bus is the
// composition's to bring up: the driver takes a TwoWire it does not own, so it is up before begin()
// is called.
Panel& buildPanelRoot(Logger& logger, PanelSettings& settings, FS& fs, Library& library,
                      const PanelPins& pins) {
  auto* battery = new AdcBattery(pins.batteryAdc, pins.batteryDivider);
  auto* buttons = new Buttons(pins.keyLatch, pins.keyClock, pins.keyData);
  // The piezo is a bare square-wave part; the player above it owns durations and note sequences.
  auto* buzzerPart = new PwmBuzzer(pins.buzzerPwm);
  auto* buzzer = new BuzzerPlayer(*buzzerPart);
  // The panel is an SH1106 on its own bus. Its driver takes its geometry and wiring at
  // construction; the callback is where it comes up, once App has the bus running.
  auto* display = new AdafruitDisplay<Adafruit_SH1106G>(
      [](Adafruit_SH1106G& panel) -> bool { return panel.begin(PanelAddress, false); },
      PanelWidth, PanelHeight, &Wire1, PanelNoResetLine);
  auto* routines = new Routines();
  auto* panel = new Panel(logger, pins, buttons, buzzer, battery, display, settings, fs, library,
                          routines);

  // The full device has the panel and the gauge, so it is the one build with bits set.
  panel->setCapabilities(PanelFitted | BatteryFitted);
  return *panel;
}

}  // namespace gardener
#endif
