#pragma once

#include <FS.h>

namespace gardener {

class CommandOutbox;
class Domain;

// The panel build's `fs` domain: the device's files over the wire, on the store the composition
// mounted. It is handed the filesystem rather than the panel because the mount is the composition's,
// and no other part of the panel reaches a file.
Domain* createFsDomain(CommandOutbox& outbox, FS& fs);

}  // namespace gardener
