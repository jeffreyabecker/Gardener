#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace gardener {

class Logger;
class MachineContext;
class SerialLink;
class BleLink;

// A write to the machine that the control task applies, carried across from the link or the VM
// that originated it. The op names the verb; the fields are the op's own, so a field that does
// not belong to the op is ignored.
struct MachineCommand {
  enum class Op : uint8_t { Start, Stop, ResetFault, SetTarget, ClearTarget, Reboot };
  Op op;
  float value = 0.0f;  // SetTarget: kPa below ambient
};

// The machine's command queue. The handle is written once at boot and read by the control task
// after.
extern QueueHandle_t machineCommandQueue;

// Connection presence, one writer (the interface owner) and read dirty by the panel's overlay.
extern volatile bool serialConnected;
extern volatile bool bleConnected;

void controlTask(void* arg);
void interfaceTask(void* arg);

// The composition spawns the control and interface tasks, pinned to their cores; the command queue
// is created here, before either task reads it.
void spawnMachineTasks(MachineContext& context, Logger& logger, SerialLink* link, BleLink* ble);
}  // namespace gardener
