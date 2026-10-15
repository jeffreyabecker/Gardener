#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "core/Snapshot.h"

// Encodes the machine view for the wire: space separated key=value tokens with token
// values and a fixed key order, built into a caller supplied buffer.
//
// No JSON: it would need escaping and an allocation per telemetry notification, and the
// only consumers are a host tool and a human with a BLE terminal. The same payload
// shape is used by the `status` reply and by telemetry, so a client writes one parser.

namespace eggplan {
namespace snapshottext {

// "kpa=41.23 vac=60.10 pct=59.3 target=40.0 floor=41.3 duty=72.0 state=HOLD fault=- up=123456 ble=1"
size_t statusBody(const SystemSnapshot& snapshot, char* out, size_t size);

// "fw=0.1.0 chip=ESP32-C3 heap=298412 tach=0"
size_t infoBody(const SystemSnapshot& snapshot, char* out, size_t size);

const char* stateToken(ControlState state);
const char* faultToken(const SystemSnapshot& snapshot);

}  // namespace snapshottext
}  // namespace eggplan
