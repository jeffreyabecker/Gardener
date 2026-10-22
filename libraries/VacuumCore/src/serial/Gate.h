#pragma once

#include <Arduino.h>

#include "MachineContext.h"
#include "Ownership.h"

namespace gardener {

// The gate every writing serial verb goes through. A connected link that has not acquired control
// is a reader, and this is the one spelling of its refusal, so the domains cannot disagree about
// what "in control" means or what the error looks like.
inline bool hasControl(MachineContext& app) {
  return app.ownership().ownedBy(Link::Serial);
}

inline bool requireControl(MachineContext& app, Stream& io) {
  if (hasControl(app)) return true;
  io.print("err not_controlling\n");
  return false;
}

}  // namespace gardener
