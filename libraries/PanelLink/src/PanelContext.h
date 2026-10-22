#pragma once

#include <FS.h>

namespace gardener {

class Battery;
class BuzzerPlayer;
class Display;
class Logger;
class PanelSettings;
class VacuumSystem;

// What a routine's native modules reach: the panel's own parts, without the machine's wiring or the
// links. A Panel is one, and so is the host harness, which composes the same parts with the
// interpreter and no board.
class PanelContext {
 public:
  virtual ~PanelContext() = default;

  virtual Display& display() = 0;
  virtual PanelSettings& settings() = 0;
  virtual VacuumSystem& vacuum() = 0;
  virtual Battery& battery() = 0;
  virtual BuzzerPlayer& buzzer() = 0;
  // The filesystem the panel's files live on: the interpreter's modules and routines, each
  // routine's values, the jingle library and the mark.
  virtual FS& fs() = 0;
  virtual Logger& logger() = 0;
};

}  // namespace gardener
