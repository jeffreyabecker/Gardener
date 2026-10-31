#pragma once

#include "serial/BoardFacts.h"

namespace gardener {
inline namespace core { class Battery; }
inline namespace machine {
class Machine;
class CommandOutbox;
}
inline namespace storage { class DeviceSettings; }

inline namespace serial {

class Domain;

Domain* createVacuumDomain(Machine& app, CommandOutbox& outbox);
// What the firmware is and which assemblies are fitted: the board's facts from `board`, the gauge
// from `battery` and the unit and target from `settings`.
Domain* createDeviceDomain(Machine& app, CommandOutbox& outbox, const BoardFacts& board,
                           const Battery& battery, DeviceSettings& settings);

}  // namespace serial
}  // namespace gardener
