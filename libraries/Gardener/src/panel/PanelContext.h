#pragma once

#include "core/SystemState.h"

namespace gardener {

struct MachineCommand;

}  // namespace gardener

namespace gardener::gfx {
class DisplayDevice;
}  // namespace gardener::gfx

namespace gardener::panel::parts {
class BuzzerPlayer;
}  // namespace gardener::panel::parts

namespace gardener::panel {

// What a routine's native modules reach: the panel's own parts — the glass, the published state, the
// piezo and the machine's queue — without the machine's wiring or the links. A Panel is one, and so
// is the host harness, which composes the same parts with the interpreter and no board.
class PanelContext {
 public:
  virtual ~PanelContext() = default;

  // The glass, and the one frame everything draws into: the device's own buffer, shared by the page,
  // the overlay and the `screen` module, so one flush sends what any of them wrote. It is a Canvas,
  // so a caller that draws hands this where a canvas is wanted.
  virtual gfx::DisplayDevice& display() = 0;
  // What the device is doing, as the composition built it and the tasks publish it: the run and the
  // chamber, the pack, the two links and the unit the device prints in. It is a dependency handed in
  // rather than an answer worked out here, and there is nothing on it to command.
  virtual SystemState& systemState() = 0;
  virtual parts::BuzzerPlayer& buzzer() = 0;
  // The machine's command queue, through the panel's own outbox, so a command the queue cannot
  // take waits for the panel's next beat rather than being dropped.
  virtual bool post(const MachineCommand& command) = 0;
};

}  // namespace gardener::panel
