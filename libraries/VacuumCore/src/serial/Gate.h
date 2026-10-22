#pragma once

#include <Arduino.h>

#include "Machine.h"
#include "Ownership.h"

namespace gardener {

// The gate every writing serial verb goes through. A connected link that has not acquired control
// is a reader, and this is the one spelling of its refusal, so the domains cannot disagree about
// what "in control" means or what the error looks like.
inline bool hasControl(Machine& app, const Link& link) {
  return app.ownership().ownedBy(link);
}

inline bool requireControl(Machine& app, const Link& link, Stream& io) {
  if (hasControl(app, link)) return true;
  io.print("err not_controlling\n");
  return false;
}

}  // namespace gardener
