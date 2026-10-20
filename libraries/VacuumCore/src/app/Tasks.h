#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app/AppContext.h"
#include "hal/Button.h"

namespace gardener {

class Logger;
class ConnectedUI;
class DisconnectedUI;
class SerialLink;
class BleLink;
class StoreServer;

// A write to the machine that the control task applies, carried across from the link or the VM
// that originated it. The op names the verb; the fields are the op's own, so a field that does
// not belong to the op is ignored.
struct MachineCommand {
  enum class Op : uint8_t { Start, Stop, ResetFault, SetTarget, ClearTarget, Reboot, Tone, Play, Off };
  Op op;
  float value = 0.0f;      // SetTarget: kPa below ambient; Tone: Hz
  uint32_t durationMs = 0; // Tone
  String text;             // Play: the raw note sequence
};

// One classified key press, from the control task to the panel task.
struct ButtonEvent {
  Button button;
  Press press;
};

// The three queues. Handles are written once at boot and read by the task bodies after; the
// store queue is created by the store bus in buildAppContext().
extern QueueHandle_t machineCommandQueue;
extern QueueHandle_t storeQueue;
extern QueueHandle_t buttonEventQueue;

// Shared scalars, one writer each. okHeldMs is the control task's publish of heldMs(Button::Ok)
// for the panel's default-hold watch; stopSeq is the control task's counter, incremented each
// time the Back hold fires, which the panel's takeStopped() compares against. Both are read dirty.
extern volatile uint32_t okHeldMs;
extern volatile uint32_t stopSeq;

// Connection presence, one writer (the interface owner) and read dirty by the panel's overlay.
extern volatile bool serialConnected;
extern volatile bool bleConnected;

void controlTask(void* arg);
void interfaceTask(void* arg);
void panelTask(void* arg);

// Creates the queues and spawns the three tasks, pinned to their cores. Called from App::begin()
// once boot is done, with the objects each task owns: the interface task takes the links and the
// store server, the panel task takes the two screens.
void spawnTasks(AppContext& context, Logger& logger, SerialLink* link, BleLink* ble,
                StoreServer* storeServer, ConnectedUI* connected, DisconnectedUI* disconnected);

}  // namespace gardener
