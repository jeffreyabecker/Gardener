#include "BindingContext.h"

#include "core/Logger.h"
#include "machine/MachineCommand.h"
#include "panel/GestureQueue.h"

namespace gardener {
namespace berry {

gfx::DisplayDevice& BindingContext::display() { return display_; }

SystemState& BindingContext::systemState() { return state_; }

panel::parts::BuzzerPlayer& BindingContext::buzzer() { return buzzer_; }

bool BindingContext::post(const MachineCommand& command) { return outbox_.post(command); }

panel::GestureQueue& BindingContext::presses() { return presses_; }

panel::page::Ux& BindingContext::ux() { return ux_; }

RoutineRegistry& BindingContext::registry() { return registry_; }

bool BindingContext::takePress(String& gesture) { return presses_.take(gesture); }

void BindingContext::info(const char* message) { logger_.info(message); }

void BindingContext::error(const char* message) { logger_.error(message); }

}  // namespace berry
}  // namespace gardener
