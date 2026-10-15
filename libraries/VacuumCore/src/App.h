#pragma once

#include <Arduino.h>

#include "AppContext.h"
#include "BleService.h"
#include "Buttons.h"
#include "DisplayUI.h"
#include "PressureSensor.h"
#include "ProgramStore.h"
#include "Pump.h"
#include "ScriptRuntime.h"
#include "VacuumController.h"

namespace eggplan {

// Wires the modules together and owns the main loop.
class App {
 public:
  bool begin();
  void update();

  AppContext& ctx() { return ctx_; }

  // Safe to call from blocking waits (buttons, script delay) so the rest of
  // the firmware keeps running.
  static void tick();

 private:
  DisplayUI::Model buildModel();
  void onButtonEvent(ButtonId id, ButtonEvent event, uint32_t heldMs);
  void startSelectedProgram();
  void stopEverything(const char* reason);

  AppContext ctx_;
  Pump pump_;
  PressureSensor sensor_;
  Buttons buttons_;
  DisplayUI ui_;
  ProgramStore store_;
  VacuumController control_;
  NullRuntime nullRuntime_;
  ScriptApi api_{ctx_};
  BleService ble_;
  String autostartCache_;
  uint32_t lastGraphSampleMs_ = 0;
  bool ready_ = false;

  static App* instance_;
};

}  // namespace eggplan
