#pragma once

#include "app/ble/BleDomain.h"

namespace gardener {

class AppContext;

// The panel's slice of the GATT table: the `unit` characteristic, which says
// what the panel prints and has no meaning on a device without one.
class PanelBleDomain : public BleDomain {
 public:
  explicit PanelBleDomain(AppContext* app);

  uint8_t specCount() const override;
  Spec specAt(uint8_t index) const override;
  void bind(uint8_t index, NimBLECharacteristic* characteristic) override;
  void describe(uint8_t index) override;
  void write(uint8_t index, const NimBLEAttValue& value) override;
  void poll(uint32_t nowMs, BleLink& link, bool baseline) override;

 private:
  AppContext* app_;
  NimBLECharacteristic* unit_ = nullptr;
  struct PendingByte {
    volatile bool has = false;
    volatile uint8_t value = 0;
  };
  PendingByte unitWrite_;
};

}  // namespace gardener
