#pragma once

namespace gardener {

class Machine;
class Domain;

Domain* createControlDomain(Machine& app);
Domain* createVacuumDomain(Machine& app);
// The machine's own `device` domain: what the firmware is and which assemblies are fitted. A build
// with a panel registers the panel's `device` instead, which answers the machine's fields and the
// panel's beside them.
Domain* createMachineDeviceDomain(Machine& app);

}  // namespace gardener
