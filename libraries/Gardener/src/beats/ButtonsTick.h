#pragma once

#include <Arduino.h>

#include "panel/parts/GestureClassifier.h"

namespace gardener {

class Logger;
class CommandQueue;
class GestureQueue;

// The keys' policy on the panel beat: one call to the classifier, the two holds the panel takes for
// itself, and the gesture a page gets. A caller steps it, so the sequence a beat runs is reached by
// a case rather than only by the task that runs it; nothing here needs an RTOS, a clock of its own
// or a board.
//
// The reset stops and vents the machine and belongs to the composition, so takeReset() hands it
// back rather than restarting from inside a beat. The force-quit leaves the page switch to the page
// tick, so it parks takeHome() rather than moving the page from here.
class ButtonsTick {
 public:
  // The keys are this beat's: the classifier is the tick's own, built on the reader it is handed,
  // so no other caller settles the same panel between two beats.
  ButtonsTick(ButtonPanel& buttons, GestureQueue& presses, CommandQueue& queue, Logger& logger);

  // The beat's start: the panel's two safety holds are named as the commands they are, so the
  // classifier answers `system:reboot` and `system:quit` for them.
  void begin();

  // One call to the classifier, and the policy for what it answers.
  void tick();

  // True once after the reset hold arrived on a beat, so the task can release and restart.
  bool takeReset();
  // True once after the force-quit asked for the root page.
  bool takeHome();

 private:
  // The gesture goes to the queue the page drains. The classifier is its one consumer, so it is
  // never delivered twice. The queue interns the word, so the pointer need only outlive the call.
  void deliver(const char* gesture);

  GestureClassifier classifier_;
  GestureQueue& presses_;
  CommandQueue& queue_;
  Logger& logger_;
  bool reset_ = false;
  bool home_ = false;
};

}  // namespace gardener