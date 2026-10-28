#include "Build.h"

#if GARDENER_PANEL

#include "panel/PanelRoot.h"

#include <Wire.h>

#include "MachineRoot.h"
#include "panel/BuzzerPlayer.h"
#include "panel/Panel.h"
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

// The full device's composition: the machine's parts, then the panel's. The parts are new'd once and
// never deleted — the device's destructor never runs — so the composition owns them by the graph it
// hands out and nothing frees them.
Panel& buildPanelRoot(Logger& logger, Library& library, const PanelPins& pins,
                      CommandQueue& queue) {
  const MachineParts machine = buildMachineParts(pins.machine);
  auto* battery = new AdcBattery(pins.batteryAdc, pins.batteryDivider);
  auto* buttons = new Buttons(pins.keyLatch, pins.keyClock, pins.keyData);
  // The piezo is a bare square-wave part; the player above it owns durations and note sequences.
  auto* buzzerPart = new PwmBuzzer(pins.buzzerPwm);
  auto* buzzer = new BuzzerPlayer(*buzzerPart);
  // The panel is an SH1106 on its own bus. Its driver takes its geometry and wiring at
  // construction and comes up in Panel::boot, once the bus is running.
  auto* display = new Sh1106Device(Wire1, PanelAddress, PanelWidth, PanelHeight);
  auto* routines = new Routines();
  auto* panel = new Panel(logger, machine.pump, machine.sensor, machine.valve, buttons, buzzer,
                          battery, display, library, routines, queue);

  // The full device has the panel and the gauge, so it is the one build with bits set.
  panel->setCapabilities(PanelFitted | BatteryFitted);
  return *panel;
}

void bootPanel(Panel& panel, const PanelPins& pins) {
  // The driver takes a TwoWire it does not own, so the bus is up before the panel brings it up.
  Wire1.begin(pins.i2cSda, pins.i2cScl, pins.i2cHz);

  if (panel.boot()) return;

  // abort() panics and the ESP32 resets on a panic, so the device keeps trying rather than coming
  // up blind and silent.
  delay(50);
  abort();
}

}  // namespace gardener
#endif
