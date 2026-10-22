#pragma once

namespace gardener {

class Machine;
class Link;
class Domain;

Domain* createControlDomain(Machine& app, Link& link);
Domain* createVacuumDomain(Machine& app, Link& link);
// The machine's own `device` domain: what the firmware is and which assemblies are fitted. A build
// with a panel registers the panel's `device` instead, which answers the machine's fields and the
// panel's beside them.
Domain* createMachineDeviceDomain(Machine& app, Link& link);

}  // namespace gardener
