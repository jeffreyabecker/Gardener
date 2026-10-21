#pragma once

namespace gardener {

class MachineContext;
class PanelContext;
class Domain;

Domain* createControlDomain(MachineContext& app);
Domain* createDeviceDomain(PanelContext& app);
Domain* createVacuumDomain(MachineContext& app);
Domain* createBuzzerDomain(PanelContext& app);
Domain* createFsDomain(PanelContext& app);

}  // namespace gardener
