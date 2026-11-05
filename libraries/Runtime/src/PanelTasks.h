#pragma once

#include <Arduino.h>

namespace gardener {
class Battery;
class ButtonPanel;
class Logger;
struct SystemState;
class CommandQueue;
class Machine;
class GestureQueue;
class BuzzerPlayer;
class Routines;
}  // namespace gardener

namespace gardener::runtime {

void panelTask(void* arg);

// The panel's boot spawns the panel task, which drives the one page loop the panel shows. `state` is
// the object the parts beat publishes the pack into and the page beat reads, and the rest are the
// parts the three beats reach: the keys and the queue a gesture goes to, the queue a command is
// posted to, the gauge and the piezo, the stack the page beat polls, and the machine the reset
// reboots.
void spawnPanelTask(ButtonPanel& buttons, GestureQueue& presses, CommandQueue& queue,
                    Logger& logger, Battery& battery, BuzzerPlayer& buzzer, Routines& routines,
                    Machine& machine, SystemState& state);

}  // namespace gardener::runtime