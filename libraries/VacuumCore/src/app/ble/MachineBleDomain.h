#pragma once

#include "app/ble/BleDomain.h"

namespace gardener {

class MachineContext;

// The machine's slice of the GATT table: state, kpa, target, duty, fault,
// command, fw, ble_proto, control, sensor and floor. Everything a controller
// commands and reads about the machine itself, before the panel adds anything
// of its own.
class MachineBleDomain : public BleDomain {
 public:
  explicit MachineBleDomain(MachineContext* app);

  uint8_t specCount() const override;
  Spec specAt(uint8_t index) const override;
  void bind(uint8_t index, NimBLECharacteristic* characteristic) override;
  void describe(uint8_t index) override;
  void write(uint8_t index, const NimBLEAttValue& value) override;
  void poll(uint32_t nowMs, BleLink& link, bool baseline) override;

 private:
  enum : uint8_t {
    State,
    Kpa,
    Target,
    Duty,
    Fault,
    Command,
    Firmware,
    Protocol,
    Control,
    Sensor,
    Floor,
    Caps,
    Count,
  };

  static const Spec kSpecs[Count];

  bool owned() const;
  uint8_t execute(uint8_t opcode);
  void applyPending(BleLink& link);
  void publish(uint32_t nowMs, BleLink& link, bool baseline);
  void publishOnChange(uint8_t index, uint8_t value, uint8_t& last, bool force, BleLink& link);
  void publishOnChange(uint8_t index, float value, float& last, bool force, BleLink& link);

  MachineContext* app_;
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
  PendingFloat target_;
  PendingByte command_;
  PendingByte control_;

  // The last value sent for each on-change field, so a pass only notifies a
  // real change.
  uint8_t lastState_ = 0;
  float lastTarget_ = 0.0f;
  String lastFault_;
  uint8_t lastSensor_ = 0;
  float lastFloor_ = 0.0f;
  uint8_t lastControl_ = 0;
  uint32_t lastFastMs_ = 0;
};

}  // namespace gardener
