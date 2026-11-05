#pragma once

#include <Arduino.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"

namespace gardener {
struct SystemState;
class Machine;
class SerialLink;
}  // namespace gardener

namespace gardener::runtime {

void controlTask(void* arg);
void interfaceTask(void* arg);

// The composition spawns the control and interface tasks, pinned to their cores. The queue the
// link posts into is built by the composition before either task runs and handed here, so the task
// that drains it and the link that fills it hold the same object. `state` is the object the two
// tasks publish their slices of — the machine's, and the way a controller reaches the device.
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link, SystemState& state);
}  // namespace gardener::runtime