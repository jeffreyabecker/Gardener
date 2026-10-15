#include "hosts/ble/BleService.h"

#include "core/Log.h"

namespace eggplan {
namespace {

// Service and characteristic UUIDs, all sharing one base.
const char* const kServiceUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00";
const char* const kCmdUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01";
const char* const kRspUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02";
const char* const kProgUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03";
const char* const kInfoUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04";

constexpr uint8_t kChunkMarker = 0x01;
constexpr uint8_t kEndMarker = 0x02;

}  // namespace

class BleService::ServerCallbacks : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacks(BleService* owner) : owner_(owner) {}
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override;
  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override;
  void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override;

 private:
  BleService* owner_;
};

class BleService::CharCallbacks : public NimBLECharacteristicCallbacks {
 public:
  enum class Role { Command, Program, Info };
  CharCallbacks(BleService* owner, Role role) : owner_(owner), role_(role) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
  void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;

 private:
  BleService* owner_;
  Role role_;
};

void BleService::begin(AppContext& ctx) {
  ctx_ = ctx;
  routerPtr_ = &router_;
  uploader_.setStore(ctx_.store);

  char suffix[8];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(suffix, sizeof(suffix), "%02X%02X", (uint8_t)(mac >> 8), (uint8_t)mac);
  name_ = String(cfg::BLE_NAME_PREFIX) + suffix;

  NimBLEDevice::init(name_.c_str());
  NimBLEDevice::setMTU(cfg::BLE_MTU);

  server_ = NimBLEDevice::createServer();
  if (!server_) {
    LOGE("ble: server creation failed");
    return;
  }
  server_->setCallbacks(new ServerCallbacks(this));

  NimBLEService* service = server_->createService(kServiceUuid);
  if (!service) {
    LOGE("ble: service creation failed");
    return;
  }

  cmdChar_ = service->createCharacteristic(
      kCmdUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rspChar_ = service->createCharacteristic(
      kRspUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  progChar_ = service->createCharacteristic(
      kProgUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  infoChar_ = service->createCharacteristic(kInfoUuid, NIMBLE_PROPERTY::READ);

  cmdChar_->setCallbacks(new CharCallbacks(this, CharCallbacks::Role::Command));
  progChar_->setCallbacks(new CharCallbacks(this, CharCallbacks::Role::Program));
  infoChar_->setCallbacks(new CharCallbacks(this, CharCallbacks::Role::Info));
  cmdChar_->setValue("");

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setName(name_.c_str());
  advertising->enableScanResponse(true);
  advertising->start();

  started_ = true;
  LOGI("ble: advertising as %s", name_.c_str());
}

bool BleService::connected() const {
  return started_ && server_ && server_->getConnectedCount() > 0;
}

uint16_t BleService::chunkSize() const {
  uint16_t size = (mtu_ > 3) ? (uint16_t)(mtu_ - 3) : 20;
  if (size > 512) size = 512;
  return size;
}

void BleService::emit(const String& text) {
  if (!started_ || !rspChar_) {
    return;
  }
  if (!connected()) {
    LOGD("ble: drop reply (no client): %s", text.c_str());
    return;
  }

  const uint16_t maxChunk = chunkSize();
  if (text.length() <= maxChunk) {
    rspChar_->setValue((const uint8_t*)text.c_str(), text.length());
    rspChar_->notify();
    return;
  }

  size_t offset = 0;
  uint8_t sequence = 0;
  uint8_t frame[514];
  while (offset < text.length()) {
    size_t take = text.length() - offset;
    if (take > (size_t)(maxChunk - 2)) take = maxChunk - 2;
    frame[0] = kChunkMarker;
    frame[1] = sequence++;
    memcpy(frame + 2, text.c_str() + offset, take);
    rspChar_->setValue(frame, take + 2);
    rspChar_->notify();
    offset += take;
    delay(4);  // give the stack time to drain the notification queue
  }
  uint8_t endFrame[2] = {kEndMarker, sequence};
  rspChar_->setValue(endFrame, sizeof(endFrame));
  rspChar_->notify();
}

void BleService::setTelemetry(bool enabled, uint32_t periodMs) {
  telemetry_ = enabled;
  if (periodMs >= 50) telemetryPeriodMs_ = periodMs;
  lastTelemetryMs_ = 0;
  LOGI("ble: telemetry %s every %ums", enabled ? "on" : "off",
       (unsigned)telemetryPeriodMs_);
}

void BleService::notifyTelemetry() {
  if (!routerPtr_) return;
  emit("tel " + routerPtr_->statusJson());
}

void BleService::update(uint32_t nowMs) {
  if (!started_) return;
  if (!telemetry_ || !connected()) return;
  if (lastTelemetryMs_ != 0 && (nowMs - lastTelemetryMs_) < telemetryPeriodMs_) return;
  lastTelemetryMs_ = nowMs;
  notifyTelemetry();
}

void BleService::onCommandData(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c == '\r') continue;
    if (c == '\n') {
      String line = lineBuffer_;
      lineBuffer_ = "";
      if (line.length() && routerPtr_) {
        routerPtr_->handleLine(line, *this);
      }
      continue;
    }
    if (lineBuffer_.length() >= cfg::BLE_LINE_MAX) {
      lineBuffer_ = "";
      emit("err line too long");
      continue;
    }
    lineBuffer_ += c;
  }
}

void BleService::onProgramData(const uint8_t* data, size_t len) {
  if (!uploader_.active() && !ctx_.store) {
    emit("err store unavailable");
    return;
  }
  emit(uploader_.handle(data, len));
}

void BleService::readInfo() {
  if (!infoChar_ || !routerPtr_) return;
  String payload = routerPtr_->infoJson();
  infoChar_->setValue((const uint8_t*)payload.c_str(), payload.length());
}

void BleService::handleConnect(uint16_t mtu) {
  mtu_ = mtu;
  lineBuffer_ = "";
  LOGI("ble: client connected (mtu %u)", (unsigned)mtu);
  emit("ok hello " + String(cfg::FIRMWARE_VERSION));
}

void BleService::handleDisconnect(int reason) {
  lineBuffer_ = "";
  if (uploader_.active()) {
    uploader_.abort("client disconnected");
  }
  LOGI("ble: client disconnected (reason %d)", reason);
}

void BleService::handleMtuChange(uint16_t mtu) {
  mtu_ = mtu;
  LOGI("ble: mtu now %u", (unsigned)mtu);
}

void BleService::ServerCallbacks::onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) {
  (void)server;
  owner_->handleConnect(connInfo.getMTU());
}

void BleService::ServerCallbacks::onDisconnect(NimBLEServer* server,
                                               NimBLEConnInfo& connInfo, int reason) {
  (void)server;
  (void)connInfo;
  owner_->handleDisconnect(reason);
}

void BleService::ServerCallbacks::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) {
  (void)connInfo;
  owner_->handleMtuChange(MTU);
}

void BleService::CharCallbacks::onWrite(NimBLECharacteristic* characteristic,
                                        NimBLEConnInfo& connInfo) {
  (void)connInfo;
  NimBLEAttValue value = characteristic->getValue();
  const uint8_t* data = value.data();
  size_t len = value.length();
  if (len == 0) return;
  switch (role_) {
    case Role::Command: owner_->onCommandData(data, len); break;
    case Role::Program: owner_->onProgramData(data, len); break;
    case Role::Info: break;
  }
}

void BleService::CharCallbacks::onRead(NimBLECharacteristic* characteristic,
                                       NimBLEConnInfo& connInfo) {
  (void)connInfo;
  if (role_ == Role::Info) {
    owner_->readInfo();
  }
}

}  // namespace eggplan
