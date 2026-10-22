#include "ble/PanelBleDomain.h"

#include "PanelContext.h"
#include "ble/BleLink.h"
#include "core/Units.h"

namespace gardener {
namespace {

constexpr uint32_t kBattRateMs = 10000;

}  // namespace

const BleDomain::Spec PanelBleDomain::kSpecs[] = {
    {0x000A, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC, 1},  // unit
    {0x0006, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},    // batt
};

PanelBleDomain::PanelBleDomain(PanelContext* app) : app_(app) {}

uint8_t PanelBleDomain::specCount() const { return Count; }

BleDomain::Spec PanelBleDomain::specAt(uint8_t index) const { return kSpecs[index]; }

void PanelBleDomain::bind(uint8_t index, NimBLECharacteristic* characteristic) {
  characteristics_[index] = characteristic;
}

void PanelBleDomain::describe(uint8_t index) {
  switch (index) {
    case Unit:
      setByte(characteristics_[Unit], static_cast<uint8_t>(app_->settings().unit()));
      break;
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
    if (app_->ownership().ownedBy(Link::Bluetooth) &&
        unitWrite_.value < kDisplayUnitCount) {
      app_->settings().setUnit(static_cast<DisplayUnit>(unitWrite_.value));
    }
    describe(Unit);
  }

  if (nowMs - lastBattMs_ >= kBattRateMs) {
    lastBattMs_ = nowMs;
    describe(Batt);
    link.notify(characteristics_[Batt]);
  }
}

}  // namespace gardener
