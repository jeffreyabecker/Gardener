// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery
// gauge. The machine lives in the VacuumCore library and the panel in GardenerPanel, so the
// sketch stays this small.

#include <GardenerPanel.h>

gardener::App app(gardener::buildAppContext());

void setup() { app.begin(); }

void loop() { vTaskDelete(NULL); }
