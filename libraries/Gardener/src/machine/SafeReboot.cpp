#include "machine/SafeReboot.h"

#include <Arduino.h>

#include "machine/Machine.h"
#include "machine/VacuumSystem.h"

namespace gardener {

void safeReboot(Machine& context) {
  VacuumSystem& vacuum = context.vacuum();
  vacuum.stop();

  // The release is what has to happen before the reset, not the whole vent: a chamber that is still
  // coming up to ambient when the device restarts is already on the safe path.
  const uint32_t startedMs = millis();
  while (vacuum.runState() == RunState::Venting &&
         (uint32_t)(millis() - startedMs) < REBOOT_VENT_WAIT_MS) {
    // The sensor has to keep sampling for the release to see that it has finished; the rest of the
    // loop is stopped while the restart is pending.
    const uint32_t now = millis();
    context.vacuum().sensor().update(now);
    vacuum.service(now);
    delay(5);
  }

  ESP.restart();
}

}  // namespace gardener
