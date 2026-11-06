#pragma once

#include <stdint.h>

#include "core/Knob.h"
#include "machine/MachineCommand.h"

namespace gardener {

// How many commands the machine's queue holds: the depth is what a burst from a link is absorbed
// into, and a post that finds it full is refused rather than waited on.
GARDENER_KNOB_VALID(uint8_t, CommandQueueDepth, GARDENER_COMMAND_QUEUE_DEPTH, 8,
                    GARDENER_COMMAND_QUEUE_DEPTH > 0, "a queue of nothing drops every command");

// The way into the machine: a writer posts a command and the control tick drains it on its next
// beat, so a writer never touches the vacuum system from its own thread. It is one type with two
// implementations — the FreeRTOS queue the device builds, and whatever a case fills — because the
// queue is the seam between a link and the tick rather than either of their business.
class CommandQueue {
 public:
  virtual ~CommandQueue() = default;

  // False when the queue is full, in which case nothing was queued and the command is dropped: a
  // writer posts and does not wait.
  virtual bool post(const MachineCommand& command) = 0;

  // Takes one command, in the order it was posted; false when there is none left.
  virtual bool take(MachineCommand& command) = 0;
};

}  // namespace gardener