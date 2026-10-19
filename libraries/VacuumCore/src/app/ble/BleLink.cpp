#include "app/ble/BleLink.h"

#include <math.h>
#include <string.h>

#include "app/Ownership.h"
#include "app/SafeReboot.h"
#include "app/VacuumSystem.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {
namespace {

// The service's base UUID: the last 16 bits select the characteristic, so the suffix in the table
// below is the whole of a characteristic's name.
constexpr const char* kServiceUuid = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10";
constexpr const char* kUuidPrefix = "6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e";
constexpr const char* kDeviceName = "gardener";
constexpr uint8_t kBleProtocolVersion = 1;

// The two rates; everything else is sent on change. 5 Hz is about as fast as a phone screen needs.
constexpr uint32_t kFastRateMs = 200;
constexpr uint32_t kBattRateMs = 10000;

// The opcodes a controller writes to `command`, and the results it is notified.
enum : uint8_t { kOpStart = 0x01, kOpStop = 0x02, kOpResetFault = 0x03, kOpReboot = 0x04 };
enum : uint8_t {
  kResultApplied = 0x00,
  kResultNoTarget = 0x01,
  kResultNoReading = 0x02,
  kResultFault = 0x03,
  kResultUnknownOpcode = 0x04,
  kResultNotControlling = 0x05,
};

struct FieldSpec {
  uint16_t suffix;
  uint32_t properties;
  uint16_t maxLength;
};

// The contract, in the order of BleLink::Field. Only the writes require encryption: nothing that
// reads here is a secret, and letting a phone observe without pairing is the point of the split.
constexpr FieldSpec kFields[] = {
    {0x0001, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                           // state
    {0x0002, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                           // kpa
    {0x0003, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},  // target
    {0x0004, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                           // duty
    {0x0005, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 12},                          // fault
    {0x0006, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                           // batt
    {0x0007, NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 1},                      // command
    {0x0008, NIMBLE_PROPERTY::READ, 16},                                                    // fw
    {0x0009, NIMBLE_PROPERTY::READ, 1},                                                     // ble_proto
    {0x000A, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC, 1},                        // unit
    {0x000B, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 1},  // control
    {0x000C, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                           // sensor
    {0x000D, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                           // floor
};

String characteristicUuid(uint16_t suffix) {
  char text[37];
  snprintf(text, sizeof(text), "%s%04x", kUuidPrefix, suffix);
  return String(text);
}

void setByte(NimBLECharacteristic* characteristic, uint8_t value) {
  // A start that failed part-way leaves characteristics missing, and nothing has a connection to
  // write to them, but the loop still walks this path once a pass.
  if (characteristic == nullptr) return;
  characteristic->setValue(&value, sizeof(value));
}

// The part is little-endian and the contract is little-endian, so the float goes out as it lies.
void setFloat(NimBLECharacteristic* characteristic, float value) {
  if (characteristic == nullptr) return;
  characteristic->setValue(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

// Whether a reading can be trusted, which is what `sensor` reports and what a run needs.
bool plausibleReading(VacuumSystem& vacuum) {
  return vacuum.sensor().present() && vacuum.sensor().readingValid();
}

}  // namespace

BleLink::Start BleLink::begin() {
  static_assert(sizeof(kFields) / sizeof(kFields[0]) == kFieldCount,
                "the table is the contract: one row per characteristic");
  static_assert(static_cast<uint8_t>(RunState::Idle) == 0 &&
                    static_cast<uint8_t>(RunState::Pulling) == 1 &&
                    static_cast<uint8_t>(RunState::Holding) == 2 &&
                    static_cast<uint8_t>(RunState::Venting) == 3 &&
                    static_cast<uint8_t>(RunState::Fault) == 4,
                "RunState is the `state` characteristic's encoding");

  if (!app_->settings().bluetoothEnabled()) return Start::Disabled;
  if (!NimBLEDevice::init(kDeviceName)) return Start::Failed;

  // Just Works, bonded: the device pairs without a passkey and keeps the bond in NVS. That is a
  // guard against a casual neighbour at the bench rather than an attacker with a radio, and it is
  // the price of a pairing flow someone will actually complete.
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) return Start::Failed;
  server_->setCallbacks(this);
  // Advertising is presence: it stops on connect and comes back when the central leaves.
  server_->advertiseOnDisconnect(true);

  NimBLEService* service = server_->createService(kServiceUuid);
  if (service == nullptr) return Start::Failed;
  for (uint8_t i = 0; i < kFieldCount; ++i) {
    const FieldSpec& spec = kFields[i];
    String uuid = characteristicUuid(spec.suffix);
    fields_[i] = service->createCharacteristic(uuid.c_str(), spec.properties, spec.maxLength);
    if (fields_[i] == nullptr) return Start::Failed;
    fields_[i]->setCallbacks(this);
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

  // The slots first, against the machine as the last control tick left it, so a notified value is
  // always one the machine actually has.
  applyPending();
  publish(nowMs);

  if (!linkLost_) return;
  linkLost_ = false;
  subscriptions_ = 0;
  // A controller that leaves while owning keeps the machine: the record stays held and stale, so a
  // reconnected central is a reader until the local reset gesture clears it.
  app_->ownership().drop(Link::Bluetooth);
}

// The loop's half of a write: ownership, the target, the unit and an opcode are applied here rather
// than in the callback, so nothing from the radio task touches the machine.
void BleLink::applyPending() {
  Ownership& ownership = app_->ownership();

  // Ownership first: every other write is gated on it, and a release is the safe exit.
  if (control_.has) {
    control_.has = false;
    if (control_.value != 0) {
      ownership.acquire(Link::Bluetooth);
    } else if (ownership.release(Link::Bluetooth)) {
      app_->vacuum().stop();
    }
    // The caller's view, so a refused acquire reads back as still not owned.
    describe(Field::Control);
    notify(Field::Control);
    lastControl_ = owned() ? 1 : 0;
  }

  if (target_.has) {
    target_.has = false;
    const float kpa = target_.value;
    if (owned() && isfinite(kpa)) {
      // 0 clears the target; anything else is the vacuum system's to accept or refuse against its
      // own floor, so the two surfaces cannot disagree about what a target is.
      if (kpa == 0.0f) {
        app_->vacuum().clearTarget();
      } else {
        app_->vacuum().setTarget(kpa);
      }
    }
    // Read back rather than echo: a refused value leaves the target where it was, and the read is
    // the answer. Only a target that actually moved is worth a notification.
    describe(Field::Target);
    const float applied = app_->vacuum().targetKpa();
    if (applied != lastTarget_) {
      lastTarget_ = applied;
      notify(Field::Target);
    }
  }

  if (unit_.has) {
    unit_.has = false;
    if (owned() && unit_.value < kDisplayUnitCount) {
      app_->settings().setUnit(static_cast<DisplayUnit>(unit_.value));
    }
    // Not notified: one central at a time means the client that wrote it already knows.
    describe(Field::Unit);
  }

  if (command_.has) {
    command_.has = false;
    const uint8_t opcode = command_.value;
    const uint8_t result = execute(opcode);
    setByte(characteristic(Field::Command), result);
    notify(Field::Command);
    // The result goes out first; the release and the restart follow.
    if (opcode == kOpReboot && result == kResultApplied) safeReboot(*app_);
  }
}

// Runs one opcode. `start` reports the same refusals the serial link reports, from the same
// VacuumSystem::startRefusal(), so the two surfaces cannot disagree about why a run will not start.
uint8_t BleLink::execute(uint8_t opcode) {
  if (!owned()) return kResultNotControlling;

  VacuumSystem& vacuum = app_->vacuum();
  switch (opcode) {
    case kOpStart: {
      if (vacuum.start()) return kResultApplied;
      const char* refusal = vacuum.startRefusal();
      if (refusal == nullptr) return kResultApplied;
      if (strcmp(refusal, "latch") == 0) return kResultFault;
      if (strcmp(refusal, "no_target") == 0) return kResultNoTarget;
      if (strcmp(refusal, "no_reading") == 0) return kResultNoReading;
      return kResultUnknownOpcode;
    }
    case kOpStop:
      vacuum.stop();
      return kResultApplied;
    case kOpResetFault:
      vacuum.resetFault();
      return kResultApplied;
    case kOpReboot:
      return kResultApplied;
    default:
      return kResultUnknownOpcode;
  }
}

// Sends whatever is due. A value only moves when the machine moves it, so the read and the
// notification are one description of the same field.
void BleLink::publish(uint32_t nowMs) {
  if (!connected()) return;

  const bool force = baseline_;
  baseline_ = false;

  VacuumSystem& vacuum = app_->vacuum();
  publishOnChange(Field::State, static_cast<uint8_t>(vacuum.runState()), lastState_, force);
  publishOnChange(Field::Sensor, plausibleReading(vacuum) ? 1 : 0, lastSensor_, force);
  publishOnChange(Field::Control, owned() ? 1 : 0, lastControl_, force);
  publishOnChange(Field::Target, vacuum.targetKpa(), lastTarget_, force);
  publishOnChange(Field::Floor, cfg::CONTROL_FLOOR_KPA, lastFloor_, force);

  const char* fault = vacuum.faultCode();
  if (force || lastFault_ != fault) {
    lastFault_ = fault;
    describe(Field::Fault);
    notify(Field::Fault);
  }

  if (nowMs - lastFastMs_ >= kFastRateMs) {
    lastFastMs_ = nowMs;
    describe(Field::Kpa);
    notify(Field::Kpa);
    describe(Field::Duty);
    notify(Field::Duty);
  }

  if (nowMs - lastBattMs_ >= kBattRateMs) {
    lastBattMs_ = nowMs;
    describe(Field::Batt);
    notify(Field::Batt);
  }
}

void BleLink::publishOnChange(Field field, uint8_t value, uint8_t& last, bool force) {
  if (!force && value == last) return;
  last = value;
  describe(field);
  notify(field);
}

void BleLink::publishOnChange(Field field, float value, float& last, bool force) {
  if (!force && value == last) return;
  last = value;
  describe(field);
  notify(field);
}

// The one place a characteristic's value is built, so a read and a notification cannot disagree.
void BleLink::describe(Field field) {
  VacuumSystem& vacuum = app_->vacuum();
  NimBLECharacteristic* chr = characteristic(field);
  if (chr == nullptr) return;
  switch (field) {
    case Field::State:
      setByte(chr, static_cast<uint8_t>(vacuum.runState()));
      break;
    case Field::Kpa:
      setFloat(chr, vacuum.currentKpa());
      break;
    case Field::Target:
      setFloat(chr, vacuum.targetKpa());
      break;
    case Field::Duty:
      setFloat(chr, vacuum.pump().dutyPercent());
      break;
    case Field::Fault:
      chr->setValue(vacuum.faultCode());
      break;
    case Field::Batt:
      setByte(chr, app_->battery().present() ? app_->battery().percent() : 0xFF);
      break;
    case Field::Command:
      break;  // the last result, written when an opcode is applied
    case Field::Firmware:
      chr->setValue(cfg::FIRMWARE_VERSION);
      break;
    case Field::Protocol:
      setByte(chr, kBleProtocolVersion);
      break;
    case Field::Unit:
      setByte(chr, static_cast<uint8_t>(app_->settings().unit()));
      break;
    case Field::Control:
      setByte(chr, owned() ? 1 : 0);
      break;
    case Field::Sensor:
      setByte(chr, plausibleReading(vacuum) ? 1 : 0);
      break;
    case Field::Floor:
      setFloat(chr, cfg::CONTROL_FLOOR_KPA);
      break;
    case Field::Count:
      break;
  }
}

void BleLink::notify(Field field) {
  if (!subscribed(field)) return;
  NimBLECharacteristic* chr = characteristic(field);
  if (chr == nullptr) return;
  chr->notify();
}

bool BleLink::subscribed(Field field) const {
  return (subscriptions_ & (1u << static_cast<uint8_t>(field))) != 0;
}

bool BleLink::owned() const { return app_->ownership().ownedBy(Link::Bluetooth); }

bool BleLink::find(const NimBLECharacteristic* characteristic, Field& field) const {
  for (uint8_t i = 0; i < kFieldCount; ++i) {
    if (fields_[i] == characteristic) {
      field = static_cast<Field>(i);
      return true;
    }
  }
  return false;
}

// --- NimBLE's callbacks, on the host task -----------------------------------

void BleLink::onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) {
  (void)server;
  (void)info;
  (void)reason;
  // Nothing is decided here: the loop applies the departure on its next pass, so core 1 keeps being
  // the only place that touches ownership.
  subscriptions_ = 0;
  linkLost_ = true;
}

void BleLink::onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  Field field;
  if (!find(characteristic, field)) return;
  describe(field);
}

// A write only lands in its slot. Lengths are checked here because they are the payload's shape,
// not a decision about the machine.
void BleLink::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) {
  (void)info;
  Field field;
  if (!find(characteristic, field)) return;

  const NimBLEAttValue& value = characteristic->getValue();
  switch (field) {
    case Field::Target: {
      if (value.size() != sizeof(float)) return;
      float kpa = 0.0f;
      memcpy(&kpa, value.data(), sizeof(kpa));
      target_.value = kpa;
      target_.has = true;
      break;
    }
    case Field::Unit:
      if (value.size() != 1) return;
      unit_.value = value.data()[0];
      unit_.has = true;
      break;
    case Field::Command:
      if (value.size() != 1) return;
      command_.value = value.data()[0];
      command_.has = true;
      break;
    case Field::Control:
      if (value.size() != 1) return;
      control_.value = value.data()[0];
      control_.has = true;
      break;
    default:
      break;  // the stack refuses a write to anything else before it gets here
  }
}

void BleLink::onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& info,
                          uint16_t value) {
  (void)info;
  Field field;
  if (!find(characteristic, field)) return;

  const uint16_t bit = static_cast<uint16_t>(1u << static_cast<uint8_t>(field));
  if (value == 0) {
    subscriptions_ &= static_cast<uint16_t>(~bit);
    return;
  }
  subscriptions_ |= bit;
  // The next pass pushes the current value of every on-change field, so a subscriber starts from the
  // machine's state rather than from the first thing that moves.
  baseline_ = true;
}

}  // namespace gardener
