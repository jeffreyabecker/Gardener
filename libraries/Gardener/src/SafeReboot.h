#pragma once

namespace gardener {

class Machine;

// The one release-then-restart path: stop the pump, open the valve, give the VENT this long to
// finish, then restart. `device reboot` and the local Back+OK hold both run it, so neither can skip
// the release on the way down.
void safeReboot(Machine& context);

}  // namespace gardener
