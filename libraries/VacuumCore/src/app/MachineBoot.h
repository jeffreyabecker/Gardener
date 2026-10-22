#pragma once

#include "app/MachineContext.h"

namespace gardener {

class BleLink;
class Logger;
class SerialLink;

// The machine's side of bring-up: brings the machine's parts up and reports them, and spawns the
// control and interface tasks on the links the caller built. A build with no panel calls this
// alone.
void machineBoot(MachineContext& context, Logger& logger, SerialLink* serial, BleLink* ble);

}  // namespace gardener
