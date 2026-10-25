#pragma once

#include "Build.h"

#if GARDENER_BLUETOOTH

#include "ble/BleDomain.h"

namespace gardener {

class Machine;

// The machine's slice of the GATT table: state, pressure_kpa, target, fault,
// command, fw, ble_proto, sensor, floor, caps, hold_s, max_run_s, write_result and uptime_ms.
// Everything a controller commands and reads about the machine itself, before the panel adds
// anything of its own. `floor` is here because it bounds a target, not because it is a setting: the
// rig's numbers are build symbols and no link carries them.
class MachineBleDomain : public BleDomain {
 public:
  explicit MachineBleDomain(Machine* app);

  uint8_t specCount() const override;
  Spec specAt(uint8_t index) const override;
  void bind(uint8_t index, NimBLECharacteristic* characteristic) override;
  void describe(uint8_t index) override;
  void write(uint8_t index, const NimBLEAttValue& value) override;
  void poll(uint32_t nowMs, BleLink& link, bool baseline) override;

 private:
  // The slot of each characteristic this domain declares, numbered in the
  // order Specs lists them. The values are written out so a slot keeps its
  // number when its neighbours move.
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
    Count = 14,
  };

  static const Spec Specs[Count];

  uint8_t execute(BleLink& link, uint8_t opcode);
  void applyPending(BleLink& link);
  // A refused write is not silent: the field's suffix and the reason are notified, while the
  // read-back stays the authority on what the machine holds.
  void refuse(BleLink& link, uint8_t index, uint8_t reason);
  void publish(uint32_t nowMs, BleLink& link, bool baseline);
  void publishOnChange(uint8_t index, uint8_t value, uint8_t& last, bool force, BleLink& link);
  void publishOnChange(uint8_t index, float value, float& last, bool force, BleLink& link);

  Machine* app_;
  NimBLECharacteristic* characteristics_[Count] = {};

  // The write slots, one per writable characteristic. Written on core 0, read
  // on core 1, each one aligned value.
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
  PendingFloat target_;
  PendingByte command_;
  PendingUint16 hold_;

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
};

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
