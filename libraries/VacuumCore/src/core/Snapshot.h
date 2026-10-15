#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/Config.h"

// Plain data shared between layers. Everything here is a value or a pointer to
// storage owned by App, so a consumer never needs a pointer back into a module.
// This is what lets ui/ and hosts/ read machine state without depending on the
// modules that produce it.

namespace eggplan {

// Checksum of a stored program. Opaque on purpose: crc32 is an internal detail of
// the program store, not something a caller should compute.
using ProgramChecksum = uint32_t;

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

struct ProgramInfo {
  char name[32] = {0};
  uint32_t size = 0;
  ProgramChecksum checksum = 0;
  bool autostart = false;
};

struct DeviceInfo {
  char chip[16] = {0};
  char sdk[24] = {0};
  uint32_t freeHeap = 0;
};

// The script drawing surface. Owned by App, written by hosts/script/ScriptApi, drawn
// by ui/Renderer, so the script host never touches the display driver.
struct ScriptOverlay {
  static constexpr uint8_t kLines = 7;
  static constexpr size_t kLineLength = 24;

  char lines[kLines][kLineLength] = {{0}};
  float bar = 0.0f;
  bool active = false;

  void clear() {
    for (uint8_t i = 0; i < kLines; i++) lines[i][0] = '\0';
    bar = 0.0f;
    active = true;
  }

  bool setLine(int index, const char* text) {
    if (index < 0 || index >= (int)kLines) return false;
    size_t i = 0;
    if (text) {
      for (; i + 1 < kLineLength && text[i] != '\0'; i++) lines[index][i] = text[i];
    }
    lines[index][i] = '\0';
    active = true;
    return true;
  }
};

// Requests from a host into the local UI: plain data again, so 'buttons' does not
// mean the command grammar knows what a page is.
struct UiSettings {
  int8_t pageStep = 0;
  char toast[26] = {0};
  uint32_t toastUntilMs = 0;
};

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
  // programs
  const ProgramInfo* programs = nullptr;
  size_t programCount = 0;
  uint32_t storeUsedKb = 0;
  uint32_t storeTotalKb = 0;
  const char* autostart = "";
  // script
  bool scriptRunning = false;
  const char* program = "";
  const char* runtime = "";
  const ScriptOverlay* overlay = nullptr;
  // device and link
  const DeviceInfo* device = nullptr;
  bool bleConnected = false;
  uint32_t uptimeMs = 0;
};

}  // namespace eggplan
