#include "RtosInternTable.h"

namespace gardener::runtime {

RtosInternTable::RtosInternTable() : lock_(xSemaphoreCreateMutex()) {}

RtosInternTable::~RtosInternTable() {
  if (lock_ != nullptr) vSemaphoreDelete(lock_);
}

const char* RtosInternTable::intern(const char* gesture) {
  if (lock_ == nullptr) return Gestures::InternTable::intern(gesture);
  xSemaphoreTake(lock_, portMAX_DELAY);
  const char* word = Gestures::InternTable::intern(gesture);
  xSemaphoreGive(lock_);
  return word;
}

}  // namespace gardener::runtime
