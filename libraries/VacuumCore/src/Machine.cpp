#include "Machine.h"

#include "Logger.h"
#include "firmwareversion.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"

namespace gardener {

// The parts are new'd once, at construction, and never deleted: the device's destructor never runs,
// so nothing owns them. The pins come from the sketch, so a board variant edits its sketch and
// nothing under hal/impl/.
Machine::Machine(Logger& logger, Settings& settings, const MachinePins& pins)
    : logger_(logger),
      settings_(settings),
      vacuum_(new PwmPump(pins.pumpPwm), new XgzpSensor(pins.sensorAdc),
              new NormallyOpenValve(pins.ventValve)) {}

void Machine::boot() {
  String boot = "gardener ";
  boot += FirmwareVersion;
  boot += " booting";
  logger_.info(boot.c_str());

  const bool sensorOk = vacuum_.sensor().begin();
  const bool pumpOk = vacuum_.pump().begin();
  vacuum_.vent().begin();
  vacuum_.begin();

  logger_.info(sensorOk ? "sensor: online" : "sensor: not found");
  logger_.info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger_.info(vacuum_.vent().energiseWhenOpen() ? "vent: normally closed"
                                                 : "vent: normally open");
}

}  // namespace gardener
