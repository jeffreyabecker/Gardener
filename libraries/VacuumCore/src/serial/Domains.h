#pragma once

namespace gardener {

class MachineContext;
class Domain;

Domain* createControlDomain(MachineContext& app);
Domain* createVacuumDomain(MachineContext& app);
// The machine's own `device` domain: what the firmware is and which assemblies are fitted. A build
// with a panel registers the panel's `device` instead, which answers the machine's fields and the
// panel's beside them.
Domain* createMachineDeviceDomain(MachineContext& app);

}  // namespace gardener
