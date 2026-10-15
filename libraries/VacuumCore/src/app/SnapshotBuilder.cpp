#include "app/SnapshotBuilder.h"

#include <Arduino.h>

#include "app/VacuumControl.h"
#include "app/VacuumSystem.h"
#include "hal/Battery.h"

namespace eggplan {

void buildSnapshot(const VacuumSystem& system, const VacuumControl& control,
                   const Battery& battery, SystemSnapshot& snapshot) {
  snapshot.kpa = system.kpa();
  snapshot.vacuumKpa = system.vacuumKpa();
  snapshot.vacuumPercent = system.vacuumPercent();
  snapshot.ambientKpa = system.ambientKpa();
  snapshot.sensorOk = system.sensorOk();
  snapshot.readingValid = system.readingValid();

  // faulted() is the trip as the plant sees it: it latches when the trip is observed and
  // clears with resetFault, so it says the same thing pump.tripped() said.
  snapshot.duty = system.duty();
  snapshot.pumpTripped = system.faulted();
  snapshot.faultCode = system.faultCode();

  snapshot.state = control.state();
  snapshot.targetActive = system.targetActive();
  snapshot.targetKpa = system.targetKpa();
  snapshot.floorKpa = system.minReachableKpa();
  snapshot.holdSeconds = control.holdSeconds();

  snapshot.batteryPresent = battery.present();
  snapshot.batteryLow = battery.low();
  snapshot.batteryVolts = battery.volts();
  snapshot.batteryPercent = battery.percent();

  snapshot.uptimeMs = millis();
}

}  // namespace eggplan
