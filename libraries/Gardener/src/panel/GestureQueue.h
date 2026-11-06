#pragma once

#include <stdint.h>

namespace gardener {

// How many gestures a queue holds. The panel drains on every beat and the classifier answers at most
// one gesture a beat, so the depth is not what a burst needs: it is what a drain that stopped leaves
// behind, set far past anything a running panel can bring.
constexpr uint8_t GestureQueueDepth = 64;

// The gestures the panel task has accepted and the page has not taken yet: the page's input and the
// list the device binding pulls from, so a press goes to whichever of the two is reading and is
// never delivered twice.
//
// A word is interned before it is queued, so a slot holds the intern table's copy rather than a
// string of its own: a press allocates nothing once the word is known, and the pointer a page is
// handed stays valid for as long as the panel runs.
//
// A post never throws away what is already waiting. A full queue refuses the gesture and answers
// false, so the loss stays with the caller that can still say so.
//
// It is an interface rather than a ring because the writers are not one task — the keys, and in
// time a link — so the board builds a FreeRTOS queue under it and the host tools build the plain
// one.
class GestureQueue {
 public:
  virtual ~GestureQueue() = default;

  // Queues `gesture`, or answers false when there is no room for it.
  virtual bool post(const char* gesture) = 0;

  // The oldest gesture a page has not taken, or nullptr when there is none. The word is the intern
  // table's, so the pointer outlives the take.
  virtual const char* take() = 0;

  // Drops every gesture that has not been taken.
  virtual void clear() = 0;
};

}  // namespace gardener
