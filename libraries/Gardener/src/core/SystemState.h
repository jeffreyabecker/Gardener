#pragma once

#include <stdint.h>

#include "core/RunState.h"
#include "core/Units.h"

namespace gardener {
inline namespace core {

// What the device is doing, as the panel draws it and a routine reads it: the run and the chamber,
// the pack, the two links, and the two preferences the device's document holds.
//
// It is one object, published rather than folded, and it is a dependency like any other: whoever
// composes the device owns it, hands a reference to every task that publishes a slice, and the panel
// hands the same reference to whatever draws. Each slice has one writer, on the task that owns the
// fact, and a reader copies the whole thing so that one draw shows one instant. A read is therefore
// up to the writer's period old — 50 ms for the machine's slice, 10 ms for the pack, 5 ms for the
// links — and a slice whose writer stops running stops moving. Both are deliberate: the alternative
// was a fold per reader, and nothing the panel draws needs better than a beat.
//
// The fields are volatile because each is written on one task and read on another, which is what the
// two connection flags this replaces already did.
struct SystemState {
  // The control tick's slice, published at the end of its beat. `canStart` is that beat's answer
  // rather than the instant's, so a start a page asks for in between can be refused by the machine.
  volatile RunState runState = RunState::Idle;
  volatile bool running = false;
  volatile float currentPressureKpa = 0.0f;
  volatile float targetPressureKpa = 0.0f;
  volatile bool canStart = false;

  // The parts tick's slice, published at the end of its beat. No gauge behind the reading means no
  // percentage rather than whatever the part last held.
  volatile uint8_t batteryPercent = 0;
  volatile bool batteryPresent = false;
  volatile bool batteryLow = false;

  // The interface loop's slice, published as it polls.
  volatile bool serialConnected = false;
  volatile bool bluetoothConnected = false;

  // DeviceSettings' slice, published when the document is read or written.
  volatile DisplayUnit unit = DisplayUnit::Kpa;
  volatile float deviceTargetKpa = 0.0f;
};

}  // namespace core
}  // namespace gardener
