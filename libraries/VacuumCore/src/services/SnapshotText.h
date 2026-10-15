#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "core/Snapshot.h"

// Encodes the machine view for the wire: space separated key=value tokens with token
// values and a fixed key order, built into a caller supplied buffer.
//
// No JSON and no escaping: values are space free by construction, and the only consumers
// are a controller on the serial interface and a human with a terminal. One payload shape
// means a client writes one parser.

namespace eggplan {
namespace snapshottext {

// "kpa=41.23 vac=60.10 pct=59.3 target=40.0 floor=41.3 duty=72.0 state=HOLD remain=572 total=600
//  batt=78 sensor=1 fault=none up=123456"
size_t statusBody(const SystemSnapshot& snapshot, char* out, size_t size);

// "proto=3 fw=0.2.0 chip=ESP32-C3 heap=298412 batt_v=3.94 batt_pct=78 log=on"
size_t infoBody(const SystemSnapshot& snapshot, const LinkSettings& settings, char* out,
                size_t size);

const char* stateToken(RunState state);
const char* faultToken(const SystemSnapshot& snapshot);

}  // namespace snapshottext
}  // namespace eggplan
