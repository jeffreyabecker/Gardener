#pragma once

#include <Arduino.h>

namespace gardener {

namespace panel {
class GestureQueue;
namespace parts {
class BuzzerPlayer;
}
}  // namespace panel

inline namespace core {
class Battery;
class ButtonPanel;
}
class CommandOutbox;
inline namespace core {
class Logger;
}
class Machine;
class Routines;
inline namespace core {
struct SystemState;
}

void panelTask(void* arg);

// The panel's boot spawns the panel task, which drives the one page loop the panel shows. `state` is
// the object the parts beat publishes the pack into and the page beat reads, and the rest are the
// parts the three beats reach: the keys and the queue a gesture goes to, the outbox the beat
// flushes, the gauge and the piezo, the stack the page beat polls, and the machine the reset
// reboots.
void spawnPanelTask(ButtonPanel& buttons, panel::GestureQueue& presses, CommandOutbox& outbox,
                    Logger& logger, Battery& battery, panel::parts::BuzzerPlayer& buzzer,
                    Routines& routines, Machine& machine, SystemState& state);

}  // namespace gardener
