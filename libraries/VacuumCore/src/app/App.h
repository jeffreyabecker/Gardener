#pragma once

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/ble/BleLink.h"
#include "app/panel/ConnectedUI.h"
#include "app/panel/DisconnectedUI.h"
#include "app/serial/SerialLink.h"

// Boot, the two links and the screen. The machine itself is serviced by the control task in
// app/Tasks.cpp — the sensor, the pump, the battery, the buzzer, the vacuum system and the keys —
// so App keeps the links and the panel until those move to their own tasks. It knows only the
// AppContext, so which parts are fitted is the build function's business.

namespace gardener {

class Logger;

class App {
 public:
  explicit App(AppContext context);

  bool begin();
  void update();

  // The context the tasks read: spawnTasks() takes it to build the per-task state.
  AppContext& context() { return context_; }
  // The boot-chosen logger, handed to the tasks that report.
  Logger& logger() { return *logger_; }

 private:
  // Which screen belongs to the machine as it is now: the owner's page or the disconnected page.
  UserInterface* screenFor(bool owned);

  // The logger is chosen once at boot from the persisted `log` preference, and the link and the
  // logger are new'd over the same Serial stream.
  AppContext context_;
  SerialLink* link_ = nullptr;
  // The radio is its own contract, not a second encoding of the serial one, so it is its own link.
  BleLink* ble_ = nullptr;
  Logger* logger_ = nullptr;
  // The panel is the device's own, so the loop paints it. Two screens, one showing: the controlling
  // link's icon while a link owns the machine, and the disconnected page while none does.
  ConnectedUI* connectedUi_ = nullptr;
  DisconnectedUI* disconnectedUi_ = nullptr;
  UserInterface* ui_ = nullptr;
  // Whether the screen showing is the connected one. It follows ownership, not attachment.
  bool ownedShown_ = false;
};

}  // namespace gardener
