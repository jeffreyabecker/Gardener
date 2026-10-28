#pragma once

#include "serial/BoardFacts.h"

namespace gardener {

class Panel;
class CommandOutbox;
class Domain;

// The panel's own slice of the serial surface: the panel half of `device` and the buzzer. A build
// with no panel registers neither of them; the files ride with the composition, which owns the mount.
Domain* createDeviceDomain(Panel& app, CommandOutbox& outbox, const BoardFacts& board);
Domain* createBuzzerDomain(Panel& app, CommandOutbox& outbox);

}  // namespace gardener
