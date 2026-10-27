#include "Build.h"

#if GARDENER_PANEL

#include "PanelRoot.h"

#include <Wire.h>

#include "BuzzerPlayer.h"
#include "Panel.h"
#include "routines/Routines.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/PwmBuzzer.h"
#include "hal/impl/Sh1106Device.h"

namespace gardener {
namespace {

// The fitted panel: a 1.3" 128x64 monochrome SH1106 on the STEMMA QT bus. Its geometry is the
// part's own, which is why it is named here with the part rather than in the board's pin map — the
// 128x32 alternative is the same wiring with different numbers.
constexpr uint16_t PanelWidth = 128;
constexpr uint16_t PanelHeight = 64;
constexpr uint8_t PanelAddress = 0x3C;

}  // namespace

// The full device's composition: the panel parts on top of the machine. The panel's bus is the
// composition's to bring up: the driver takes a TwoWire it does not own, so it is up before begin()
// is called.
Panel& buildPanelRoot(Logger& logger, FS& fs, Library& library, const PanelPins& pins) {
  auto* battery = new AdcBattery(pins.batteryAdc, pins.batteryDivider);
  auto* buttons = new Buttons(pins.keyLatch, pins.keyClock, pins.keyData);
  // The piezo is a bare square-wave part; the player above it owns durations and note sequences.
  auto* buzzerPart = new PwmBuzzer(pins.buzzerPwm);
  auto* buzzer = new BuzzerPlayer(*buzzerPart);
  // The panel is an SH1106 on its own bus. Its driver takes its geometry and wiring at
  // construction and comes up in Panel::boot, once the bus is running.
  auto* display = new Sh1106Device(Wire1, PanelAddress, PanelWidth, PanelHeight);
  auto* routines = new Routines();
  auto* panel =
      new Panel(logger, pins, buttons, buzzer, battery, display, fs, library, routines);

  // The full device has the panel and the gauge, so it is the one build with bits set.
  panel->setCapabilities(PanelFitted | BatteryFitted);
  return *panel;
}

}  // namespace gardener
#endif
