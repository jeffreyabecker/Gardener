#pragma once

#include <Arduino.h>

#include "Build.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"

namespace gardener {

class Machine;
class SerialLink;
#if GARDENER_BLUETOOTH
class BleLink;
#endif

void controlTask(void* arg);
void interfaceTask(void* arg);

// The composition spawns the control and interface tasks, pinned to their cores. The queue the
// links post into is built by the composition before either task runs and handed here, so the task
// that drains it and the links that fill it hold the same object.
#if GARDENER_BLUETOOTH
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link, BleLink* ble);
#else
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link);
#endif
}  // namespace gardener
