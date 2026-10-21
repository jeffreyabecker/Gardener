#include "app/Build.h"

#include <Adafruit_SH110X.h>
#include <Wire.h>

#include "app/AppContext.h"
#include "app/BuzzerPlayer.h"
#include "app/Capabilities.h"
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

// The machine's own composition: the pressure loop's parts, the ownership stack and the
// capability bits every build shares. The parts are new'd once, at boot, and never deleted: the
// device's destructor never runs, so nothing owns them. The sensor, pump and vent are named here
// with their pins, so a board variant edits this file and core/Config.h and nothing under
// hal/impl/.
MachineContext& buildMachineRoot() {
  auto* sensor = new XgzpSensor(cfg::SENSOR_ADC);
  auto* pump = new PwmPump(cfg::PUMP_PWM);
  auto* vent = new NormallyOpenValve(cfg::VENT_VALVE);
  auto* system = new VacuumSystem(pump, sensor, vent);
  auto* ownership = new Ownership();
  auto* capabilities = new Capabilities();
  // Like every build, the machine vents when its owner leaves.
  capabilities->setOwnershipStop(true);
  return *new MachineContext(sensor, pump, vent, system, ownership, capabilities);
}

// The full device's composition: the machine root, then the panel parts on top. This is the one
// place that names the panel's rig — its pins, its bus, its geometry — so a board variant edits
// this file and nothing under hal/impl/. The AppContext itself is copied out by value.
//
// The panel's bus is App's to bring up: the driver takes a TwoWire it does not own, so it is up
// before begin() is called. The sensor is a voltage on an ADC pin and needs no bus.
AppContext buildAppContext() {
  MachineContext& machine = buildMachineRoot();
  // The full device has the panel and the gauge, and the panel pushes itself above the floor so
  // it is the default controller.
  machine.capabilities().setPanel(true);
  machine.capabilities().setBattery(true);
  machine.ownership().acquire(Link::Panel);

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
  auto* overlay = new Overlay(display, battery, &machine.vacuum());
  auto* storage = new Storage();
  auto* settings = new Settings(*storage);
  auto* routines = new Routines();
  // The store bus: the panel's client and the interface's server share one queue. The queue is the
  // one handle named in app/Tasks.h; the other two are created when the tasks spawn.
  storeQueue = xQueueCreate(StoreQueueDepth, sizeof(StoreRequest));
  auto* storeClient = new StoreClient(storeQueue);
  auto* storeServer = new StoreServer(storeQueue, *settings, *storage);
  auto* panel = new PanelContext(buttons, buzzer, battery, overlay, storage, settings, routines,
                                 storeClient, storeServer, machine);
  return AppContext(machine, *panel);
}

}  // namespace gardener
