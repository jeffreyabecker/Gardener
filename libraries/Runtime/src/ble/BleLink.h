#pragma once

#include "Build.h"

#if GARDENER_BLUETOOTH

#include <NimBLEDevice.h>
#include <vector>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "ble/BleDomain.h"

namespace gardener {

// The Bluetooth interface to the controller: the machine as a GATT table
// rather than as lines. This is deliberately a second contract and not the
// serial grammar over a pipe, and the table is composed the same way the
// serial link's domains are: the build registers domains, each contributing
// its characteristics, so a panel build adds its own and a machine-alone
// build does not.
//
// A pairing is what lets a central write; there is no claim to make after it.
//
// NimBLE runs its host on core 0 and the loop on core 1. A status read answers
// from the machine as it is, and a write only lands in a slot here. poll()
// applies whatever is pending at the top of its pass, then notifies what
// changed, so a notified value is always one the machine actually has.
class BleLink : private NimBLEServerCallbacks, private NimBLECharacteristicCallbacks {
 public:
  // What begin() found. The radio is a bonus transport, so a link that does
  // not come up is a line in the boot log rather than a boot failure.
  enum class Start : uint8_t { Advertising, Failed };

  explicit BleLink(CommandQueue& queue) : outbox_(queue) {
    domains_.reserve(MaxDomains);
    fields_.reserve(MaxFields);
  }

  // The link's queue, shared by the domains the build registers.
  bool post(const MachineCommand& command) { return outbox_.post(command); }
  void flushCommands() { outbox_.flush(); }

  // The surface is whatever the build registers: the machine brings its
  // fields, and a panel build brings its own.
  void registerDomain(BleDomain* domain);

  Start begin();
  void poll(uint32_t nowMs);

  // Whether a central is connected. Presence only: the panel's overlay shows
  // this.
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

  CommandOutbox outbox_;
  NimBLEServer* server_ = nullptr;
  std::vector<BleDomain*> domains_;
  std::vector<Field> fields_;

  // Written on core 0, read on core 1. Each is one aligned value, which is
  // all this model needs.
  volatile uint16_t subscriptions_ = 0;
  // A central that has just subscribed gets the current value of every
  // on-change field, so it starts from the machine's state rather than from
  // the first thing that moves.
  volatile bool baseline_ = false;
};

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
