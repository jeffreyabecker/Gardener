#pragma once

#include <Arduino.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"

namespace gardener {
inline namespace core {
struct SystemState;
}
inline namespace machine {
class Machine;
}
}  // namespace gardener

namespace gardener {

inline namespace serial {
class SerialLink;
}
class BleLink;

void controlTask(void* arg);
void interfaceTask(void* arg);

// The composition spawns the control and interface tasks, pinned to their cores. The queue the
// links post into is built by the composition before either task runs and handed here, so the task
// that drains it and the links that fill it hold the same object. `state` is the object the two tasks
// publish their slices of — the machine's, and the two ways a controller can reach the device.
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link, BleLink* ble,
                       SystemState& state);
}  // namespace gardener
