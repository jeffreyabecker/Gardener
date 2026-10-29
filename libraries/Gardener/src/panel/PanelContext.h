#pragma once

#include <stdint.h>

namespace gardener {

class Battery;
class Config;
class Library;
class Logger;
class VacuumSystem;
struct MachineCommand;

}  // namespace gardener

namespace gardener::gfx {
class Canvas;
class DisplayDevice;
}  // namespace gardener::gfx

namespace gardener::panel::parts {
class BuzzerPlayer;
}  // namespace gardener::panel::parts

namespace gardener::panel {

// What a routine's native modules reach: the panel's own parts, without the machine's wiring or the
// links. A Panel is one, and so is the host harness, which composes the same parts with the
// interpreter and no board.
class PanelContext {
 public:
  virtual ~PanelContext() = default;

  // The glass, and the one frame everything draws into: a canvas over the device's buffer, shared
  // by the page, the overlay and the `screen` module, so one flush sends what any of them wrote.
  virtual gfx::DisplayDevice& display() = 0;
  virtual gfx::Canvas& canvas() = 0;
  // The device's own document, /cfg/settings.ini: whoever needs the unit or the target under
  // [device] reads and writes that key here.
  virtual Config& config() = 0;
  virtual VacuumSystem& vacuum() = 0;
  virtual Battery& battery() = 0;
  virtual parts::BuzzerPlayer& buzzer() = 0;
  // The interpreter's own files: the modules and routines it compiles, and the data it reads.
  virtual Library& library() = 0;
  virtual Logger& logger() = 0;
  // The two ways a controller can reach the device, as the interface owner last reported them. The
  // overlay reports presence: a connected link that has not acquired control still counts.
  virtual bool serialConnected() const = 0;
  virtual bool bluetoothConnected() const = 0;
  // The machine's command queue, through the panel's own outbox, so a command the queue cannot
  // take waits for the panel's next beat rather than being dropped.
  virtual bool post(const MachineCommand& command) = 0;
};

}  // namespace gardener::panel
