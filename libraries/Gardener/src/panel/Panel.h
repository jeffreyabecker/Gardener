#pragma once

#include <Arduino.h>
#include <Gfx.h>
#include <stdint.h>

#include "core/Battery.h"
#include "core/ButtonPanel.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "panel/parts/BuzzerPlayer.h"
#include "panel/parts/GestureClassifier.h"
#include "panel/PanelContext.h"
#include "routines/Routines.h"
#include "storage/Config.h"

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

// The panel's graph: the display, keys, buzzer, gauge, interpreter and files, arranged on top of the
// machine it reads and commands. It *is* a Machine, so the panel's code reaches the machine by the
// same surface a controller uses and there is no second name for it.
//
// It commands the machine through its own outbox, the same queue a wire link uses, so a press and a
// serial command are two writers to one machine rather than two paths.
class Panel : public Machine, public PanelContext {
 public:
  Panel(Logger& logger, Pump* pump, PressureSensor* sensor, VentValve* valve,
        ButtonPanel* buttons, parts::BuzzerPlayer* buzzer, Battery* battery,
        gfx::DisplayDevice* display, Library& library, Routines* routines, CommandQueue& queue)
      : Machine(logger, pump, sensor, valve),
        outbox_(queue),
        buttons_(buttons),
        gestures_(*buttons),
        buzzer_(buzzer),
        battery_(battery),
        config_(&library),
        display_(display),
        canvas_(display->rootCanvas()),
        routines_(routines),
        library_(library) {}

  // The panel's own route to the machine's command queue: the beat flushes what a full queue
  // refused and the buttons tick posts the force-quit's stop, the same queue the links write.
  bool post(const MachineCommand& command) override { return outbox_.post(command); }
  void flushCommands() { outbox_.flush(); }
  CommandOutbox& outbox() { return outbox_; }

  // The machine's parts come up first, then the panel's: the display, the gauge and buzzer, and the
  // interpreter. True when the glass answered: the panel is the one part with no fallback, so the
  // composition makes a false fatal.
  bool boot();

  ButtonPanel& buttons() { return *buttons_; }
  parts::GestureClassifier& gestures() { return gestures_; }
  parts::BuzzerPlayer& buzzer() override { return *buzzer_; }
  Battery& battery() override { return *battery_; }
  // The machine's own reads, named again so the context's virtuals resolve to one function.
  VacuumSystem& vacuum() override { return Machine::vacuum(); }
  Logger& logger() override { return Machine::logger(); }
  // The glass, and the frame every drawing verb lands in.
  gfx::DisplayDevice& display() override { return *display_; }
  gfx::Canvas& canvas() override { return canvas_; }
  // The two links, as the interface task last reported them to the machine; presence is all the
  // band reports.
  bool serialConnected() const override { return Machine::serialConnected(); }
  bool bluetoothConnected() const override { return Machine::bluetoothConnected(); }
  Config& config() override { return config_; }
  Library& library() override { return library_; }
  Routines& routines() { return *routines_; }

 private:
  CommandOutbox outbox_;
  ButtonPanel* buttons_;
  parts::GestureClassifier gestures_;
  parts::BuzzerPlayer* buzzer_;
  Battery* battery_;
  // The device's document, ahead of the display and of the interpreter that reads it.
  Config config_;
  gfx::DisplayDevice* display_;
  // The one frame a page draws into, over the device's own buffer.
  gfx::Canvas canvas_;
  Routines* routines_;
  Library& library_;
};

}  // namespace gardener::panel
