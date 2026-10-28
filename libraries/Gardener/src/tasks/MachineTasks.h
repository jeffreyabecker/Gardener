#pragma once

#include <Arduino.h>

#include "Build.h"
#include "machine/MachineCommand.h"

namespace gardener {

class Machine;
class SerialLink;
#if GARDENER_BLUETOOTH
class BleLink;
#endif

void controlTask(void* arg);
void interfaceTask(void* arg);

// The composition spawns the control and interface tasks, pinned to their cores; the command queue
// is created here, before either task reads it.
#if GARDENER_BLUETOOTH
void spawnMachineTasks(Machine& machine, SerialLink* link, BleLink* ble);
#else
void spawnMachineTasks(Machine& machine, SerialLink* link);
#endif
}  // namespace gardener
