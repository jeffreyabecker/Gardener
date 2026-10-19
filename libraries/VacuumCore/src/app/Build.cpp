#include "app/Build.h"

#include <Adafruit_SH110X.h>
#include <Wire.h>

#include "app/AppContext.h"
#include "app/BuzzerPlayer.h"
#include "app/Overlay.h"
#include "app/Settings.h"
#include "app/Storage.h"
#include "app/VacuumSystem.h"
#include "app/disconnected-ui/Routines.h"
#include "core/Config.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/MprlsSensor.h"
#include "hal/impl/PwmBuzzer.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"

namespace gardener {
namespace {

// The fitted panel: an Adafruit 1.3" 128x64 monochrome SH1106 on the STEMMA QT bus. Its geometry
// is the part's own, which is why it is named here with the part rather than in the board's pin
// map — the 128x32 alternative is the same wiring with different numbers.
constexpr uint16_t kPanelWidth = 128;
constexpr uint16_t kPanelHeight = 64;
constexpr uint8_t kPanelAddress = 0x3C;
// The STEMMA QT breakout has no reset line, and the library spells that as a pin of -1.
constexpr int16_t kPanelNoResetLine = -1;

}  // namespace

// The parts are new'd once, at boot, and never deleted: the device's destructor never runs, so
// nothing owns them — the same policy the SH1106 driver already documents. This is the one place
// that names the rig: which pin drives what, which bus each I2C part sits on, and which valve is
// fitted. The drivers are handed those facts rather than reading them, so a board variant edits
// this file and core/Config.h and nothing under hal/impl/. The AppContext itself is copied out by
// value.
//
// The two I2C buses are App's to bring up: each driver takes a TwoWire it does not own, so both are
// up before begin() is called.
AppContext buildAppContext() {
  auto* sensor = new MprlsSensor(Wire);
  auto* pump = new PwmPump(cfg::PUMP_PWM);
  auto* vent = new NormallyOpenValve(cfg::VENT_VALVE);
  auto* battery = new AdcBattery(cfg::BATTERY_ADC);
  auto* buttons = new Buttons(cfg::BTN_UP, cfg::BTN_DOWN, cfg::BTN_OK, cfg::BTN_BACK);
  // The piezo is a bare square-wave part; the player above it owns durations and note sequences.
  auto* buzzerPart = new PwmBuzzer(cfg::BUZZER_PWM);
  auto* buzzer = new BuzzerPlayer(*buzzerPart);
  // The panel is an SH1106 on its own bus. Its driver takes its geometry and wiring at
  // construction; the callback is where it comes up, once App has the bus running.
  auto* display = new AdafruitDisplay<Adafruit_SH1106G>(
      [](Adafruit_SH1106G& panel) -> bool { return panel.begin(kPanelAddress, false); },
      kPanelWidth, kPanelHeight, &Wire1, kPanelNoResetLine);
  auto* system = new VacuumSystem(pump, sensor, vent);
  auto* overlay = new Overlay(display, battery, system);
  auto* storage = new Storage();
  auto* settings = new Settings(*storage);
  auto* routines = new Routines();
  return AppContext(sensor, pump, vent, battery, buttons, buzzer, overlay, system, storage,
                    settings, routines);
}

}  // namespace gardener
