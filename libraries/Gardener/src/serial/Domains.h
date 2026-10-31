#pragma once

#include "serial/BoardFacts.h"

namespace gardener {

class Machine;
class CommandOutbox;
class Domain;
inline namespace core {
class Battery;
}
class DeviceSettings;

Domain* createVacuumDomain(Machine& app, CommandOutbox& outbox);
// What the firmware is and which assemblies are fitted: the board's facts from `board`, the gauge
// from `battery` and the unit and target from `settings`.
Domain* createDeviceDomain(Machine& app, CommandOutbox& outbox, const BoardFacts& board,
                           const Battery& battery, DeviceSettings& settings);

}  // namespace gardener
