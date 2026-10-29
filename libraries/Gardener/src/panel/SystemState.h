#pragma once

#include <stdint.h>

#include "core/Units.h"
#include "machine/VacuumSystem.h"

namespace gardener {

class Battery;
class DeviceSettings;

// What the panel can see of the machine, as one value: the run and the chamber, the pack, the two
// links, and the two preferences the device's document holds. Every field is a reading the panel
// draws with or answers from; none of them writes, which is the point of the type — a routine reads
// the machine through it and cannot start a pump or set a key.
//
// It is folded out of the owners when it is asked for and never stored, so there is no second copy
// to keep in step: a reader gets what is true at the read.
struct SystemState {
  RunState runState = RunState::Idle;
  bool running = false;
  float currentPressureKpa = 0.0f;
  float targetPressureKpa = 0.0f;
  bool canStart = false;
  uint8_t batteryPercent = 0;
  bool batteryPresent = false;
  bool batteryLow = false;
  bool serialConnected = false;
  bool bluetoothConnected = false;
  DisplayUnit unit = DisplayUnit::Kpa;
  float deviceTargetKpa = 0.0f;
};

// The one fold, so the field-to-source mapping exists once: the machine, the pack, the document and
// the two links the interface task last reported. Each part is read once, and a start's answer is
// the machine's own refusal rather than a second opinion on it.
SystemState readSystemState(const VacuumSystem& vacuum, const Battery& battery,
                            const DeviceSettings& settings, bool serialConnected,
                            bool bluetoothConnected);

}  // namespace gardener
