#pragma once

namespace gardener {

class Panel;
class CommandOutbox;
class Domain;

// The panel's own slice of the serial surface: the panel half of `device`, the files and the
// buzzer. A build with no panel registers none of them.
Domain* createDeviceDomain(Panel& app, CommandOutbox& outbox);
Domain* createBuzzerDomain(Panel& app, CommandOutbox& outbox);
Domain* createFsDomain(Panel& app, CommandOutbox& outbox);

}  // namespace gardener
