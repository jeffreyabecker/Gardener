#pragma once

#include <stdint.h>

namespace gardener {

// A piece of per-beat work the composition installs on the control beat, for
// a part that advances with the beat — today the buzzer. The machine ticks
// whatever is installed and names no part; a build without that part installs
// nothing.
class TickService {
 public:
  virtual ~TickService() = default;
  virtual void tick(uint32_t nowMs) = 0;
};

}  // namespace gardener
