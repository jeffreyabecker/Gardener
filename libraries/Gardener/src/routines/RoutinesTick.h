#pragma once

#include <stdint.h>

namespace gardener {
inline namespace routines {

class Routines;

// The page's beat: the root scene goes on when the panel comes up, the top scene is polled on each
// beat, and the go-home gesture gives the panel back to the root.
class RoutinesTick {
 public:
  explicit RoutinesTick(Routines& routines) : routines_(routines) {}

  // The panel came up: the root scene goes on with the first page, so a scene re-reads the unit and
  // draws rather than trusting what a previous boot left on the glass.
  void begin();

  // One beat: the top scene's poll and the frame with it.
  void tick(uint32_t nowMs);

  // The force-quit: leave whichever scenes are showing and give the panel back to the root.
  void home();

 private:
  Routines& routines_;
};

}  // namespace routines
}  // namespace gardener
