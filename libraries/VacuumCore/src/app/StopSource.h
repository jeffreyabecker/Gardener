#pragma once

#include <stdint.h>

namespace gardener {

// The machine's safety slot: the build installs a source that watches the
// operator's stop on the control beat and reports a verdict. The machine
// applies the verdict — stop the run, or reset the device — so it never names
// a key. A build with no local stop installs nothing, and the slot stays
// empty.
class StopSource {
 public:
  enum class Verdict : uint8_t { None, Stop, Reset };

  virtual ~StopSource() = default;
  virtual Verdict tick() = 0;
};

}  // namespace gardener
