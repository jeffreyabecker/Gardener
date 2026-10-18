#include "app/ContextFactory.h"

#include <Wire.h>

namespace gardener {

ContextFactory::ContextFactory()
    : sensor_(Wire),
      overlay_(display_, battery_),
      system_(pump_, sensor_, vent_),
      context_(sensor_, pump_, vent_, battery_, buttons_, overlay_, system_) {}

}  // namespace gardener
