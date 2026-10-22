#pragma once

// Bluetooth is a build fact, not a preference. A build defines `GARDENER_BLUETOOTH` as 1 or 0, and
// the radio, its GATT table and the NimBLE stack are compiled in or left out with it. The default
// keeps a build that says nothing the full one, which is what a bare `arduino-cli compile` gets.
#ifndef GARDENER_BLUETOOTH
#define GARDENER_BLUETOOTH 1
#endif
