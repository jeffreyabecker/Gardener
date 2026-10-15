#pragma once

#include "core/Snapshot.h"

namespace eggplan {

// Reads the ESP facts once at boot so nothing else has to reach for ESP.* .
DeviceInfo readDeviceInfo();

}  // namespace eggplan
