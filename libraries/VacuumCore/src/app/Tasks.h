#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "hal/Button.h"

namespace gardener {

class Logger;
class MachineContext;
class PanelContext;
class ConnectedUI;
class DisconnectedUI;
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

// The two queues. Handles are written once at boot and read by the task bodies after; the
// store queue is created by the store bus in buildAppContext().
extern QueueHandle_t machineCommandQueue;
extern QueueHandle_t storeQueue;

// Shared scalars, one writer each. stopSeq is the panel task's counter, bumped when the Back hold
// stops a run and when control is handed between the panel and an external link; the routine's
// takeStopped() compares against it and reads it dirty.
extern volatile uint32_t stopSeq;

// goHomeSeq is the panel task's counter for the OK pressed while the Back hold vents: the
// disconnected page watches it and hands the panel back to `default` on a bump. One writer, read
// dirty.
extern volatile uint32_t goHomeSeq;

// Connection presence, one writer (the interface owner) and read dirty by the panel's overlay.
extern volatile bool serialConnected;
extern volatile bool bleConnected;

void controlTask(void* arg);
void interfaceTask(void* arg);
void panelTask(void* arg);

// The machine's boot spawns the control and interface tasks, pinned to their cores; the command
// queue is created here, before either task reads it.
void spawnMachineTasks(MachineContext& context, Logger& logger, SerialLink* link, BleLink* ble);

// The panel's boot spawns the panel task, with the two screens it owns.
void spawnPanelTask(PanelContext& context, Logger& logger, ConnectedUI* connected,
                    DisconnectedUI* disconnected);

}  // namespace gardener
