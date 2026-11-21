#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "core/Logger.h"
#include "machine/SystemControl.h"

namespace gardener::runtime {

// The Bluetooth LE link: one GATT service over the same SystemControl the serial wire and the page
// ask, so it is another way of asking rather than another way of deciding. The service carries the
// reading as a status packet, a command write, and the outcome of the last command as a reply; the
// packets themselves are GattValues.h, which knows nothing of the board.
//
// One writer per characteristic: the write callback sets the reply and the beat sets the status, so
// the two tasks never touch the same value.
class BleLink {
 public:
  BleLink(SystemControl& control, Logger& logger);

  // Starts the stack, the service and the advertisement. False when the stack would not come up, in
  // which case the rest of the device runs without it.
  bool begin();
  // The status beat: refreshes what a read answers, and notifies on the period and at once when the
  // state or the target moved.
  void poll(uint32_t nowMs);

 private:
  class ServerCallbacks;
  class CommandCallbacks;

  void connected();
  void disconnected();
  void commanded(NimBLECharacteristic* characteristic);

  SystemControl& control_;
  Logger& logger_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* status_ = nullptr;
  NimBLECharacteristic* reply_ = nullptr;
  uint32_t lastStatusMs_ = 0;
  uint8_t lastState_ = 0xFF;
  float lastTargetKpa_ = 0.0f;
};

}  // namespace gardener::runtime
