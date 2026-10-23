#pragma once

#include <Arduino.h>
#include <FS.h>
#include <stdint.h>

#include "BuzzerPlayer.h"
#include "Machine.h"
#include "MachineCommand.h"
#include "PanelContext.h"
#include "panel/GestureClassifier.h"
#include "panel/Overlay.h"
#include "routines/Routines.h"
#include "storage/PanelSettings.h"
#include "hal/Battery.h"
#include "hal/ButtonPanel.h"
#include "hal/Display.h"

namespace gardener {

// The panel's half of the rig. The machine's three pins come first, then the display bus, the
// piezo, the gauge, and the three lines the key register needs. The sketch names them, so this
// library carries no board.
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
  Panel(Logger& logger, const PanelPins& pins, ButtonPanel* buttons, BuzzerPlayer* buzzer,
        Battery* battery, Display* display, PanelSettings& settings, FS& fs, Library& library,
        Routines* routines)
      : Machine(logger, pins.machine, settings),
        pins_(pins),
        buttons_(buttons),
        gestures_(*buttons),
        buzzer_(buzzer),
        battery_(battery),
        overlay_(new Overlay(display, battery, &vacuum(), &settings)),
        routines_(routines),
        settings_(settings),
        fs_(fs),
        library_(library) {}

  // The panel's own route to the machine's command queue.
  bool post(const MachineCommand& command) override { return outbox_.post(command); }
  void flushCommands() { outbox_.flush(); }

  // The machine's parts come up first, then the panel's: the display, the gauge and buzzer, and the
  // interpreter.
  void boot();

  ButtonPanel& buttons() { return *buttons_; }
  GestureClassifier& gestures() { return gestures_; }
  BuzzerPlayer& buzzer() override { return *buzzer_; }
  Battery& battery() override { return *battery_; }
  // The machine's own reads, named again so the context's virtuals resolve to one function.
  VacuumSystem& vacuum() override { return Machine::vacuum(); }
  Logger& logger() override { return Machine::logger(); }
  // The display is the overlay — every drawing verb goes through it — so both are the same object.
  Display& display() override { return *overlay_; }
  Overlay& overlay() { return *overlay_; }
  PanelSettings& settings() override { return settings_; }
  Library& library() override { return library_; }
  // The filesystem is the panel's own, for the mark it draws and the `fs` wire domain. It is not
  // part of the context: no routine reaches a file.
  FS& fs() { return fs_; }
  Routines& routines() { return *routines_; }

 private:
  CommandOutbox outbox_;
  // Kept for boot(), which brings the display bus up on the two pins it names.
  const PanelPins pins_;
  ButtonPanel* buttons_;
  GestureClassifier gestures_;
  BuzzerPlayer* buzzer_;
  Battery* battery_;
  Overlay* overlay_;
  Routines* routines_;
  PanelSettings& settings_;
  FS& fs_;
  Library& library_;
};

}  // namespace gardener
