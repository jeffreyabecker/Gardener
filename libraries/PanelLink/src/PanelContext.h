#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "BuzzerPlayer.h"
#include "MachineContext.h"
#include "panel/Overlay.h"
#include "routines/Routines.h"
#include "storage/Settings.h"
#include "storage/Storage.h"
#include "hal/Battery.h"
#include "hal/ButtonPanel.h"
#include "hal/Display.h"

namespace gardener {

class StoreClient;
class StoreServer;

// The panel's graph: the display, keys, buzzer, gauge, interpreter and files, arranged on top of the
// machine it reads and commands. It *is* a MachineContext, so the panel's code reaches the machine by
// the same surface a controller uses and there is no second name for it.
class PanelContext : public MachineContext {
 public:
  PanelContext(ButtonPanel* buttons, BuzzerPlayer* buzzer, Battery* battery, Overlay* overlay,
               Storage* storage, Settings* settings, Routines* routines, StoreClient* storeClient,
               StoreServer* storeServer, MachineContext& machine)
      : MachineContext(&machine.vacuum(), &machine.ownership(), &machine.capabilities()),
        buttons_(buttons),
        buzzer_(buzzer),
        battery_(battery),
        overlay_(overlay),
        storage_(storage),
        settings_(settings),
        routines_(routines),
        storeClient_(storeClient),
        storeServer_(storeServer) {}

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
};

}  // namespace gardener
