#pragma once

#include <Arduino.h>

// The modules talk to each other through this context rather than through
// globals, so the whole application can be instantiated in one place.
namespace eggplan {

class BleService;
class Buttons;
class DisplayUI;
class PressureSensor;
class ProgramStore;
class Pump;
class ScriptApi;
class ScriptRuntime;
class VacuumController;

struct AppContext {
  Pump* pump = nullptr;
  PressureSensor* sensor = nullptr;
  Buttons* buttons = nullptr;
  DisplayUI* ui = nullptr;
  ProgramStore* store = nullptr;
  VacuumController* control = nullptr;
  ScriptRuntime* runtime = nullptr;
  ScriptApi* api = nullptr;
  BleService* ble = nullptr;
};

}  // namespace eggplan
