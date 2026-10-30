#include "ble/BleLink.h"

#include <stdio.h>

namespace gardener {
namespace {

// The service's base UUID: the last 16 bits select the characteristic, so the
// suffix in each domain's spec is the whole of a characteristic's name.
constexpr const char* ServiceUuid = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10";
constexpr const char* UuidPrefix = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e";
constexpr const char* DeviceName = "gardener";

String characteristicUuid(uint16_t suffix) {
  char text[37];
  snprintf(text, sizeof(text), "%s%04x", UuidPrefix, suffix);
  return String(text);
}

}  // namespace

void BleLink::registerDomain(BleDomain* domain) {
  if (domain == nullptr) return;
  domains_.push_back(domain);
}

BleLink::Start BleLink::begin() {
  if (!NimBLEDevice::init(DeviceName)) return Start::Failed;

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

  NimBLEService* service = server_->createService(ServiceUuid);
  if (service == nullptr) return Start::Failed;

  // Each registered domain contributes its characteristics in registration
  // order, and the link hands each one back so the domain can answer it.
  fields_.clear();
  for (BleDomain* domain : domains_) {
    for (uint8_t i = 0; i < domain->specCount(); ++i) {
      const BleDomain::Spec& spec = domain->specAt(i);
      NimBLECharacteristic* characteristic = service->createCharacteristic(
          characteristicUuid(spec.suffix).c_str(), spec.properties, spec.maxLength);
      if (characteristic == nullptr) return Start::Failed;
      characteristic->setCallbacks(this);
      domain->bind(i, characteristic);
      fields_.push_back({domain, i, characteristic});
    }
  }

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(service->getUUID());
  advertising->setName(DeviceName);
  advertising->enableScanResponse(true);
  return advertising->start() ? Start::Advertising : Start::Failed;
}

bool BleLink::connected() const {
  return server_ != nullptr && server_->getConnectedCount() > 0;
}

void BleLink::poll(uint32_t nowMs) {
  if (server_ == nullptr) return;

  // A command the control task's queue could not take on an earlier pass is retried first, so a
  // momentarily full queue costs this link a beat rather than the command.
  flushCommands();

  // A fresh subscription wants a baseline, and it is consumed by the first
  // pass after it.
  const bool baseline = baseline_;
  baseline_ = false;
  for (BleDomain* domain : domains_) {
    domain->poll(nowMs, *this, baseline);
  }
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
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i].characteristic == characteristic) return static_cast<int>(i);
  }
  return -1;
}

// --- NimBLE's callbacks, on the host task -----------------------------------

void BleLink::onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) {
  (void)server;
  (void)info;
  (void)reason;
  // A departure is presence only: the subscriptions are dropped and a reconnect starts
  // from a fresh baseline. Nothing about the machine changes.
  subscriptions_ = 0;
  baseline_ = false;
}

void BleLink::onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  const int index = find(characteristic);
  if (index < 0) return;
  const Field& field = fields_[index];
  field.domain->describe(field.index);
}

// A write only lands in its slot. Lengths are checked by the owning domain,
// because they are the payload's shape, not a decision about the machine.
void BleLink::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  const int index = find(characteristic);
  if (index < 0) return;
  const Field& field = fields_[index];
  field.domain->write(field.index, characteristic->getValue());
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
