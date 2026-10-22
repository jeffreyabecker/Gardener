#include "ble/BleLink.h"

#include <stdio.h>

#include "Ownership.h"

namespace gardener {
namespace {

// The service's base UUID: the last 16 bits select the characteristic, so the
// suffix in each domain's spec is the whole of a characteristic's name.
constexpr const char* kServiceUuid = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10";
constexpr const char* kUuidPrefix = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e";
constexpr const char* kDeviceName = "gardener";

String characteristicUuid(uint16_t suffix) {
  char text[37];
  snprintf(text, sizeof(text), "%s%04x", kUuidPrefix, suffix);
  return String(text);
}

}  // namespace

void BleLink::registerDomain(BleDomain* domain) {
  if (domain == nullptr || domainCount_ >= kMaxDomains) return;
  domains_[domainCount_++] = domain;
}

BleLink::Start BleLink::begin() {
  if (!app_->config().bluetooth) return Start::Disabled;
  if (!NimBLEDevice::init(kDeviceName)) return Start::Failed;

  // Just Works, bonded: the device pairs without a passkey and keeps the bond
  // in NVS. That is a guard against a casual neighbour at the bench rather
  // than an attacker with a radio, and it is the price of a pairing flow
  // someone will actually complete.
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) return Start::Failed;
  server_->setCallbacks(this);
  // Advertising is presence: it stops on connect and comes back when the
  // central leaves.
  server_->advertiseOnDisconnect(true);

  NimBLEService* service = server_->createService(kServiceUuid);
  if (service == nullptr) return Start::Failed;

  // Each registered domain contributes its characteristics in registration
  // order, and the link hands each one back so the domain can answer it.
  fieldCount_ = 0;
  for (uint8_t d = 0; d < domainCount_; ++d) {
    BleDomain* domain = domains_[d];
    for (uint8_t i = 0; i < domain->specCount(); ++i) {
      const BleDomain::Spec& spec = domain->specAt(i);
      NimBLECharacteristic* characteristic = service->createCharacteristic(
          characteristicUuid(spec.suffix).c_str(), spec.properties, spec.maxLength);
      if (characteristic == nullptr) return Start::Failed;
      characteristic->setCallbacks(this);
      domain->bind(i, characteristic);
      entries_[fieldCount_] = {domain, i};
      fields_[fieldCount_] = characteristic;
      ++fieldCount_;
    }
  }

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(service->getUUID());
  advertising->setName(kDeviceName);
  advertising->enableScanResponse(true);
  return advertising->start() ? Start::Advertising : Start::Failed;
}

bool BleLink::connected() const {
  return server_ != nullptr && server_->getConnectedCount() > 0;
}

void BleLink::poll(uint32_t nowMs) {
  if (server_ == nullptr) return;

  // A fresh subscription wants a baseline, and it is consumed by the first
  // pass after it.
  const bool baseline = baseline_;
  baseline_ = false;
  for (uint8_t d = 0; d < domainCount_; ++d) {
    domains_[d]->poll(nowMs, *this, baseline);
  }

  if (!linkLost_) return;
  linkLost_ = false;
  subscriptions_ = 0;
  // A central that leaves is removed from the stack; the control beat vents if
  // it was on top, and a reconnected central simply claims again.
  app_->ownership().drop(Link::Bluetooth);
}

void BleLink::notify(NimBLECharacteristic* characteristic) {
  if (characteristic == nullptr) return;
  if (!subscribed(characteristic)) return;
  characteristic->notify();
}

bool BleLink::subscribed(const NimBLECharacteristic* characteristic) const {
  const int index = find(characteristic);
  if (index < 0) return false;
  return (subscriptions_ & (1u << index)) != 0;
}

int BleLink::find(const NimBLECharacteristic* characteristic) const {
  for (uint8_t i = 0; i < fieldCount_; ++i) {
    if (fields_[i] == characteristic) return static_cast<int>(i);
  }
  return -1;
}

// --- NimBLE's callbacks, on the host task -----------------------------------

void BleLink::onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) {
  (void)server;
  (void)info;
  (void)reason;
  // Nothing is decided here: the loop applies the departure on its next pass,
  // so core 1 keeps being the only place that touches ownership.
  subscriptions_ = 0;
  linkLost_ = true;
}

void BleLink::onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  const int index = find(characteristic);
  if (index < 0) return;
  const Entry& entry = entries_[index];
  entry.domain->describe(entry.index);
}

// A write only lands in its slot. Lengths are checked by the owning domain,
// because they are the payload's shape, not a decision about the machine.
void BleLink::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  const int index = find(characteristic);
  if (index < 0) return;
  const Entry& entry = entries_[index];
  entry.domain->write(entry.index, characteristic->getValue());
}

void BleLink::onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& info,
                          uint16_t value) {
  (void)info;
  const int index = find(characteristic);
  if (index < 0) return;
  const uint16_t bit = static_cast<uint16_t>(1u << index);
  if (value == 0) {
    subscriptions_ &= static_cast<uint16_t>(~bit);
    return;
  }
  subscriptions_ |= bit;
  // The next pass pushes the current value of every on-change field, so a
  // subscriber starts from the machine's state rather than from the first
  // thing that moves.
  baseline_ = true;
}

}  // namespace gardener
