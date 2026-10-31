#include "ble/BleLink.h"

#include <stdio.h>

namespace gardener {
namespace {

// The service's base UUID: the last 16 bits select the characteristic, so the
// suffix in each slot's spec is the whole of a characteristic's name.
constexpr const char* ServiceUuid = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10";
constexpr const char* UuidPrefix = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e";
constexpr const char* DeviceName = "gardener";

String characteristicUuid(uint16_t suffix) {
  char text[37];
  snprintf(text, sizeof(text), "%s%04x", UuidPrefix, suffix);
  return String(text);
}

}  // namespace

BleLink::Start BleLink::begin() {
  static_assert(Count <= 16, "subscriptions_ is one bit per slot");

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

  // The table is fixed, so every characteristic is created here, in slot order.
  for (uint8_t i = 0; i < Count; ++i) {
    const Spec& spec = Specs[i];
    NimBLECharacteristic* characteristic = service->createCharacteristic(
        characteristicUuid(spec.suffix).c_str(), spec.properties, spec.maxLength);
    if (characteristic == nullptr) return Start::Failed;
    characteristic->setCallbacks(this);
    characteristics_[i] = characteristic;
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
  outbox_.flush();

  // A fresh subscription wants a baseline, and it is consumed by the first pass after it.
  const bool baseline = baseline_;
  baseline_ = false;
  applyPending();
  publish(nowMs, baseline);
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
  for (uint8_t i = 0; i < Count; ++i) {
    if (characteristics_[i] == characteristic) return static_cast<int>(i);
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
  describe(static_cast<uint8_t>(index));
}

// A write only lands in its slot. Lengths are checked in `write`, because they are the payload's
// shape, not a decision about the machine.
void BleLink::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  const int index = find(characteristic);
  if (index < 0) return;
  write(static_cast<uint8_t>(index), characteristic->getValue());
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

// --- the value setters ------------------------------------------------------

void BleLink::setByte(NimBLECharacteristic* characteristic, uint8_t value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(&value, sizeof(value));
}

// The part is little-endian and the contract is little-endian, so the float
// goes out as it lies.
void BleLink::setFloat(NimBLECharacteristic* characteristic, float value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

// The same trade for the integer fields: the ESP32 and the contract are both little-endian.
void BleLink::setUint16(NimBLECharacteristic* characteristic, uint16_t value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void BleLink::setUint32(NimBLECharacteristic* characteristic, uint32_t value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

}  // namespace gardener
