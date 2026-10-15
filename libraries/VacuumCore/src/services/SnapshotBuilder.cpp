#include "services/SnapshotBuilder.h"

#include <Arduino.h>

#include "hal/Battery.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "services/VacuumControl.h"

namespace eggplan {

SystemSnapshot buildSnapshot(const Pump& pump, const PressureSensor& sensor,
                            const VacuumControl& control, const Battery& battery) {
  SystemSnapshot snapshot;

  snapshot.kpa = sensor.kpa();
  snapshot.vacuumKpa = sensor.vacuumKpa();
  snapshot.vacuumPercent = sensor.vacuumPercent();
  snapshot.ambientKpa = sensor.ambientKpa();
  snapshot.sensorOk = sensor.present();
  snapshot.readingValid = sensor.readingValid();

  snapshot.duty = pump.dutyPercent();
  snapshot.pumpTripped = pump.tripped();
  snapshot.faultCode = pump.faultCode();

  snapshot.state = control.state();
  snapshot.targetActive = control.targetActive();
  snapshot.targetKpa = control.targetKpa();
  snapshot.floorKpa = control.minReachableKpa();
  snapshot.remainSeconds = control.remainingSeconds();
  snapshot.totalSeconds = control.durationSeconds();

  snapshot.batteryPresent = battery.present();
  snapshot.batteryLow = battery.low();
  snapshot.batteryVolts = battery.volts();
  snapshot.batteryPercent = battery.percent();

  snapshot.uptimeMs = millis();

  return snapshot;
}

}  // namespace eggplan
