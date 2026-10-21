#include "app/ble/PanelBleDomain.h"

#include "app/AppContext.h"
#include "core/Units.h"

namespace gardener {
namespace {

constexpr BleDomain::Spec kUnitSpec = {0x000A,
                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC, 1};

}  // namespace

PanelBleDomain::PanelBleDomain(AppContext* app) : app_(app) {}

uint8_t PanelBleDomain::specCount() const { return 1; }

BleDomain::Spec PanelBleDomain::specAt(uint8_t index) const { return kUnitSpec; }

void PanelBleDomain::bind(uint8_t index, NimBLECharacteristic* characteristic) {
  unit_ = characteristic;
}

void PanelBleDomain::describe(uint8_t index) {
  setByte(unit_, static_cast<uint8_t>(app_->settings().unit()));
}

void PanelBleDomain::write(uint8_t index, const NimBLEAttValue& value) {
  if (value.size() != 1) return;
  unitWrite_.value = value.data()[0];
  unitWrite_.has = true;
}

void PanelBleDomain::poll(uint32_t nowMs, BleLink& link, bool baseline) {
  (void)nowMs;
  (void)link;
  (void)baseline;
  if (unitWrite_.has) {
    unitWrite_.has = false;
    // Not notified: one central at a time means the client that wrote it
    // already knows. The read-back is the answer.
    if (app_->ownership().ownedBy(Link::Bluetooth) && unitWrite_.value < kDisplayUnitCount) {
      app_->settings().setUnit(static_cast<DisplayUnit>(unitWrite_.value));
    }
    describe(0);
  }
}

}  // namespace gardener
