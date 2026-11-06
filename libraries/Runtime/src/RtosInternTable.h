#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "core/Gestures.h"

namespace gardener::runtime {

// The intern table the device installs: the panel's own, with a lock across the insert. The panel
// task presses and, once a link can press too, so does the interface task, and a word the table has
// not seen is a push into a vector that two tasks cannot do at once.
//
// The lock is a mutex rather than a critical section because an insert allocates, and the heap is not
// to be touched with interrupts off.
class RtosInternTable : public Gestures::InternTable {
 public:
  RtosInternTable();
  ~RtosInternTable() override;

  const char* intern(const char* gesture) override;

 private:
  SemaphoreHandle_t lock_ = nullptr;
};

}  // namespace gardener::runtime
