#pragma once

#include <stdint.h>

#include "panel/Units.h"

namespace gardener {

class Battery;
class BuzzerPlayer;
class Display;
class JinglesConfig;
class Library;
class Logger;
class VacuumSystem;
struct MachineCommand;

// What a routine's native modules reach: the panel's own parts, without the machine's wiring or the
// links. A Panel is one, and so is the host harness, which composes the same parts with the
// interpreter and no board.
class PanelContext {
 public:
  virtual ~PanelContext() = default;

  virtual Display& display() = 0;
  // The jingle library, which is also where the selected jingle is kept.
  virtual JinglesConfig& jingles() = 0;
  // The preferences in force: the values the device's own document holds, restored at boot and
  // written back on a change. A read never touches the document, so the panel task and the
  // interface task do not share a String.
  virtual DisplayUnit unit() const = 0;
  virtual void setUnit(DisplayUnit unit) = 0;
  virtual float targetPressureKpa() const = 0;
  virtual void setTarget(float pressureKpa) = 0;
  virtual VacuumSystem& vacuum() = 0;
  virtual Battery& battery() = 0;
  virtual BuzzerPlayer& buzzer() = 0;
  // The interpreter's own files: the modules and routines it compiles, and the data it reads.
  virtual Library& library() = 0;
  virtual Logger& logger() = 0;
  // The machine's command queue, through the panel's own outbox, so a command the queue cannot
  // take waits for the panel's next beat rather than being dropped.
  virtual bool post(const MachineCommand& command) = 0;
};

}  // namespace gardener
