#pragma once

// Umbrella header for the device's own composition, beside the sketch. It names what the board
// supplies — the builder of the pump, the sensor and the valve from the pins — the web link with
// its radio, portal and page, the wifi plugin the wire serves that radio under, the Bluetooth LE
// link over the same control, and the control task that steps the tick and the interface task that
// polls the two links.

#include "BleLink.h"
#include "MachineRoot.h"
#include "MachineTasks.h"
#include "RtosCommandQueue.h"
#include "WebLink.h"
#include "WifiCommands.h"
