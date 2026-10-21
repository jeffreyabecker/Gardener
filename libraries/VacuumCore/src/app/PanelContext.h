#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "app/BuzzerPlayer.h"
#include "app/MachineContext.h"
#include "app/panel/Overlay.h"
#include "app/routines/Routines.h"
#include "app/storage/Settings.h"
#include "app/storage/Storage.h"
#include "hal/Battery.h"
#include "hal/ButtonPanel.h"
#include "hal/Display.h"

namespace gardener {

class StoreClient;
class StoreServer;

// The panel's graph: the display, keys, buzzer, gauge, interpreter and files, plus the machine
// context it reads and commands through. The composition root builds it; the panel's code reads it.
class PanelContext {
 public:
  PanelContext(ButtonPanel* buttons, BuzzerPlayer* buzzer, Battery* battery, Overlay* overlay,
               Storage* storage, Settings* settings, Routines* routines, StoreClient* storeClient,
               StoreServer* storeServer, MachineContext& machine)
      : buttons_(buttons),
        buzzer_(buzzer),
        battery_(battery),
        overlay_(overlay),
        storage_(storage),
        settings_(settings),
        routines_(routines),
        storeClient_(storeClient),
        storeServer_(storeServer),
        machine_(machine) {}

  ButtonPanel& buttons() { return *buttons_; }
  BuzzerPlayer& buzzer() { return *buzzer_; }
  Battery& battery() { return *battery_; }
  // The display is the overlay — every drawing verb goes through it — so both are the same object.
  Display& display() { return *overlay_; }
  Overlay& overlay() { return *overlay_; }
  Storage& storage() { return *storage_; }
  Settings& settings() { return *settings_; }
  Routines& routines() { return *routines_; }
  StoreClient& storeClient() { return *storeClient_; }
  StoreServer& storeServer() { return *storeServer_; }
  // The machine the panel reads and commands, by the same surface a controller uses.
  MachineContext& machine() { return machine_; }

 private:
  ButtonPanel* buttons_;
  BuzzerPlayer* buzzer_;
  Battery* battery_;
  Overlay* overlay_;
  Storage* storage_;
  Settings* settings_;
  Routines* routines_;
  StoreClient* storeClient_;
  StoreServer* storeServer_;
  MachineContext& machine_;
};

}  // namespace gardener
