#pragma once

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/StreamLogger.h"
#include "app/UI.h"
#include "app/serial/SerialLink.h"

// The application loop: boot, the service order, the run log and the screen. It knows only the
// AppContext, so which parts are fitted is the build function's business.
//
// The device is a serial peripheral: the front panel keys are read on demand, but no key changes
// what the device does, so the loop services the devices, the control loop and the link, in that
// order. App is also the only place that reports anything: the context returns values, and the
// run and the keys are logged from here.

namespace gardener {

class App {
 public:
  explicit App(AppContext context);

  bool begin();
  void update();

 private:
  void logRunChanges(uint32_t nowMs);
  void logButtonPresses();

  // App owns the diagnostic switch the link toggles (`device log=`), and both the link and the
  // logger honour it. The link and the logger are new'd over the same Serial stream.
  AppContext context_;
  bool logEnabled_ = true;
  SerialLink* link_ = nullptr;
  StreamLogger* logger_ = nullptr;
  // The panel is the device's own, so the loop paints it: what the run is doing, and the two
  // pressures it is about. A pointer, and new'd with the link and the logger, because a build with
  // no panel has no UI at all — and nothing on the device deletes what it built at boot.
  ConnectedUI* ui_ = nullptr;
  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
};

}  // namespace gardener
