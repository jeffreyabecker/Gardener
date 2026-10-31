#include "machine/Machine.h"

#include "core/Logger.h"
#include "core/firmwareversion.h"

namespace gardener {
inline namespace machine {

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
  const bool pumpOk = vacuum_.pump().begin();
  vacuum_.vent().begin();
  vacuum_.begin();

  logger_.info(sensorOk ? "sensor: online" : "sensor: not found");
  logger_.info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger_.info(vacuum_.vent().energiseWhenOpen() ? "vent: normally closed"
                                                 : "vent: normally open");

  // The fitted valve is the fail-safe: a normally open part vents when the coil is not held, so
  // the bit says the resting state is safe rather than that a valve is fitted at all.
  if (!vacuum_.vent().energiseWhenOpen()) capabilities_ |= FailSafeValve;

  // The loop the build describes: the two rig numbers and the gains they derive, one line so a
  // bench read can confirm the loop the device will run.
  const PidGains gains = pidGainsFor(PUMP_FLOW_LPM, CHAMBER_VOLUME_L);
  String pid = "pid: flow ";
  pid += String(PUMP_FLOW_LPM, 2u);
  pid += " lpm, chamber ";
  pid += String(CHAMBER_VOLUME_L, 3u);
  pid += " l, kp=";
  pid += String(gains.kp, 2u);
  pid += " ki=";
  pid += String(gains.ki, 3u);
  pid += " kd=";
  pid += String(gains.kd, 2u);
  logger_.info(pid.c_str());
}

}  // namespace machine
}  // namespace gardener
