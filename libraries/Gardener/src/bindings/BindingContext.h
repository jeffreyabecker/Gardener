#pragma once

#include <Arduino.h>

namespace gardener {
class Logger;
struct SystemState;
class CommandQueue;
struct MachineCommand;
class DeviceSettings;
namespace gfx {
class DisplayDevice;
}

class GestureQueue;
class BuzzerPlayer;
class RoutineRegistry;
class SystemSettings;
class Ux;

// What the native modules reach of the app, held as the parts themselves: one VM and one app, so a
// callback -- a plain C function with no user data -- gets at the glass, the published state and
// the document behind it, the piezo, the machine's queue, the presses, the stack, the registry and
// the log through this, and no module names a global of its own. The accessors are the ones the
// modules need and nothing more, so what a page can reach is this list rather than whatever the
// objects beside it happen to hold.
class BindingContext {
 public:
  BindingContext(gfx::DisplayDevice& display, SystemState& state, DeviceSettings& settings,
                 SystemSettings& systemSettings, BuzzerPlayer& buzzer, CommandQueue& queue,
                 GestureQueue& presses, Ux& ux, RoutineRegistry& registry, Logger& logger)
      : display_(display), state_(state), settings_(settings), systemSettings_(systemSettings),
        buzzer_(buzzer), queue_(queue), presses_(presses), ux_(ux), registry_(registry),
        logger_(logger) {}

  // --- the panel's parts ---------------------------------------------------
  // The glass, the published state, the piezo, and the queue a command is posted to.
  gfx::DisplayDevice& display();
  SystemState& systemState();
  BuzzerPlayer& buzzer();
  bool post(const MachineCommand& command);

  // The document the state's two device values come from: what a module reads a preference from,
  // and what it writes one through.
  DeviceSettings& settings();

  // The device's own settings list: the rows the system page opens over, and the set it writes back.
  SystemSettings& systemSettings();

  // --- the runtime behind the image ----------------------------------------
  // The presses waiting for a page, the stack a launch pushes on, the routines the image
  // registered, and the queue the `device` module drains.
  GestureQueue& presses();
  Ux& ux();
  RoutineRegistry& registry();
  const char* takePress();

  // --- the device's log ----------------------------------------------------
  void info(const char* message);
  void error(const char* message);

 private:
  gfx::DisplayDevice& display_;
  SystemState& state_;
  DeviceSettings& settings_;
  SystemSettings& systemSettings_;
  BuzzerPlayer& buzzer_;
  CommandQueue& queue_;
  GestureQueue& presses_;
  Ux& ux_;
  RoutineRegistry& registry_;
  Logger& logger_;
};

}  // namespace gardener