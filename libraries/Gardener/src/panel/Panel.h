#pragma once

#include <Arduino.h>
#include <FS.h>
#include <Gfx.h>
#include <stdint.h>

#include "hal/Battery.h"
#include "hal/ButtonPanel.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "panel/BuzzerPlayer.h"
#include "panel/GestureClassifier.h"
#include "panel/PanelContext.h"
#include "routines/Routines.h"
#include "storage/Config.h"
#include "storage/Jingles.h"

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
        Battery* battery, gfx::DisplayDevice* display, FS& fs, Library& library,
        Routines* routines)
      : Machine(logger, pins.machine),
        pins_(pins),
        buttons_(buttons),
        gestures_(*buttons),
        buzzer_(buzzer),
        battery_(battery),
        config_(&library),
        jingles_(&library),
        display_(display),
        canvas_(gfx::Canvas::over(*display)),
        routines_(routines),
        fs_(fs),
        library_(library) {}

  // The panel's own route to the machine's command queue.
  bool post(const MachineCommand& command) override { return outbox_.post(command); }
  void flushCommands() { outbox_.flush(); }

  // The force-quit: Cancel held five seconds stops and vents, then asks the panel for its root page.
  // The panel task raises it and the page loop takes it, so one request is one switch home and
  // neither side needs the other's header.
  void requestHome() { homeRequests_++; }
  uint32_t homeRequests() const { return homeRequests_; }

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
  // The glass, and the frame every drawing verb lands in.
  gfx::DisplayDevice& display() override { return *display_; }
  gfx::Canvas& canvas() override { return canvas_; }
  // The two links, as the interface task last reported them to the machine; presence is all the
  // band reports.
  bool serialConnected() const override { return Machine::serialConnected(); }
  bool bluetoothConnected() const override { return Machine::bluetoothConnected(); }
  Config& config() override { return config_; }
  JinglesConfig& jingles() override { return jingles_; }
  Library& library() override { return library_; }
  // The filesystem is the panel's own, for the mark it draws and the `fs` wire domain. It is not
  // part of the context: no routine reaches a file.
  FS& fs() { return fs_; }
  Routines& routines() { return *routines_; }

 private:
  CommandOutbox outbox_;
  // Raised by the panel task's force-quit, taken by the page loop.
  uint32_t homeRequests_ = 0;
  // Kept for boot(), which brings the display bus up on the two pins it names.
  const PanelPins pins_;
  ButtonPanel* buttons_;
  GestureClassifier gestures_;
  BuzzerPlayer* buzzer_;
  Battery* battery_;
  // The device's documents, ahead of the display and of the interpreter that is installed over the
  // jingle library.
  Config config_;
  JinglesConfig jingles_;
  gfx::DisplayDevice* display_;
  // The one frame a page draws into, over the device's own buffer.
  gfx::Canvas canvas_;
  Routines* routines_;
  FS& fs_;
  Library& library_;
};

}  // namespace gardener
