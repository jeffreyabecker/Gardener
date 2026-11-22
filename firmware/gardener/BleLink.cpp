// The link is the Bluetooth stack's; the s3-wifi profile compiles this file to nothing.
#if __has_include(<NimBLEDevice.h>)

#include "BleLink.h"

#include <NimBLEDevice.h>
#include <stdint.h>

#include "GattValues.h"
#include "core/Knob.h"

namespace gardener::runtime {

namespace {

// The name the device advertises under, and the period its status is refreshed on. Both belong to
// the link rather than to the machine, so a build that wants a different name or a slower beat
// names it here rather than on a shared header.
GARDENER_KNOB_VALID(const char*, GattNamePrefix, GARDENER_GATT_NAME_PREFIX, "Gardener", true,
                    "the prefix of the name the device advertises under");
GARDENER_KNOB_VALID(
    uint32_t, GattStatusPeriodMs, GARDENER_GATT_STATUS_PERIOD_MS, 5000,
    GARDENER_GATT_STATUS_PERIOD_MS >= 100 && GARDENER_GATT_STATUS_PERIOD_MS <= 60000,
    "how often the status is notified while a client is subscribed, in milliseconds");

// One primary service and its three characteristics, all on one 128-bit base. They are not SIG
// assignments, and a short one would collide with the first phone that ships a service of the same
// number, so every one is 128 bits.
constexpr const char* ServiceUuid = "6d3a0000-6a3f-4d1c-9d5b-9e0f4c7a2b31";
constexpr const char* StatusUuid = "6d3a0001-6a3f-4d1c-9d5b-9e0f4c7a2b31";
constexpr const char* CommandUuid = "6d3a0002-6a3f-4d1c-9d5b-9e0f4c7a2b31";
constexpr const char* ReplyUuid = "6d3a0003-6a3f-4d1c-9d5b-9e0f4c7a2b31";

// The low two bytes of the board's own address, so two boards on a bench advertise differently.
String nameFor(const char* prefix) {
  const uint64_t address = NimBLEDevice::getAddress();
  String suffix = String((uint16_t)(address & 0xFFFF), HEX);
  suffix.toUpperCase();
  return String(prefix) + "-" + suffix;
}

}  // namespace

// The stack's own callbacks: a client arriving and leaving is the whole of what the link watches
// for, and a connection forces the next beat to publish at once rather than a period later.
class BleLink::ServerCallbacks : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacks(BleLink& link) : link_(link) {}

  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { link_.connected(); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override { link_.disconnected(); }

 private:
  BleLink& link_;
};

// The command characteristic's write: a decoded command goes to the control and its outcome to the
// reply, both in this callback, because this task is the reply's only writer.
class BleLink::CommandCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit CommandCallbacks(BleLink& link) : link_(link) {}

  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    link_.commanded(characteristic);
  }

 private:
  BleLink& link_;
};

BleLink::BleLink(SystemControl& control, Logger& logger) : control_(control), logger_(logger) {}

bool BleLink::begin() {
  // The stack has to be up before the board's own address can be read, so the name is the prefix
  // alone to begin with and the suffix is set once the stack knows what the address is.
  if (!NimBLEDevice::init(GattNamePrefix)) {
    logger_.warn("ble: the stack did not start");
    return false;
  }
  const String name = nameFor(GattNamePrefix);
  NimBLEDevice::setDeviceName(name.c_str());

  // Bonding with Just Works: no passkey and no man-in-the-middle protection, because the board has
  // no display to show a number on and no keypad to type one into. Every characteristic requires
  // the encrypted link, so an unpaired client's first access starts the pair on its own side.
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  server_ = NimBLEDevice::createServer();
  server_->setCallbacks(new ServerCallbacks(*this));
  server_->advertiseOnDisconnect(true);

  NimBLEService* service = server_->createService(ServiceUuid);
  status_ = service->createCharacteristic(
      StatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC);
  NimBLECharacteristic* command = service->createCharacteristic(
      CommandUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC);
  reply_ = service->createCharacteristic(
      ReplyUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC);
  command->setCallbacks(new CommandCallbacks(*this));
  service->start();

  if (!server_->start()) {
    logger_.warn("ble: the service did not start");
    return false;
  }

  // The service UUID does not fit beside the name in a 31-byte advertising packet, so the name goes
  // in the scan response; every scanner reads that anyway.
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->enableScanResponse(true);
  advertising->addServiceUUID(ServiceUuid);
  advertising->setName(name.c_str());
  if (!advertising->start()) {
    logger_.warn("ble: advertising did not start");
    return false;
  }

  String line = "ble: advertising as ";
  line += name;
  logger_.info(line.c_str());
  return true;
}

void BleLink::poll(uint32_t nowMs) {
  if (!server_) return;

  // The value is refreshed on every pass, so a read answers the reading as the beat last saw it
  // rather than one from whenever the connection happened; only the notification waits for a client.
  const SystemStatus status = control_.status();
  const gatt::Status packet = gatt::encodeStatus(status);
  status_->setValue(packet.bytes, gatt::StatusSize);

  if (server_->getConnectedCount() == 0) return;

  const bool moved =
      static_cast<uint8_t>(status.state) != lastState_ || status.targetKpa != lastTargetKpa_;
  if (!moved && nowMs - lastStatusMs_ < GattStatusPeriodMs) return;

  status_->notify();
  lastStatusMs_ = nowMs;
  lastState_ = static_cast<uint8_t>(status.state);
  lastTargetKpa_ = status.targetKpa;
}

void BleLink::connected() {
  // The next beat publishes rather than waiting for the period.
  lastState_ = 0xFF;
  logger_.info("ble: client connected");
}

void BleLink::disconnected() { logger_.info("ble: client disconnected"); }

void BleLink::commanded(NimBLECharacteristic* characteristic) {
  const NimBLEAttValue value = characteristic->getValue();
  gatt::Command command;
  gatt::Result result = gatt::Result::BadCommand;
  gatt::Refusal refusal = gatt::Refusal::None;

  if (gatt::decodeCommand(value.data(), value.size(), command)) {
    ServeResult served = ServeResult::NotReady;
    switch (command.op) {
      case gatt::Op::Set:
        served = control_.set(command.targetKpa);
        break;
      case gatt::Op::Vent:
        served = control_.vent();
        break;
      case gatt::Op::Reset:
        served = control_.reset();
        break;
    }

    result = static_cast<gatt::Result>(static_cast<uint8_t>(served));
    if (served == ServeResult::NotReady) {
      refusal = static_cast<gatt::Refusal>(gatt::refusalCode(control_.refusal()));
    }
  }

  const gatt::Reply reply = gatt::encodeReply(result, refusal);
  reply_->setValue(reply.bytes, gatt::ReplySize);
  reply_->notify();
}

}  // namespace gardener::runtime

#endif
