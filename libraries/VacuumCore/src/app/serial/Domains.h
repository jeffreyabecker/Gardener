#pragma once

namespace gardener {

class AppContext;
class Domain;

Domain* createDeviceDomain(AppContext& app);
Domain* createVacuumDomain(AppContext& app);
Domain* createBuzzerDomain(AppContext& app);
Domain* createFsDomain(AppContext& app);

}  // namespace gardener
