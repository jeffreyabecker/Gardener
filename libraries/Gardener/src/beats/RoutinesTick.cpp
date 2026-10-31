#include "beats/RoutinesTick.h"

#include "routines/Routines.h"

namespace gardener {

void RoutinesTick::begin() { routines_.enterPage(); }

void RoutinesTick::tick(uint32_t nowMs) { routines_.poll(nowMs); }

void RoutinesTick::home() { routines_.showDefault(); }

}  // namespace gardener
