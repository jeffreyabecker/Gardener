#include "app/Build.h"

#include <Adafruit_SH110X.h>
#include <Wire.h>
#include <stdlib.h>

#include "app/AppContext.h"
#include "app/BuzzerPlayer.h"
#include "app/Capabilities.h"
#include "app/Logger.h"
#include "app/MachineContext.h"
#include "app/Ownership.h"
#include "app/PanelContext.h"
#include "app/StoreBus.h"
#include "app/Tasks.h"
#include "app/VacuumSystem.h"
#include "app/panel/Overlay.h"
#include "app/routines/Routines.h"
#include "app/storage/Settings.h"
#include "app/storage/Storage.h"
#include "core/Config.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/PwmBuzzer.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"

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
// that names the rig: which pin drives what, which bus the panel sits on, and which valve is
// fitted. The drivers are handed those facts rather than reading them, so a board variant edits
// this file and core/Config.h and nothing under hal/impl/. The AppContext itself is copied out by
// value.
//
// The panel's bus is App's to bring up: the driver takes a TwoWire it does not own, so it is up
// before begin() is called. The sensor is a voltage on an ADC pin and needs no bus.
AppContext buildAppContext() {
  auto* sensor = new XgzpSensor(cfg::SENSOR_ADC);
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
  auto* ownership = new Ownership();
  // The panel is the default controller: it sits above the floor from boot, and serial and
  // Bluetooth push themselves above it when they claim.
  ownership->acquire(Link::Panel);
  auto* capabilities = new Capabilities();
  // The full device has the panel and the gauge, and — like every build — it vents when its owner
  // leaves.
  capabilities->setPanel(true);
  capabilities->setBattery(true);
  capabilities->setOwnershipStop(true);
  // The store bus: the panel's client and the interface's server share one queue. The queue is the
  // one handle named in app/Tasks.h; the other two are created when the tasks spawn.
  storeQueue = xQueueCreate(StoreQueueDepth, sizeof(StoreRequest));
  auto* storeClient = new StoreClient(storeQueue);
  auto* storeServer = new StoreServer(storeQueue, *settings, *storage);
  auto* machine = new MachineContext(sensor, pump, vent, system, ownership, capabilities);
  auto* panel = new PanelContext(buttons, buzzer, battery, overlay, storage, settings, routines,
                                 storeClient, storeServer, *machine);
  return AppContext(*machine, *panel);
}

// The panel is the one part with no fallback: nothing on the device can draw
// without it, so a panel that does not answer is fatal rather than a degraded
// mode. The panel build calls this from its boot; a machine-alone build never
// does and comes up with no display.
bool panelBoot(PanelContext& context, Logger& logger) {
  Wire1.begin(cfg::PANEL_I2C_SDA, cfg::PANEL_I2C_SCL, cfg::PANEL_I2C_FREQ_HZ);
  const bool displayOk = context.display().begin();
  logger.info(displayOk ? "oled: online" : "oled: not found, aborting");
  if (!displayOk) {
    // abort() panics and the ESP32 resets on a panic, so the device keeps
    // trying rather than coming up blind and silent.
    delay(50);
    abort();
  }
  return true;
}

}  // namespace gardener
