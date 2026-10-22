#pragma once

#include "Build.h"

#if GARDENER_BLUETOOTH

#include <NimBLEDevice.h>

namespace gardener {

class BleLink;

// One registered piece of the GATT surface. The link owns the transport —
// creating characteristics, routing callbacks, tracking subscriptions and
// sending notifications — and a domain owns the content: which
// characteristics it declares, what they read as, what a write means, and
// what to publish on the next pass. The surface is whatever the build
// registers, so the machine and the panel each bring their own fields.
class BleDomain {
 public:
  struct Spec {
    uint16_t suffix;
    uint32_t properties;
    uint16_t maxLength;
  };

  virtual ~BleDomain() = default;

  // The characteristics this domain declares, in the order the link creates
  // them. The index handed to the methods below is this list's index.
  virtual uint8_t specCount() const = 0;
  virtual Spec specAt(uint8_t index) const = 0;

  // The link hands back the characteristic it created for each spec. The
  // domain stores them and answers reads through them.
  virtual void bind(uint8_t index, NimBLECharacteristic* characteristic) = 0;

  // A read of this domain's characteristic `index`: set its value.
  virtual void describe(uint8_t index) = 0;

  // A write of this domain's characteristic `index`: validate the payload and
  // queue whatever it means; the loop applies it on the next pass.
  virtual void write(uint8_t index, const NimBLEAttValue& value) = 0;

  // Once per loop pass: apply the writes that landed, then set and notify
  // what moved. `link` is the notifier, so a change is reported the same way
  // a read answers it. `baseline` is true once after a new subscription, so
  // every on-change field starts the subscriber from the machine's state.
  virtual void poll(uint32_t nowMs, BleLink& link, bool baseline) = 0;
};

// A start that failed part-way leaves characteristics missing, and nothing has
// a connection to write to them, but the loop still walks this path once a
// pass.
inline void setByte(NimBLECharacteristic* characteristic, uint8_t value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(&value, sizeof(value));
}

// The part is little-endian and the contract is little-endian, so the float
// goes out as it lies.
inline void setFloat(NimBLECharacteristic* characteristic, float value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
