#include "panel/SystemState.h"

#include "core/Battery.h"
#include "storage/DeviceSettings.h"

namespace gardener {

SystemState readSystemState(const VacuumSystem& vacuum, const Battery& battery,
                            const DeviceSettings& settings, bool serialConnected,
                            bool bluetoothConnected) {
  SystemState state;
  state.runState = vacuum.runState();
  state.running = vacuum.running();
  state.currentPressureKpa = vacuum.currentPressureKpa();
  state.targetPressureKpa = vacuum.targetPressureKpa();
  state.canStart = vacuum.startRefusal() == nullptr;

  // A pack with no gauge behind it has no percentage to report, so it reads zero rather than
  // whatever the part last held.
  state.batteryPresent = battery.present();
  state.batteryPercent = state.batteryPresent ? battery.percent() : 0;
  state.batteryLow = battery.low();

  state.serialConnected = serialConnected;
  state.bluetoothConnected = bluetoothConnected;

  state.unit = settings.unit();
  state.deviceTargetKpa = settings.targetKpa();
  return state;
}

}  // namespace gardener
