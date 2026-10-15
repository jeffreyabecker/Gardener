#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/Config.h"

// Plain data shared between layers. The device is a serial peripheral, so this is the
// machine's read-only view: what a controller, the display or the log can see without
// holding a pointer into a module.

namespace eggplan {

// What the device is doing, which is also the state a controller watches. There is no DONE
// (after venting it is IDLE, and the countdown reaching zero is what said it finished)
// and no MANUAL (raw duty is not part of the interface, Q15).
enum class RunState : uint8_t { Idle, Pulling, Holding, Venting, Fault };

inline const char* runStateName(RunState state) {
  switch (state) {
    case RunState::Idle: return "IDLE";
    case RunState::Pulling: return "PULL";
    case RunState::Holding: return "HOLD";
    case RunState::Venting: return "VENT";
    case RunState::Fault: return "FAULT";
    default: return "?";
  }
}

struct DeviceInfo {
  char chip[16] = {0};
  char sdk[24] = {0};
  uint32_t freeHeap = 0;
};

// Written by the command grammar, read by the transport and by the payload renderer: the
// one interface setting a controller can change. Plain data, so the transport never holds
// a pointer into the grammar that answers it.
struct LinkSettings {
  bool logEnabled = true;
};

struct SystemSnapshot {
  // pressure
  float kpa = 0.0f;
  float vacuumKpa = 0.0f;
  float vacuumPercent = 0.0f;
  float ambientKpa = 0.0f;
  bool sensorOk = false;
  bool readingValid = false;
  // pump
  float duty = 0.0f;
  float rpm = 0.0f;
  bool tachEnabled = false;
  bool pumpTripped = false;
  const char* faultCode = "";
  const char* faultText = "";
  // run
  RunState state = RunState::Idle;
  bool targetActive = false;
  float targetKpa = 0.0f;
  float floorKpa = 0.0f;
  uint32_t remainSeconds = 0;  // totalSeconds == 0 means "no timer"
  uint32_t totalSeconds = 0;
  // battery: a readout only, nothing acts on it (Q11)
  bool batteryPresent = false;
  bool batteryLow = false;
  float batteryVolts = 0.0f;
  uint8_t batteryPercent = 0;
  // device
  const DeviceInfo* device = nullptr;
  uint32_t uptimeMs = 0;
};

}  // namespace eggplan
