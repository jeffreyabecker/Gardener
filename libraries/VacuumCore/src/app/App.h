#pragma once

#include <Arduino.h>

#include "core/ReplyQueue.h"
#include "core/Snapshot.h"
#include "hal/Buttons.h"
#include "hal/Device.h"
#include "hal/Display.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/Storage.h"
#include "hosts/ble/BleService.h"
#include "hosts/commands/CommandRouter.h"
#include "hosts/script/NullRuntime.h"
#include "hosts/script/ScriptApi.h"
#include "services/ProgramStore.h"
#include "services/ProgramUploader.h"
#include "services/VacuumControl.h"
#include "ui/UiController.h"

// Composition root. This is the only class that knows the whole graph: every other
// module is handed exactly what it uses, by reference, at construction.

namespace eggplan {

class App {
 public:
  App();

  bool begin();
  void update();

  // Safe to call from blocking waits (buttons, script delay) so sensors, UI and the
  // transport keep running.
  static void tick();

 private:
  static PumpConfig pumpConfig();
  static PidConfig pidConfig();

  void onIntent(const Intent& intent);
  void startProgram(size_t index);
  void stopEverything(const char* reason);
  void refreshSnapshot();

  static App* instance_;

  // Declaration order is construction order, and several members hold references to
  // the ones above them.
  Storage storage_;
  PressureSensor sensor_;
  Pump pump_;
  Buttons buttons_;
  Display display_;
  ProgramStore store_;
  VacuumControl control_;
  DeviceInfo device_;
  SystemSnapshot snapshot_;
  ScriptOverlay overlay_;
  UiSettings uiSettings_;
  TelemetrySettings telemetry_;
  ScriptApi api_;
  NullRuntime runtime_;
  Renderer renderer_;
  UiController ui_;
  ReplyQueue replies_;
  ReplyQueue inbound_;
  CommandRouter router_;
  ProgramUploader uploader_;
  BleService ble_;
  bool ready_ = false;
};

}  // namespace eggplan
