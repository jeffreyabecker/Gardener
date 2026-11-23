#pragma once

#include <Arduino.h>

#include "core/Logger.h"
#include "machine/SystemControl.h"

// Whether this build has the Bluetooth stack: the c3-wifi-ble profile names NimBLE-Arduino and the
// c3-wifi profile does not. It is asked of the headers rather than of a define, so a profile alone
// decides it, and without the stack the link below is a stub that leaves the composition root the
// same.
#if __has_include(<NimBLEDevice.h>)
#include <NimBLEDevice.h>
#define GARDENER_HAS_BLE 1
#else
#define GARDENER_HAS_BLE 0
#endif

namespace gardener::runtime {

#if GARDENER_HAS_BLE

// The Bluetooth LE link: one GATT service over the same SystemControl the page asks, so it is
// another way of asking rather than another way of deciding. The service carries the reading as a
// status packet, a command write, and the outcome of the last command as a reply; the packets
// themselves are GattValues.h, which knows nothing of the board.
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

#else

// The c3-wifi build's link: present so nothing around it needs a guard, and silent.
class BleLink {
 public:
  BleLink(SystemControl&, Logger&) {}
  bool begin() { return false; }
  void poll(uint32_t) {}
};

#endif

}  // namespace gardener::runtime
