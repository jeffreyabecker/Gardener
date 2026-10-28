#pragma once

#include "serial/BoardFacts.h"

namespace gardener {

class Machine;
class CommandOutbox;
class Domain;

Domain* createVacuumDomain(Machine& app, CommandOutbox& outbox);
// The machine's own `device` domain: what the firmware is and which assemblies are fitted. A build
// with a panel registers the panel's `device` instead, which answers the machine's fields and the
// panel's beside them.
Domain* createMachineDeviceDomain(Machine& app, CommandOutbox& outbox, const BoardFacts& board);

}  // namespace gardener
