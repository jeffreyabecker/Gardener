#pragma once

namespace gardener {

class Panel;
class Link;
class Domain;

// The panel's own slice of the serial surface: the panel half of `device`, the files and the
// buzzer. A build with no panel registers none of them.
Domain* createDeviceDomain(Panel& app, Link& link);
Domain* createBuzzerDomain(Panel& app, Link& link);
Domain* createFsDomain(Panel& app, Link& link);

}  // namespace gardener
