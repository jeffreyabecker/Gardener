// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + Adafruit MPRLS + SH1106 OLED, with a vent valve and a 1S Li-ion battery
// gauge. All of the interesting code lives in the VacuumCore library so the sketch stays
// this small.

#include <VacuumCore.h>

gardener::App app;

void setup() { app.begin(); }

void loop() { app.update(); }
