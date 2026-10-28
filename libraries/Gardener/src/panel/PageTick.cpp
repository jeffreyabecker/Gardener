#include "Build.h"

#if GARDENER_PANEL

#include "panel/PageTick.h"

#include "routines/Routines.h"

namespace gardener {

void PageTick::begin() { routines_.enterPage(); }

void PageTick::tick(uint32_t nowMs) { routines_.poll(nowMs); }

void PageTick::home() { routines_.showDefault(); }

}  // namespace gardener
#endif
