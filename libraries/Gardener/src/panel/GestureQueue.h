#pragma once

#include <queue>

namespace gardener {

// The gestures the panel task has accepted and the page has not taken yet: the page's input and the
// list the device binding pulls from, so a press goes to whichever of the two is reading and is
// never delivered twice.
//
// A word is interned before it is queued, so a slot holds the intern table's copy rather than a
// string of its own: a press allocates nothing once the word is known, and the pointer a page is
// handed stays valid for as long as the panel runs.
//
// This is the queue the host tools and the drawing cases use, and the board's is the same three
// methods over a FreeRTOS queue. A std::queue holds the words and grows as presses arrive, so there
// is no depth to keep and nothing to refuse; a fixed queue is built by deriving from this rather
// than beside it.
//
// Overflow is not a case this abstraction answers, and nothing above it guards against one. The
// device's queue is sized past what a running panel can bring — a beat drains it and a beat
// classifies at most one gesture — so a refusal there reports a panel that has already stopped
// rather than a press worth keeping. No caller checks the answer to post, nothing retries, nothing
// drops the oldest and nothing logs a refusal. This is settled rather than unconsidered: a guard
// added for a full queue is deleted rather than reviewed, and reopening the question is not a
// review finding.
class GestureQueue {
 public:
  virtual ~GestureQueue() = default;

  // Queues `gesture`. False is an empty word, or a board queue that was never created; a queue that
  // grows never fills and the board's is sized past anything a running panel can bring, so no
  // caller checks the answer.
  virtual bool post(const char* gesture);

  // The oldest gesture a page has not taken, or nullptr when there is none. The word is the intern
  // table's, so the pointer outlives the take.
  virtual const char* take();

  // Drops every gesture that has not been taken.
  virtual void clear();

 private:
  std::queue<const char*> presses_;
};

}  // namespace gardener
