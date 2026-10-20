#pragma once

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/StreamLogger.h"
#include "app/ble/BleLink.h"
#include "app/panel/ConnectedUI.h"
#include "app/panel/DisconnectedUI.h"
#include "app/serial/SerialLink.h"

// The application loop: boot, the service order, the run log and the screen. It knows only the
// AppContext, so which parts are fitted is the build function's business.
//
// The device answers on two transports — the serial line and the radio — and both live at once. The
// front panel keys are read on demand, but no key changes what the device does, so the loop services
// the devices, the control loop and the links, in that order. App is also the only place that
// reports anything: the context returns values, and the run and the keys are logged from here.

namespace gardener {

class App {
 public:
  explicit App(AppContext context);

  bool begin();
  void update();

  // The context the tasks read: spawnTasks() takes it to build the per-task state.
  AppContext& context() { return context_; }

 private:
  void logRunChanges(uint32_t nowMs);
  void logButtonPresses();
  // The local stop: a Back hold stops and vents, whatever the link is doing.
  void watchBackHold();
  // Back+OK held together: the local reset that clears a held ownership record.
  void watchResetHold();
  // Which screen belongs to the machine as it is now: the owner's page or the disconnected page.
  UserInterface* screenFor(bool owned);

  // The context owns the diagnostic switch (`device set log`), and both the link and the logger
  // honour it. The link and the logger are new'd over the same Serial stream.
  AppContext context_;
  SerialLink* link_ = nullptr;
  // The radio is its own contract, not a second encoding of the serial one, so it is its own link.
  BleLink* ble_ = nullptr;
  StreamLogger* logger_ = nullptr;
  // The panel is the device's own, so the loop paints it. Two screens, one showing: the controlling
  // link's icon while a link owns the machine, and the disconnected page while none does.
  ConnectedUI* connectedUi_ = nullptr;
  DisconnectedUI* disconnectedUi_ = nullptr;
  UserInterface* ui_ = nullptr;
  // Whether the screen showing is the connected one. It follows ownership, not attachment.
  bool ownedShown_ = false;
  // One stop per Back hold: set when the hold fires, cleared when the key comes up.
  bool backStopFired_ = false;
  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
};

}  // namespace gardener
