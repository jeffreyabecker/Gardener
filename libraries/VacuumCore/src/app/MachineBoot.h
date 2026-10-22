#pragma once

#include "app/MachineContext.h"

namespace gardener {

class BleLink;
class Logger;
class SerialLink;

// What the machine's boot hands back: the BLE link it built, on which the panel's boot registers
// its own domains beside the machine's. The serial link is the caller's, built and populated
// before the boot is asked to bring the parts up.
struct MachineBoot {
  SerialLink* serial;
  BleLink* ble;
};

// The machine's side of bring-up: brings the machine's parts up and reports them, builds and
// starts the BLE link, and spawns the control and interface tasks. The serial link arrives built,
// with the machine's `control` and `vacuum` domains already registered on it. A build with no panel
// calls this alone.
MachineBoot machineBoot(MachineContext& context, Logger& logger, SerialLink* serial);

}  // namespace gardener
