// Eggplan Vacuum - Bluetooth controllable vacuum pump with downloadable,
// scriptable programs.
//
// ESP32-C3 + NimBLE-Arduino + Berry (optional) + Adafruit MPRLS + SH1106 OLED.
// All of the interesting code lives in the VacuumCore library so the sketch
// stays this small.

#include <VacuumCore.h>

eggplan::App app;

void setup() { app.begin(); }

void loop() { app.update(); }
