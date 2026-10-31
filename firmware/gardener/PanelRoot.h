#pragma once

#include <Arduino.h>

#include <Gardener.h>
#include <Runtime.h>

#include "core/SystemState.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/PwmBuzzer.h"
#include "hal/impl/Sh1106Device.h"
#include "machine/MachineCommand.h"
#include "panel/GestureQueue.h"
#include "panel/page/RoutineRegistry.h"
#include "panel/page/Ux.h"
#include "panel/parts/BuzzerPlayer.h"
#include "routines/BerryInterpreter.h"
#include "routines/Routines.h"
#include "bindings/BindingContext.h"
#include "storage/DeviceSettings.h"

namespace gardener {

// The board this build is wired to. The machine's three pins come first, then the display bus, the
// piezo, the gauge and the three lines the key register needs: a board variant is this block and the
// sketch's constant, and nothing in the libraries names a pin.
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

// The device's composition root: every part the device is made of, owned here and handed out by
// accessor. The log and the store come first, then the queue every writer posts into and the state
// each task publishes its slice into; the machine over the parts the board wires, and the panel's
// own parts after it — the gauge, the keys, the piezo, the glass and the device's document; the
// queue a gesture goes to and the stack a page is pushed on; the registry the image registers into;
// and the app the native modules reach, the VM over it and the loader that drives it.
//
// Nothing here is freed: the device's destructor never runs, so the parts live as long as the power
// does.
class PanelRoot {
 public:
  explicit PanelRoot(const PanelPins& pins);

  PanelRoot(const PanelRoot&) = delete;
  PanelRoot& operator=(const PanelRoot&) = delete;

  // Mounts the store, brings the bus up and then the machine and the panel's parts, loads the image
  // and reports what came up. False when the glass did not answer, which the sketch treats as fatal:
  // a device that comes up blind is worse than one that resets.
  bool begin();

  Logger& logger() { return logger_; }
  Library& files() { return files_; }
  CommandQueue& commands() { return commands_; }
  SystemState& state() { return state_; }
  Machine& machine() { return machine_; }
  CommandOutbox& outbox() { return outbox_; }
  Battery& battery() { return battery_; }
  ButtonPanel& buttons() { return keys_; }
  panel::parts::BuzzerPlayer& buzzer() { return buzzer_; }
  DeviceSettings& settings() { return settings_; }
  Routines& routines() { return routines_; }

 private:
  const PanelPins pins_;
  // The log the device reports through, and the store its documents and image are read from.
  SerialLogger logger_;
  FileLibrary files_;
  // One queue into the machine, which the panel, either link and the control tick all reach, and the
  // state each task publishes its slice into.
  RtosCommandQueue commands_;
  SystemState state_;
  // The machine over the parts the board wires, and the queue the panel commands it through.
  MachineParts parts_;
  Machine machine_;
  CommandOutbox outbox_;
  // The panel's own parts.
  AdcBattery battery_;
  Buttons keys_;
  PwmBuzzer buzzerPart_;
  panel::parts::BuzzerPlayer buzzer_;
  Sh1106Device display_;
  DeviceSettings settings_;
  // The queue a gesture goes to and the stack a page is pushed on.
  panel::GestureQueue presses_;
  panel::page::Ux ux_;
  // The routines the image registers into, the app the native modules reach, and the VM over them.
  panel::page::RoutineRegistry registry_;
  bindings::BindingContext bindings_;
  BerryInterpreter interpreter_;
  Routines routines_;
};

}  // namespace gardener
