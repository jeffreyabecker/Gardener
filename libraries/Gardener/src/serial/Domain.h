#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "machine/MachineCommand.h"

namespace gardener {
inline namespace serial {

// The longest command line a domain serves: the link refuses a longer one, and so does each domain
// handed one. A command body, when a domain defines one, is not bounded here.
constexpr uint16_t COMMAND_LINE_MAX = 128;

// One domain of the serial surface. The dispatcher reads only the domain token, then hands it the
// stream at that point. `io` is the port: the domain reads the rest of its command from it and
// writes its reply back to it. `outbox_` is the link's queue, so a command the control task cannot
// take yet waits for the link's next poll instead of being lost.
class Domain {
 public:
  explicit Domain(CommandOutbox& outbox) : outbox_(outbox) {}
  virtual ~Domain() {}
  virtual const char* name() const = 0;
  virtual void execute(Stream& io) = 0;

 protected:
  CommandOutbox& outbox_;
};

}  // namespace serial
}  // namespace gardener
