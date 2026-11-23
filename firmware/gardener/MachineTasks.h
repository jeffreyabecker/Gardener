#pragma once

#include <Arduino.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"

namespace gardener {
class Machine;
}  // namespace gardener

namespace gardener::runtime {

class BleLink;

// The composition spawns the control and interface tasks, pinned to their cores. The queue the
// links post into is built by the composition before either task runs and handed here, so the task
// that drains it and the links that fill it hold the same object. The Bluetooth link is stepped on
// the interface pass and is null in a build without one.
void spawnMachineTasks(Machine& machine, CommandQueue& queue, BleLink* bluetooth);
}  // namespace gardener::runtime