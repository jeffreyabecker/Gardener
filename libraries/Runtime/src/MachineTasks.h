#pragma once

#include <Arduino.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"

namespace gardener {
class Machine;
class SerialLink;
}  // namespace gardener

namespace gardener::runtime {

void controlTask(void* arg);
void interfaceTask(void* arg);

// The composition spawns the control and interface tasks, pinned to their cores. The queue the
// link posts into is built by the composition before either task runs and handed here, so the task
// that drains it and the link that fills it hold the same object.
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link);
}  // namespace gardener::runtime