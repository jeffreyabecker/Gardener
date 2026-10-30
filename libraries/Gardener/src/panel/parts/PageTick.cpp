#include "panel/parts/PageTick.h"

#include "routines/Routines.h"

namespace gardener::panel::parts {

void PageTick::begin() { routines_.enterPage(); }

void PageTick::tick(uint32_t nowMs) { routines_.poll(nowMs); }

void PageTick::home() { routines_.showDefault(); }

}  // namespace gardener::panel::parts
