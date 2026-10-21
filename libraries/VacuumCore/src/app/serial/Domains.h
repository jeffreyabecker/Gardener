#pragma once

namespace gardener {

class MachineContext;
class Domain;

Domain* createControlDomain(MachineContext& app);
Domain* createVacuumDomain(MachineContext& app);

}  // namespace gardener
