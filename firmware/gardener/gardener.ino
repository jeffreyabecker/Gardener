// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery
// gauge. All of the interesting code lives in the VacuumCore library so the sketch stays
// this small.

#include <VacuumCore.h>

gardener::App app(gardener::buildAppContext());

void setup() {
  app.begin();
  gardener::spawnTasks(app.context(), app.logger());
}

void loop() { app.update(); }
