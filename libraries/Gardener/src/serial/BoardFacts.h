#pragma once

#include <stdint.h>

namespace gardener {

// The board's own facts, which the `device` reply reports: the part number and the heap as it is
// when the reply is formatted. They are function pointers rather than values so nothing here holds a
// reading, and a host case supplies its own pair.
struct BoardFacts {
  const char* (*chip)();
  uint32_t (*heap)();
};

}  // namespace gardener
