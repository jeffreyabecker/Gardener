#include "BindingContext.h"

#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "routines/Routines.h"

namespace gardener {
namespace berry {

gfx::DisplayDevice& BindingContext::display() { return context_.display(); }

SystemState& BindingContext::systemState() { return context_.systemState(); }

panel::parts::BuzzerPlayer& BindingContext::buzzer() { return context_.buzzer(); }

bool BindingContext::post(const MachineCommand& command) { return context_.post(command); }

panel::GestureQueue& BindingContext::presses() { return routines_.presses(); }

panel::page::Ux& BindingContext::ux() { return routines_.ux(); }

RoutineRegistry& BindingContext::registry() { return routines_.registry(); }

bool BindingContext::takePress(String& gesture) { return routines_.takePress(gesture); }

void BindingContext::info(const char* message) { logger_.info(message); }

void BindingContext::error(const char* message) { logger_.error(message); }

}  // namespace berry
}  // namespace gardener
