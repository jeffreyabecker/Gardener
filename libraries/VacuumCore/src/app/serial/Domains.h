#pragma once

namespace gardener {

class Domain;
class DomainContext;
class DomainList;

Domain* createDeviceDomain(DomainContext& context);
Domain* createVacuumDomain(DomainContext& context);
Domain* createBuzzerDomain(DomainContext& context);
Domain* createHelpDomain(DomainContext& context, DomainList& domains);

}  // namespace gardener
