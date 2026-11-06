#pragma once

#include <stdint.h>

#include "core/Gestures.h"

namespace gardener {

// The gestures the panel task has accepted and the page has not taken yet. Its writer is the panel
// task; its readers are the device binding's pull and the scene stack, so the two are never the
// same caller, and nothing here takes a lock. A full queue drops the oldest so the newest gesture
// still arrives, and the depth is set far past what a beat can bring — the panel drains every
// beat and the classifier answers at most one gesture a beat, so a drop means the drain stopped
// rather than that the panel is busy.
//
// A gesture is one of the words `Gestures` names, so a slot holds the intern table's copy rather
// than a string of its own: a press allocates nothing once the word is known, and the pointer a
// page is handed stays valid for as long as the panel runs.
//
// It is the page's input as well, so a page and the device binding drain one list and no press is
// delivered twice.
class GestureQueue {
 public:
  static constexpr uint8_t Capacity = 64;

  virtual void post(const char* gesture);
  virtual void clear();

  // The oldest gesture a page has not taken, or nullptr when there is none. The word is the intern
  // table's, so the pointer outlives the take.
  const char* take();

 private:
  const char* presses_[Capacity] = {};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};

}  // namespace gardener
