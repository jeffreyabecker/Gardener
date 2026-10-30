#pragma once

#include <Arduino.h>
#include <Gfx.h>
#include <stdint.h>

#include "core/Battery.h"
#include "core/ButtonPanel.h"
#include "core/SystemState.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "panel/parts/BuzzerPlayer.h"
#include "routines/Routines.h"
#include "storage/DeviceSettings.h"

namespace gardener::panel {

// The panel's half of the rig. The machine's three pins come first, then the display bus, the
// piezo, the gauge, and the three lines the key register needs. The sketch names them and the
// composition builds the parts from them, so this library carries no board.
struct PanelPins {
  MachinePins machine;
  uint8_t i2cSda;
  uint8_t i2cScl;
  uint32_t i2cHz;
  uint8_t buzzerPwm;
  uint8_t batteryAdc;
  float batteryDivider;
  uint8_t keyLatch;
  uint8_t keyClock;
  uint8_t keyData;
};

// The panel's graph: the display, keys, buzzer, gauge, interpreter and files, arranged around the
// machine it is handed. machine() is that machine, so the composition reaches it by the same
// surface a controller uses; the panel itself only brings it up and commands it through its own
// outbox.
//
// It commands the machine through its own outbox, the same queue a wire link uses, so a press and a
// serial command are two writers to one machine rather than two paths.
class Panel {
 public:
  Panel(Machine& machine, ButtonPanel* buttons, parts::BuzzerPlayer* buzzer, Battery* battery,
        gfx::DisplayDevice* display, Library& library, Routines* routines, CommandOutbox& outbox,
        SystemState& state)
      : machine_(machine),
        outbox_(outbox),
        buttons_(buttons),
        buzzer_(buzzer),
        battery_(battery),
        settings_(&library, state),
        state_(state),
        display_(display),
        routines_(routines) {}

  // The panel's own route to the machine's command queue: the beat flushes what a full queue
  // refused and the buttons tick posts the force-quit's stop, the same queue the links write.
  void flushCommands() { outbox_.flush(); }
  CommandOutbox& outbox() { return outbox_; }

  // The machine's parts come up first, then the panel's: the display, the gauge and buzzer, and the
  // interpreter. True when the glass answered: the panel is the one part with no fallback, so the
  // composition makes a false fatal.
  bool boot();

  ButtonPanel& buttons() { return *buttons_; }
  parts::BuzzerPlayer& buzzer() { return *buzzer_; }
  // The gauge, for the panel task's own beat and the wire's `device get`, which hold a Panel.
  Battery& battery() { return *battery_; }
  // The glass, and the frame every drawing verb lands in.
  gfx::DisplayDevice& display() { return *display_; }
  // The device's own document, for the two places that write it: the serial `device set` and the
  // BLE unit characteristic.
  DeviceSettings& settings() { return settings_; }
  // The published state the composition owns: the tasks write their slices into it and whatever
  // draws reads a copy of it.
  SystemState& systemState() { return state_; }
  Routines& routines() { return *routines_; }
  // The machine the panel is arranged around: the composition hands it to the wire's domains and the
  // machine task, which reach it by the surface a controller uses.
  Machine& machine() { return machine_; }
  // What the panel task and the panel's own boot report through, from the machine it holds.
  Logger& logger() { return machine_.logger(); }

 private:
  // The machine the panel is arranged around: the composition's, so the wire's domains and the
  // machine task reach the same one the panel commands.
  Machine& machine_;
  // The machine's queue through the panel's own outbox: the composition's, so the bindings' post()
  // and the buttons' beat retry into the same buffer the beat flushes.
  CommandOutbox& outbox_;
  ButtonPanel* buttons_;
  parts::BuzzerPlayer* buzzer_;
  Battery* battery_;
  // The device's document, ahead of the display and of the interpreter that reads it.
  DeviceSettings settings_;
  // The published state, owned by whoever composed the panel.
  SystemState& state_;
  gfx::DisplayDevice* display_;
  Routines* routines_;
};

}  // namespace gardener::panel
