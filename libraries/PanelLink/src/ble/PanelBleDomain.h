#pragma once

#include "ble/BleDomain.h"

namespace gardener {

class Panel;

// The panel's slice of the GATT table: `unit` — what the panel prints — and `batt` — the gauge.
// Both have no meaning on a device without a panel.
class PanelBleDomain : public BleDomain {
 public:
  explicit PanelBleDomain(Panel* app);

  uint8_t specCount() const override;
  Spec specAt(uint8_t index) const override;
  void bind(uint8_t index, NimBLECharacteristic* characteristic) override;
  void describe(uint8_t index) override;
  void write(uint8_t index, const NimBLEAttValue& value) override;
  void poll(uint32_t nowMs, BleLink& link, bool baseline) override;

 private:
  enum : uint8_t { Unit, Batt, Count };

  static const Spec kSpecs[Count];

  Panel* app_;
  NimBLECharacteristic* characteristics_[Count] = {};
  struct PendingByte {
    volatile bool has = false;
    volatile uint8_t value = 0;
  };
  PendingByte unitWrite_;
  uint32_t lastBattMs_ = 0;
};

}  // namespace gardener
