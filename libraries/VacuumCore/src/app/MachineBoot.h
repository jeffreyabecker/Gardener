#pragma once

#include "app/MachineContext.h"

namespace gardener {

class BleLink;
class Logger;
class SerialLink;

// What the machine's boot hands back: the two links it built and started, on which the panel's
// boot registers its own domains beside the machine's.
struct MachineBoot {
  SerialLink* serial;
  BleLink* ble;
};

// The machine's side of bring-up: builds and starts the serial and BLE links, registers the
// machine's domains, brings the machine's parts up, and spawns the control and interface tasks.
// A build with no panel calls this alone.
MachineBoot machineBoot(MachineContext& context, Logger& logger);

}  // namespace gardener
