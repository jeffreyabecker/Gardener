#include "BindingContext.h"

#include "core/Logger.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "panel/GestureQueue.h"

namespace gardener {

gfx::DisplayDevice& BindingContext::display() { return display_; }

SystemState& BindingContext::systemState() { return state_; }

DeviceSettings& BindingContext::settings() { return settings_; }

SystemSettings& BindingContext::systemSettings() { return systemSettings_; }

BuzzerPlayer& BindingContext::buzzer() { return buzzer_; }

bool BindingContext::post(const MachineCommand& command) { return queue_.post(command); }

GestureQueue& BindingContext::presses() { return presses_; }

Ux& BindingContext::ux() { return ux_; }

RoutineRegistry& BindingContext::registry() { return registry_; }

const char* BindingContext::takePress() { return presses_.take(); }

void BindingContext::info(const char* message) { logger_.info(message); }

void BindingContext::error(const char* message) { logger_.error(message); }

}  // namespace gardener