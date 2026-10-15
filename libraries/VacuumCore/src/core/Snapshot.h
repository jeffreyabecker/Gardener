#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/Config.h"

// Plain data shared between layers. The device is a BLE peripheral, so this is the
// machine's read-only view: what a host, the display or the log can see without holding
// a pointer into a module.

namespace eggplan {

enum class ControlState : uint8_t { Idle, Pumping, Holding, Manual, Fault };

inline const char* controlStateName(ControlState state) {
  switch (state) {
    case ControlState::Idle: return "IDLE";
    case ControlState::Pumping: return "PUMP";
    case ControlState::Holding: return "HOLD";
    case ControlState::Manual: return "MAN";
    case ControlState::Fault: return "FAULT";
    default: return "?";
  }
}

struct DeviceInfo {
  char chip[16] = {0};
  char sdk[24] = {0};
  uint32_t freeHeap = 0;
};

// Written by a host, read by the transport, so a command can change the telemetry rate
// without holding a pointer into the service that sends it.
struct TelemetrySettings {
  bool enabled = false;
  uint32_t periodMs = cfg::TELEMETRY_DEFAULT_MS;
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
  // control
  ControlState state = ControlState::Idle;
  bool targetActive = false;
  float targetKpa = 0.0f;
  float floorKpa = 0.0f;
  // device and link
  const DeviceInfo* device = nullptr;
  bool bleConnected = false;
  uint32_t uptimeMs = 0;
};

}  // namespace eggplan
