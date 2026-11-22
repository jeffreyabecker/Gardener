#pragma once

#include <Arduino.h>

namespace gardener {

// What a part says on the serial wire: one prefix, the lines under it, and the line it repeats on
// its own. The link holds a table of these, so a part is served without the link knowing it exists:
// it registers its prefix and answers whatever the link hands it under that word.
class CommandPlugin {
 public:
  virtual ~CommandPlugin() = default;

  // The first word of every line this plugin serves, lower case.
  virtual const char* prefix() const = 0;

  // One line under that prefix: the arguments, trimmed, with the prefix and its space gone.
  virtual void serve(Print& io, const String& args) = 0;

  // What the link writes unprompted on its own clock, if anything: silence by default, because a
  // part with nothing to report should cost the beat nothing.
  virtual void heartbeat(Print& io) {}
};

}  // namespace gardener
