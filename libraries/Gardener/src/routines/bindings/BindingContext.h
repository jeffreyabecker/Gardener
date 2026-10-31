#pragma once

#include <Arduino.h>

namespace gardener {
inline namespace core {
class Logger;
struct SystemState;
}
inline namespace machine {
class CommandOutbox;
struct MachineCommand;
}
}  // namespace gardener

namespace gardener {

namespace gfx {
class DisplayDevice;
}

namespace panel {
class GestureQueue;
namespace parts {
class BuzzerPlayer;
}
namespace page {
class RoutineRegistry;
class Ux;
}
}  // namespace panel

namespace berry {

// What the native modules reach of the app, held as the parts themselves: one VM and one app, so a
// callback -- a plain C function with no user data -- gets at the glass, the published state, the
// piezo, the machine's queue, the presses, the stack, the registry and the log through this, and no
// module names a global of its own. The accessors are the ones the modules need and nothing more,
// so what a page can reach is this list rather than whatever the objects beside it happen to hold.
class BindingContext {
 public:
  BindingContext(gfx::DisplayDevice& display, SystemState& state,
                 panel::parts::BuzzerPlayer& buzzer, CommandOutbox& outbox,
                 panel::GestureQueue& presses, panel::page::Ux& ux,
                 panel::page::RoutineRegistry& registry, Logger& logger)
      : display_(display),
        state_(state),
        buzzer_(buzzer),
        outbox_(outbox),
        presses_(presses),
        ux_(ux),
        registry_(registry),
        logger_(logger) {}

  // --- the panel's parts ---------------------------------------------------
  // The glass, the published state, the piezo, and the queue a command is posted to.
  gfx::DisplayDevice& display();
  SystemState& systemState();
  panel::parts::BuzzerPlayer& buzzer();
  bool post(const MachineCommand& command);

  // --- the runtime behind the image ----------------------------------------
  // The presses waiting for a page, the stack a launch pushes on, the routines the image
  // registered, and the queue the `device` module drains.
  panel::GestureQueue& presses();
  panel::page::Ux& ux();
  panel::page::RoutineRegistry& registry();
  bool takePress(String& gesture);

  // --- the device's log ----------------------------------------------------
  void info(const char* message);
  void error(const char* message);

 private:
  gfx::DisplayDevice& display_;
  SystemState& state_;
  panel::parts::BuzzerPlayer& buzzer_;
  CommandOutbox& outbox_;
  panel::GestureQueue& presses_;
  panel::page::Ux& ux_;
  panel::page::RoutineRegistry& registry_;
  Logger& logger_;
};

}  // namespace berry
}  // namespace gardener