#pragma once

#include <Arduino.h>

namespace gardener {

class Link;
class Machine;

// One of `device set`'s machine fields — `flow`, `volume`, `floor`, `pump_max`, `deadband` and
// `valve` — parsed, bounded, refused and applied the same way whichever build serves the command.
// The panel domain and the machine-alone domain both call this, so the two cannot disagree about a
// bound, a refusal's shape or which task applies the value: the reply is written here and the
// change is posted to the control task.
//
// `extra` is whatever followed the value, so a second token is refused here like anywhere else.
// True when `field` is one of the six and a reply has been written; false leaves the field to the
// caller, which is where the panel's own `unit` and both builds' `log` live.
bool setMachineField(Stream& io, Machine& app, Link& link, const String& field, const String& value,
                     const String& extra);

}  // namespace gardener
