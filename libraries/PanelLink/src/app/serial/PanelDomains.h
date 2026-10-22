#pragma once

namespace gardener {

class PanelContext;
class Domain;

// The panel's own slice of the serial surface: the panel half of `device`, the files and the
// buzzer. A build with no panel registers none of them.
Domain* createDeviceDomain(PanelContext& app);
Domain* createBuzzerDomain(PanelContext& app);
Domain* createFsDomain(PanelContext& app);

}  // namespace gardener
