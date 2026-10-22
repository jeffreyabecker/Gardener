#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "Build.h"
#include "MachineCommand.h"

namespace gardener {

class Machine;
class SerialLink;
#if GARDENER_BLUETOOTH
class BleLink;
#endif

// The machine's command queue. The handle is written once at boot and read by the control task
// after.
extern QueueHandle_t machineCommandQueue;

// Connection presence, one writer (the interface owner) and read dirty by the panel's overlay. A
// build without the radio never sets the Bluetooth one, so the panel draws no rune.
extern volatile bool serialConnected;
extern volatile bool bleConnected;

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
