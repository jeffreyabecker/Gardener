#include "hosts/ble/BleService.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "core/Config.h"
#include "core/Log.h"
#include "hosts/commands/CommandRouter.h"
#include "services/SnapshotText.h"

namespace eggplan {
namespace {

// Service and characteristics, documented in docs/ble-protocol.md.
const char* const kServiceUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00";
const char* const kCmdUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01";
const char* const kRspUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02";
const char* const kStateUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03";
const char* const kInfoUuid = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04";

// Long replies are split: 0x01 + sequence byte + payload, terminated by 0x02.
constexpr uint8_t kChunkMarker = 0x01;
constexpr uint8_t kEndMarker = 0x02;
constexpr size_t kMaxNotification = 512;
constexpr size_t kLineMax = 512;

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
  enum class Role { Command, Info, State };
  CharCallbacks(BleService* owner, Role role) : owner_(owner), role_(role) {}
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
  void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
  void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo,
                   uint16_t subValue) override;

 private:
  BleService* owner_;
  Role role_;
};

BleService::BleService(CommandRouter& router, TelemetrySettings& telemetry,
                       ReplyQueue& replies, ReplyQueue& inbound,
                       const SystemSnapshot& snapshot)
    : router_(router),
      telemetry_(telemetry),
      replies_(replies),
      inbound_(inbound),
      snapshot_(snapshot) {}

void BleService::begin() {
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
  stateChar_ = service->createCharacteristic(
      kStateUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  infoChar_ = service->createCharacteristic(kInfoUuid, NIMBLE_PROPERTY::READ);

  cmdChar_->setCallbacks(new CharCallbacks(this, CharCallbacks::Role::Command));
  stateChar_->setCallbacks(new CharCallbacks(this, CharCallbacks::Role::State));
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
  uint16_t size = mtu_ > 3 ? (uint16_t)(mtu_ - 3) : 20;
  return size > kMaxNotification ? (uint16_t)kMaxNotification : size;
}

bool BleService::consumeLinkLost() {
  bool lost = linkLost_;
  linkLost_ = false;
  return lost;
}

void BleService::poll(uint32_t nowMs) {
  if (!started_) return;

  // A connected but silent central only matters if the build says it does (Q17).
  if (cfg::LINK_TIMEOUT_MS > 0 && connected() && lastWriteMs_ != 0 &&
      (nowMs - lastWriteMs_) > cfg::LINK_TIMEOUT_MS) {
    linkLost_ = true;
    lastWriteMs_ = nowMs;  // report once per window rather than once per loop
  }

  drainInbound();
  drainReplies();

  if (stateSubscribers_ == 0 || !connected()) return;
  if (lastTelemetryMs_ != 0 && (nowMs - lastTelemetryMs_) < telemetry_.periodMs) return;
  lastTelemetryMs_ = nowMs;
  sendTelemetry();
}

void BleService::drainInbound() {
  String line;
  while (inbound_.pop(line)) {
    router_.handleLine(line, replies_, snapshot_);
  }
}

void BleService::drainReplies() {
  String line;
  while (replies_.pop(line)) {
    transmit(rspChar_, line);
  }
}

void BleService::transmit(NimBLECharacteristic* characteristic, const String& text) {
  if (!characteristic || !connected()) {
    LOGD("ble: dropped notification (no client): %s", text.c_str());
    return;
  }

  const uint16_t maxChunk = chunkSize();
  if (text.length() <= maxChunk) {
    characteristic->setValue((const uint8_t*)text.c_str(), text.length());
    characteristic->notify();
    return;
  }

  size_t offset = 0;
  uint8_t sequence = 0;
  uint8_t frame[kMaxNotification];
  while (offset < text.length()) {
    size_t take = text.length() - offset;
    if (take > (size_t)(maxChunk - 2)) take = maxChunk - 2;
    frame[0] = kChunkMarker;
    frame[1] = sequence++;
    memcpy(frame + 2, text.c_str() + offset, take);
    characteristic->setValue(frame, take + 2);
    characteristic->notify();
    offset += take;
    delay(4);  // let the stack drain its notification queue
  }
  uint8_t endFrame[2] = {kEndMarker, sequence};
  characteristic->setValue(endFrame, sizeof(endFrame));
  characteristic->notify();
}

void BleService::refreshStateValue() {
  if (!stateChar_) return;
  char body[224];
  size_t size = snapshottext::statusBody(snapshot_, body, sizeof(body));
  stateChar_->setValue((const uint8_t*)body, size);
}

void BleService::sendTelemetry() {
  if (!stateChar_) return;
  char body[224];
  snapshottext::statusBody(snapshot_, body, sizeof(body));
  transmit(stateChar_, String(body));
}

void BleService::onCommandData(const uint8_t* data, size_t length) {
  // Runs on the NimBLE task: assemble lines here, act on them in poll().
  lastWriteMs_ = millis();
  for (size_t i = 0; i < length; i++) {
    char c = (char)data[i];
    if (c == '\r') continue;
    if (c == '\n') {
      if (lineBuffer_.length() && !inbound_.push(lineBuffer_)) {
        LOGW("ble: inbound queue full, line dropped");
      }
      lineBuffer_ = "";
      continue;
    }
    if (lineBuffer_.length() >= kLineMax) {
      lineBuffer_ = "";
      LOGW("ble: command line too long, dropped");
      continue;
    }
    lineBuffer_ += c;
  }
}

void BleService::onInfoRead() {
  if (!infoChar_) return;
  char body[224];
  size_t size = snapshottext::infoBody(snapshot_, body, sizeof(body));
  infoChar_->setValue((const uint8_t*)body, size);
}

void BleService::handleConnect(uint16_t mtu) {
  mtu_ = mtu;
  lineBuffer_ = "";
  lastWriteMs_ = millis();
  linkLost_ = false;
  stateSubscribers_ = 0;
  LOGI("ble: client connected (mtu %u)", (unsigned)mtu);
  // Fill the state characteristic so a client can read it before subscribing.
  refreshStateValue();
}

void BleService::handleDisconnect(int reason) {
  lineBuffer_ = "";
  replies_.clear();
  inbound_.clear();
  linkLost_ = true;
  // Subscriptions belong to the connection, so the stack has already dropped it.
  stateSubscribers_ = 0;
  LOGI("ble: client disconnected (reason %d)", reason);
}

// subValue is 0 for unsubscribe, 1 for notify and 2 for indicate.
void BleService::onStateSubscribe(uint16_t subValue) {
  if (subValue == 0) {
    if (stateSubscribers_ > 0) stateSubscribers_--;
    LOGI("ble: state telemetry unsubscribed");
    return;
  }
  stateSubscribers_++;
  LOGI("ble: state telemetry subscribed (%u client(s))", (unsigned)stateSubscribers_);
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

void BleService::ServerCallbacks::onMTUChange(uint16_t mtu, NimBLEConnInfo& connInfo) {
  (void)connInfo;
  owner_->handleMtuChange(mtu);
}

void BleService::CharCallbacks::onWrite(NimBLECharacteristic* characteristic,
                                        NimBLEConnInfo& connInfo) {
  (void)connInfo;
  NimBLEAttValue value = characteristic->getValue();
  const uint8_t* data = value.data();
  size_t length = value.length();
  if (length == 0) return;
  if (role_ == Role::Command) owner_->onCommandData(data, length);
}

void BleService::CharCallbacks::onRead(NimBLECharacteristic* characteristic,
                                       NimBLEConnInfo& connInfo) {
  (void)characteristic;
  (void)connInfo;
  if (role_ == Role::Info) {
    owner_->onInfoRead();
  } else if (role_ == Role::State) {
    owner_->refreshStateValue();
  }
}

void BleService::CharCallbacks::onSubscribe(NimBLECharacteristic* characteristic,
                                            NimBLEConnInfo& connInfo, uint16_t subValue) {
  (void)characteristic;
  (void)connInfo;
  if (role_ == Role::State) owner_->onStateSubscribe(subValue);
}

}  // namespace eggplan
