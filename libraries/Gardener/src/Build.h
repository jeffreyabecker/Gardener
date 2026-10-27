#pragma once

// Bluetooth is a build fact, not a preference. A build defines `GARDENER_BLUETOOTH` as 1 or 0, and
// the radio, its GATT table and the NimBLE stack are compiled in or left out with it. The default
// keeps a build that says nothing the full one, which is what a bare `arduino-cli compile` gets.
#ifndef GARDENER_BLUETOOTH
#define GARDENER_BLUETOOTH 1
#endif

// The panel is a build fact too. A build defines `GARDENER_PANEL` as 1 or 0, and the panel's
// sources compile or are skipped with it: a machine-alone build compiles neither the panel nor the
// Berry interpreter behind it. The default keeps a build that says nothing the full one, which is
// what a bare `arduino-cli compile` gets.
#ifndef GARDENER_PANEL
#define GARDENER_PANEL 1
#endif
