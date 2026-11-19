#include "machine/Machine.h"

#include <Arduino.h>

#include "core/Logger.h"
#include "core/firmwareversion.h"

namespace gardener {

// The parts are the composition's: it builds them from the board's pins and hands them here, so
// this class is the control law and the boot report rather than the wiring.
Machine::Machine(Logger& logger, Pump* pump, PressureSensor* sensor, VentValve* valve)
    : logger_(logger), vacuum_(pump, sensor, valve) {}

void Machine::boot() {
  String boot = "gardener ";
  boot += FirmwareVersion;
  boot += " booting";
  logger_.info(boot.c_str());

  const bool sensorOk = vacuum_.sensor().begin();
  vacuum_.pump().begin();
  vacuum_.vent().begin();
  vacuum_.begin();

  logger_.info(sensorOk ? "sensor: online" : "sensor: not found");
  logger_.info(vacuum_.vent().energiseWhenOpen() ? "vent: normally closed" : "vent: normally open");

  // The band the build holds, one line so a bench read can confirm the loop the device will run.
  String band = "band: -";
  band += String(ControlBelowPercent, 1u);
  band += "% +";
  band += String(ControlAbovePercent, 1u);
  band += "% of target";
  logger_.info(band.c_str());
}

}  // namespace gardener