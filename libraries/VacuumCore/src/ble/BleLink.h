#pragma once

#include <NimBLEDevice.h>
#include <vector>

#include "Machine.h"
#include "Ownership.h"
#include "ble/BleDomain.h"

namespace gardener {

// The Bluetooth interface to the controller: the machine as a GATT table
// rather than as lines. This is deliberately a second contract and not the
// serial grammar over a pipe, and the table is composed the same way the
// serial link's domains are: the build registers domains, each contributing
// its characteristics, so a panel build adds its own and a machine-alone
// build does not.
//
// Connecting is read-only. Ownership is the explicit write to `control`, and
// leaving while owning pops the link off the stack, which vents the machine on
// the next control beat.
//
// NimBLE runs its host on core 0 and the loop on core 1. A status read answers
// from the machine as it is, and a write only lands in a slot here. poll()
// applies whatever is pending at the top of its pass, then notifies what
// changed, so a notified value is always one the machine actually has.
class BleLink : public Link,
                private NimBLEServerCallbacks,
                private NimBLECharacteristicCallbacks {
 public:
  static constexpr uint8_t Code = 2;

  // What begin() found. The radio is a bonus transport, so a link that does
  // not come up is a line in the boot log rather than a boot failure, and a
  // device with the preference off stays silent.
  enum class Start : uint8_t { Disabled, Advertising, Failed };

  explicit BleLink(Machine* context) : app_(context) {
    domains_.reserve(MaxDomains);
    fields_.reserve(MaxFields);
  }

  const char* name() const override { return "bluetooth"; }
  uint8_t code() const override { return Code; }

  // A claim cancels a departure the stack has not applied yet: the central is back and driving.
  void claimed() override { linkLost_ = false; }

  // The surface is whatever the build registers: the machine brings its
  // fields, and a panel build brings its own.
  void registerDomain(BleDomain* domain);

  Start begin();
  void poll(uint32_t nowMs);

  // Whether a central is connected. Presence, not ownership: the overlay shows
  // this and the page follows the ownership record.
  bool connected() const;

  // The domain's notifier: send this characteristic if a subscriber is
  // listening. A domain calls it from its poll after setting the value.
  void notify(NimBLECharacteristic* characteristic);
  bool subscribed(const NimBLECharacteristic* characteristic) const;

 private:
  // The machine's domains and a panel's, and the characteristics they declare. Both are a build
  // fact, so the tables are reserved once and do not grow while the device runs.
  static constexpr uint8_t MaxDomains = 4;
  static constexpr uint8_t MaxFields = 16;

  // One registered characteristic: the domain that declares it, its index in that domain's specs,
  // and the characteristic the link created for it. One list of three, because a read or a write
  // routes on all three and two parallel arrays are two things to keep in step.
  struct Field {
    BleDomain* domain;
    uint8_t index;  // this domain's spec index
    NimBLECharacteristic* characteristic;
  };

  // --- NimBLE's callbacks, on the host task ---------------------------------
  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) override;
  void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override;
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override;
  void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& info,
                   uint16_t value) override;

  // The global index of a characteristic, or -1 when it is not ours.
  int find(const NimBLECharacteristic* characteristic) const;

  Machine* app_;
  NimBLEServer* server_ = nullptr;
  std::vector<BleDomain*> domains_;
  std::vector<Field> fields_;

  // Written on core 0, read on core 1. Each is one aligned value, which is
  // all this model needs.
  volatile bool linkLost_ = false;
  volatile uint16_t subscriptions_ = 0;
  // A central that has just subscribed gets the current value of every
  // on-change field, so it starts from the machine's state rather than from
  // the first thing that moves.
  volatile bool baseline_ = false;
};

}  // namespace gardener
