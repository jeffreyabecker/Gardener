#include "Machine.h"

#include "Logger.h"
#include "core/Config.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"

namespace gardener {

// The parts are new'd once, at construction, and never deleted: the device's destructor never runs,
// so nothing owns them. The sensor, pump and vent are named here with their pins, so a board variant
// edits this file and core/Config.h and nothing under hal/impl/.
Machine::Machine(Logger& logger)
    : logger_(logger),
      vacuum_(new PwmPump(cfg::PUMP_PWM), new XgzpSensor(cfg::SENSOR_ADC),
              new NormallyOpenValve(cfg::VENT_VALVE)) {}

void Machine::boot() {
  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
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
