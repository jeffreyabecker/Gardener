#pragma once

#include <stdint.h>

#include "machine/MachineCommand.h"

namespace gardener {

// How many commands the machine's queue holds, and the size of the batch a tick drains: a link that
// writes faster than the tick runs is held in its outbox rather than blocking the link.
constexpr uint8_t CommandQueueDepth = 8;

// The way into the machine: a writer posts a command and the control tick drains it on its next
// beat, so a writer never touches the vacuum system from its own thread. It is one type with two
// implementations — the FreeRTOS queue the device builds, and whatever a case fills — because the
// queue is the seam between a link and the tick rather than either of their business.
class CommandQueue {
 public:
  virtual ~CommandQueue() = default;

  // False when the queue is full, in which case nothing was queued and the caller keeps the
  // command.
  virtual bool post(const MachineCommand& command) = 0;

  // Takes one command, in the order it was posted; false when there is none left.
  virtual bool take(MachineCommand& command) = 0;
};

}  // namespace gardener
