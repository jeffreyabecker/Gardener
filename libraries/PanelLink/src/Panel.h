#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "BuzzerPlayer.h"
#include "Machine.h"
#include "panel/Overlay.h"
#include "routines/Routines.h"
#include "storage/PanelSettings.h"
#include "hal/Battery.h"
#include "hal/ButtonPanel.h"
#include "hal/Display.h"

namespace gardener {

// The panel's half of the rig. The machine's three pins come first, then the display bus, the
// piezo, the gauge and the four keys. The sketch names them, so this library carries no board.
struct PanelPins {
  MachinePins machine;
  uint8_t i2cSda;
  uint8_t i2cScl;
  uint32_t i2cHz;
  uint8_t buzzerPwm;
  uint8_t batteryAdc;
  float batteryDivider;
  uint8_t up;
  uint8_t down;
  uint8_t ok;
  uint8_t back;
};

// The panel's graph: the display, keys, buzzer, gauge, interpreter and files, arranged on top of the
// machine it reads and commands. It *is* a Machine, so the panel's code reaches the machine by the
// same surface a controller uses and there is no second name for it.
//
// It is also one of the links on the ownership stack: the composition pushes it at boot, so it is
// the default controller. The two control notifications are what its pages follow — the panel's own
// pages while it drives, the connected page while an external link does.
class Panel : public Machine, public Link {
 public:
  static constexpr uint8_t Code = 3;

  Panel(Logger& logger, const PanelPins& pins, ButtonPanel* buttons, BuzzerPlayer* buzzer,
        Battery* battery, Display* display, PanelSettings* settings, Routines* routines)
      : Machine(logger, *settings, pins.machine),
        pins_(pins),
        buttons_(buttons),
        buzzer_(buzzer),
        battery_(battery),
        overlay_(new Overlay(display, battery, &vacuum())),
        routines_(routines) {}

  const char* name() const override { return "panel"; }
  uint8_t code() const override { return Code; }

  void gainedControl() override { external_ = false; }
  void lostControl() override { external_ = true; }
  // True while an external link drives: the panel shows that link's page rather than its own.
  bool externalPage() const { return external_; }

  // The machine's parts come up first, then the panel's: the display, the gauge and buzzer, and the
  // interpreter.
  void boot();

  ButtonPanel& buttons() { return *buttons_; }
  BuzzerPlayer& buzzer() { return *buzzer_; }
  Battery& battery() { return *battery_; }
  // The display is the overlay — every drawing verb goes through it — so both are the same object.
  Display& display() { return *overlay_; }
  Overlay& overlay() { return *overlay_; }
  // The machine holds the settings, and the panel is built with the panel's subclass of them.
  PanelSettings& settings() { return static_cast<PanelSettings&>(Machine::settings()); }
  Routines& routines() { return *routines_; }

 private:
  // Kept for boot(), which brings the display bus up on the two pins it names.
  const PanelPins pins_;
  ButtonPanel* buttons_;
  BuzzerPlayer* buzzer_;
  Battery* battery_;
  Overlay* overlay_;
  Routines* routines_;
  // Written on the control beat, read on the panel beat: one aligned flag, the same trade the
  // connection flags make.
  volatile bool external_ = false;
};

}  // namespace gardener
