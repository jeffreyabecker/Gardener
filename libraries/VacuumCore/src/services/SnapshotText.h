#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "core/Snapshot.h"

// Encodes the machine view for the wire: space separated key=value tokens with token
// values and a fixed key order, built into a caller supplied buffer.
//
// No JSON anywhere: it would need escaping and allocation per telemetry
// notification, and the only consumers are our own host tool and a human with a BLE
// terminal. See docs/ble-protocol.md.

namespace eggplan {
namespace snapshottext {

// "kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 floor=41.3 fault=- up=123456"
size_t statusBody(const SystemSnapshot& snapshot, char* out, size_t size);

// "fw=0.1.0 chip=ESP32-C3 runtime=none progs=2 store_kb=1440 sensor=1 tach=0 heap=298412"
size_t infoBody(const SystemSnapshot& snapshot, char* out, size_t size);

// "p hold_vacuum 137 0x1A2B3C4D auto\np pulse 331 0xDEADBEEF -"
String programRows(const SystemSnapshot& snapshot);

const char* stateToken(ControlState state);
const char* faultToken(const SystemSnapshot& snapshot);

}  // namespace snapshottext
}  // namespace eggplan
