#include "Build.h"

#if GARDENER_PANEL

#include "ble/PanelDomains.h"

#if GARDENER_BLUETOOTH

#include "ble/BleDomain.h"
#include "ble/BleLink.h"
#include "panel/Panel.h"
#include "core/Units.h"
#include "storage/DeviceConfig.h"

namespace gardener {

// The panel's slice of the GATT table, private to the factory at the bottom of this file.
class PanelBleDomain : public BleDomain {
 public:
  explicit PanelBleDomain(panel::Panel& panel) : app_(&panel) {}

  uint8_t specCount() const override;
  Spec specAt(uint8_t index) const override;
  void bind(uint8_t index, NimBLECharacteristic* characteristic) override;
  void describe(uint8_t index) override;
  void write(uint8_t index, const NimBLEAttValue& value) override;
  void poll(uint32_t nowMs, BleLink& link, bool baseline) override;

 private:
  enum : uint8_t { Unit, Batt, Count };

  static const Spec Specs[Count];

  panel::Panel* app_;
  NimBLECharacteristic* characteristics_[Count] = {};
  struct PendingByte {
    volatile bool has = false;
    volatile uint8_t value = 0;
  };
  PendingByte unitWrite_;
  uint32_t lastBattMs_ = 0;
};

namespace {

constexpr uint32_t BattRateMs = 10000;

}  // namespace

const BleDomain::Spec PanelBleDomain::Specs[] = {
    {0x000A, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC, 1},  // unit
    {0x0006, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},    // batt
};

uint8_t PanelBleDomain::specCount() const { return Count; }

BleDomain::Spec PanelBleDomain::specAt(uint8_t index) const { return Specs[index]; }

void PanelBleDomain::bind(uint8_t index, NimBLECharacteristic* characteristic) {
  characteristics_[index] = characteristic;
}

void PanelBleDomain::describe(uint8_t index) {
  switch (index) {
    case Unit: {
      DisplayUnit unit = DisplayUnit::Kpa;
      parseDisplayUnit(
          app_->config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);
      setByte(characteristics_[Unit], static_cast<uint8_t>(unit));
      break;
    }
    case Batt:
      setByte(characteristics_[Batt],
              app_->battery().present() ? app_->battery().percent() : 0xFF);
      break;
    case Count:
      break;
  }
}

void PanelBleDomain::write(uint8_t index, const NimBLEAttValue& value) {
  if (index != Unit) return;
  if (value.size() != 1) return;
  unitWrite_.value = value.data()[0];
  unitWrite_.has = true;
}

void PanelBleDomain::poll(uint32_t nowMs, BleLink& link, bool baseline) {
  (void)baseline;
  if (unitWrite_.has) {
    unitWrite_.has = false;
    // Not notified: one central at a time means the client that wrote it
    // already knows. The read-back is the answer.
    if (unitWrite_.value < DisplayUnitCount) {
      const DisplayUnit unit =
          static_cast<DisplayUnit>(unitWrite_.value);
      app_->config().set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
      app_->config().save();
    }
    describe(Unit);
  }

  if (nowMs - lastBattMs_ >= BattRateMs) {
    lastBattMs_ = nowMs;
    describe(Batt);
    link.notify(characteristics_[Batt]);
  }
}

BleDomain* createPanelBleDomain(panel::Panel& panel) { return new PanelBleDomain(panel); }

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
#endif
