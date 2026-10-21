#pragma once

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/storage/Storage.h"

namespace gardener {

class BleLink;
class ConnectedUI;
class DisconnectedUI;
class Logger;
class MachineContext;
class PanelContext;
class SerialLink;

// What the machine's boot hands back: the two links it built and started, on which the panel's
// boot registers its own domains beside the machine's.
struct MachineBoot {
  SerialLink* serial;
  BleLink* ble;
};

// What the panel's boot hands back: the two screens the panel task shows.
struct PanelBoot {
  ConnectedUI* connected;
  DisconnectedUI* disconnected;
};

// Boot. The machine's boot brings up its parts, its links and its domains; the panel's boot brings
// up the panel, the VM and its registrations. The full build's App::begin composes the two in
// order, mounting the panel's storage and settings itself first because the persisted knobs — the
// radio and the diagnostics — gate the machine's boot.
MachineBoot machineBoot(MachineContext& context, Logger& logger);
PanelBoot panelBoot(PanelContext& context, Logger& logger, const MachineBoot& machine,
                    Storage::Mount mount);

class App {
 public:
  explicit App(AppContext context);

  bool begin();

 private:
  AppContext context_;
};

}  // namespace gardener
