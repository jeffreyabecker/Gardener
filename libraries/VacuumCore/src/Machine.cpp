#include "Machine.h"

#include "Logger.h"
#include "firmwareversion.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"
#include "storage/MachineSettings.h"

namespace gardener {

// The parts are new'd once, at construction, and never deleted: the device's destructor never runs,
// so nothing owns them. The pins come from the sketch, so a board variant edits its sketch and
// nothing under hal/impl/.
Machine::Machine(Logger& logger, const MachinePins& pins, MachineSettings& settings)
    : logger_(logger),
      settings_(settings),
      vacuum_(new PwmPump(pins.pumpPwm), new XgzpSensor(pins.sensorAdc),
              new NormallyOpenValve(pins.ventValve), settings) {}

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

  // The loop the settings describe: the two knobs and the gains they derive, one line so a bench
  // read can confirm the loop the device will run.
  const PidGains gains = pidGainsFor(settings_.pumpFlowLpm(), settings_.chamberVolumeL());
  String pid = "pid: flow ";
  pid += String(settings_.pumpFlowLpm(), 2u);
  pid += " lpm, chamber ";
  pid += String(settings_.chamberVolumeL(), 3u);
  pid += " l, kp=";
  pid += String(gains.kp, 2u);
  pid += " ki=";
  pid += String(gains.ki, 3u);
  pid += " kd=";
  pid += String(gains.kd, 2u);
  logger_.info(pid.c_str());
}

}  // namespace gardener
