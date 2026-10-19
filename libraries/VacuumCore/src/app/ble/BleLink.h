#pragma once

#include <NimBLEDevice.h>

#include "app/AppContext.h"

namespace gardener {

// The Bluetooth interface to the controller: the machine as a GATT table rather than as lines.
//
// This is deliberately a second contract and not the serial grammar over a pipe. A phone binds to
// characteristics, reads typed values and subscribes to the ones it shows, so the machine is
// exposed as typed fields with directions. The table is docs/plans/ble-control.md §4, and every
// value is read from the same AppContext the serial link reads, so there is still one machine and
// no second copy of its state.
//
// Connecting is read-only. Ownership is the explicit write to `control`, and leaving while owning
// changes nothing: the record stays held (and stale) until the local reset gesture clears it.
//
// NimBLE runs its host on core 0 and the loop on core 1. A status read answers from the machine as
// it is, and a write only lands in a slot here. poll() applies whatever is pending at the top of
// its pass, then notifies what changed, so a notified value is always one the machine actually has.
// There is no lock, no queue and no second write promise: two writes to one characteristic before
// the loop runs leave the last one standing.
class BleLink : private NimBLEServerCallbacks, private NimBLECharacteristicCallbacks {
 public:
  // What begin() found. The radio is a bonus transport, so a link that does not come up is a line in
  // the boot log rather than a boot failure, and a device with the preference off stays silent.
  enum class Start : uint8_t { Disabled, Advertising, Failed };

  explicit BleLink(AppContext* context) : app_(context) {}

  Start begin();
  void poll(uint32_t nowMs);

  // Whether a central is connected. Presence, not ownership: the overlay shows this and the page
  // follows the ownership record.
  bool connected() const;

 private:
  // One per characteristic, in the table's order. The value is also the characteristic's suffix in
  // the service's UUID.
  enum class Field : uint8_t {
    State,
    Kpa,
    Target,
    Duty,
    Fault,
    Batt,
    Command,
    Firmware,
    Protocol,
    Unit,
    Control,
    Sensor,
    Floor,
    Count,
  };

  static constexpr uint8_t kFieldCount = static_cast<uint8_t>(Field::Count);

  // --- NimBLE's callbacks, on the host task ---------------------------------
  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) override;
  void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override;
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override;
  void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& info,
                   uint16_t value) override;

  // --- the loop's side ------------------------------------------------------
  // Applies the writes the callbacks landed: ownership, the target, the unit and an opcode.
  void applyPending();
  // Sends whatever is due: the on-change fields that moved, then the rates.
  void publish(uint32_t nowMs);
  // Answers a read from the machine as it is.
  void describe(Field field);
  void notify(Field field);
  // Sends a field whose value only matters when it moves, and remembers what was sent.
  void publishOnChange(Field field, uint8_t value, uint8_t& last, bool force);
  void publishOnChange(Field field, float value, float& last, bool force);
  // Runs an opcode and reports the result byte the contract lists.
  uint8_t execute(uint8_t opcode);

  NimBLECharacteristic* characteristic(Field field) const {
    return fields_[static_cast<uint8_t>(field)];
  }
  bool find(const NimBLECharacteristic* characteristic, Field& field) const;
  bool subscribed(Field field) const;
  bool owned() const;

  AppContext* app_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* fields_[kFieldCount] = {};

  // A write the callback landed and the loop has not applied yet. One slot per writable
  // characteristic: last write wins, and the value is applied on the next pass.
  struct PendingByte {
    volatile bool has = false;
    volatile uint8_t value = 0;
  };
  struct PendingFloat {
    volatile bool has = false;
    volatile float value = 0.0f;
  };
  PendingFloat target_;
  PendingByte unit_;
  PendingByte command_;
  PendingByte control_;

  // Written on core 0, read on core 1. Each is one aligned value, which is all this model needs.
  volatile bool linkLost_ = false;
  volatile uint16_t subscriptions_ = 0;
  // A central that has just subscribed gets the current value of every on-change field, so it
  // starts from the machine's state rather than from the first thing that moves.
  volatile bool baseline_ = false;

  // The last value sent for each on-change characteristic, so a pass only notifies a real change.
  uint8_t lastState_ = 0;
  float lastTarget_ = 0.0f;
  String lastFault_;
  uint8_t lastSensor_ = 0;
  float lastFloor_ = 0.0f;
  uint8_t lastControl_ = 0;

  uint32_t lastFastMs_ = 0;
  uint32_t lastBattMs_ = 0;
};

}  // namespace gardener
