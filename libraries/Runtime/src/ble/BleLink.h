#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"

namespace gardener {
inline namespace core {
class Battery;
}
inline namespace machine {
class Machine;
}
inline namespace storage {
class DeviceSettings;
}

// The Bluetooth interface to the controller: the machine as a GATT table rather than as lines. This
// is deliberately a second contract and not the serial grammar over a pipe. The table is one
// service, and every characteristic it holds is this link's — one slot list, from the machine's
// state to the panel's two fields, created in the order the list names.
//
// A pairing is what lets a central write; there is no claim to make after it.
//
// NimBLE runs its host on core 0 and the loop on core 1. A status read answers from the machine as
// it is, and a write only lands in a slot here. poll() applies whatever is pending at the top of its
// pass, then notifies what changed, so a notified value is always one the machine actually has.
class BleLink : private NimBLEServerCallbacks, private NimBLECharacteristicCallbacks {
 public:
  // What begin() found. The radio is a bonus transport, so a link that does not come up is a line in
  // the boot log rather than a boot failure.
  enum class Start : uint8_t { Advertising, Failed };

  BleLink(CommandQueue& queue, Machine& machine, DeviceSettings& settings, Battery& battery)
      : outbox_(queue), machine_(machine), settings_(settings), battery_(battery) {}

  Start begin();
  void poll(uint32_t nowMs);

  // Whether a central is connected. Presence only: the panel's overlay shows this.
  bool connected() const;

 private:
  // The table, in creation order: the machine's fields, then the panel's. The index is a
  // characteristic's slot everywhere below, and one bit of `subscriptions_`.
  enum : uint8_t {
    State = 0,
    PressureKpa = 1,
    Target = 2,
    Fault = 3,
    Command = 4,
    Firmware = 5,
    Protocol = 6,
    Sensor = 7,
    Floor = 8,
    Caps = 9,
    HoldS = 10,
    MaxRunS = 11,
    WriteResult = 12,
    UptimeMs = 13,
    Unit = 14,
    Batt = 15,
    Count = 16,
  };

  struct Spec {
    uint16_t suffix;
    uint32_t properties;
    uint16_t maxLength;
  };

  static const Spec Specs[Count];

  // The link's queue, for the opcodes and setpoints the table applies.
  bool post(const MachineCommand& command) { return outbox_.post(command); }

  // --- the table's content -------------------------------------------------
  // A read of slot `index`: set its value.
  void describe(uint8_t index);
  // A write of slot `index`: validate the payload and queue whatever it means; the pass applies it.
  void write(uint8_t index, const NimBLEAttValue& value);
  // Once per pass, in this order: the writes that landed, then what has moved.
  void applyPending();
  void publish(uint32_t nowMs, bool baseline);
  // Runs one opcode, and answers the result byte the `command` field carries.
  uint8_t execute(uint8_t opcode);
  // A refused write is not silent: the field's suffix and the reason are notified, while the
  // read-back stays the authority on what the machine holds.
  void refuse(uint8_t index, uint8_t reason);
  void publishOnChange(uint8_t index, uint8_t value, uint8_t& last, bool force);
  void publishOnChange(uint8_t index, float value, float& last, bool force);

  // --- NimBLE's callbacks, on the host task --------------------------------
  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) override;
  void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override;
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override;
  void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& info,
                   uint16_t value) override;

  // The slot of a characteristic, or -1 when it is not ours.
  int find(const NimBLECharacteristic* characteristic) const;

  // The link's notifier: send this characteristic if a subscriber is listening.
  void notify(NimBLECharacteristic* characteristic);
  bool subscribed(const NimBLECharacteristic* characteristic) const;

  // A start that failed part-way leaves characteristics missing, and nothing has a connection to
  // write to them, but the loop still walks this path once a pass.
  static void setByte(NimBLECharacteristic* characteristic, uint8_t value);
  static void setFloat(NimBLECharacteristic* characteristic, float value);
  static void setUint16(NimBLECharacteristic* characteristic, uint16_t value);
  static void setUint32(NimBLECharacteristic* characteristic, uint32_t value);

  CommandOutbox outbox_;
  Machine& machine_;
  DeviceSettings& settings_;
  Battery& battery_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* characteristics_[Count] = {};

  // The write slots, one per writable characteristic. Written on core 0, read on core 1, each one
  // aligned value.
  struct PendingByte {
    volatile bool has = false;
    volatile uint8_t value = 0;
  };
  struct PendingFloat {
    volatile bool has = false;
    volatile float value = 0.0f;
  };
  struct PendingUint16 {
    volatile bool has = false;
    volatile uint16_t value = 0;
  };
  PendingFloat targetWrite_;
  PendingByte commandWrite_;
  PendingUint16 holdWrite_;
  PendingByte unitWrite_;

  // The hold a write asked for, in whole seconds; the next start grants it, and 0 asks for the
  // compiled maximum.
  uint16_t requestedHoldS_ = 0;

  // The last value sent for each on-change field, so a pass only notifies a
  // real change.
  uint8_t lastState_ = 0;
  float lastTarget_ = 0.0f;
  String lastFault_;
  uint8_t lastSensor_ = 0;
  float lastFloor_ = 0.0f;
  uint16_t lastHoldS_ = 0;
  uint32_t lastFastMs_ = 0;
  uint32_t lastBattMs_ = 0;

  // Written on core 0, read on core 1. Each is one aligned value, which is all this model needs.
  volatile uint16_t subscriptions_ = 0;
  // A central that has just subscribed gets the current value of every on-change field, so it starts
  // from the machine's state rather than from the first thing that moves.
  volatile bool baseline_ = false;
};

}  // namespace gardener
