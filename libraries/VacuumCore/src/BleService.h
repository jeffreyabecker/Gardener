#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "AppContext.h"
#include "ProgramStore.h"
#include "Protocol.h"
#include "VacuumConfig.h"

namespace eggplan {

// Bluetooth LE transport.
//
// Four characteristics under one service:
//   cmd   write     newline terminated command lines (same grammar as serial)
//   rsp   notify    replies; long replies are split into 0x01-tagged chunks and
//                   terminated by a 0x02 byte
//   prog  write     program transfer frames, one opcode per write
//   info  read      device information JSON
//
// Telemetry notifications are prefixed with "tel " so a client can tell them
// apart from command replies.
class BleService : public ResponseSink {
 public:
  void begin(AppContext& ctx);
  void update(uint32_t nowMs);

  bool connected() const;
  void setTelemetry(bool enabled, uint32_t periodMs);
  bool telemetryEnabled() const { return telemetry_; }
  uint32_t telemetryPeriodMs() const { return telemetryPeriodMs_; }

  void emit(const String& text) override;
  void notifyTelemetry();

  const char* deviceName() const { return name_.c_str(); }

 private:
  class ServerCallbacks;
  class CharCallbacks;

  void handleConnect(uint16_t mtu);
  void handleDisconnect(int reason);
  void handleMtuChange(uint16_t mtu);
  void onCommandData(const uint8_t* data, size_t len);
  void onProgramData(const uint8_t* data, size_t len);
  void readInfo();
  uint16_t chunkSize() const;

  AppContext ctx_;
  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* cmdChar_ = nullptr;
  NimBLECharacteristic* rspChar_ = nullptr;
  NimBLECharacteristic* progChar_ = nullptr;
  NimBLECharacteristic* infoChar_ = nullptr;
  CommandRouter router_{ctx_};
  ProgramUploader uploader_;

  String name_;
  String lineBuffer_;
  bool started_ = false;
  bool telemetry_ = false;
  uint32_t telemetryPeriodMs_ = cfg::TELEMETRY_DEFAULT_MS;
  uint32_t lastTelemetryMs_ = 0;
  uint16_t mtu_ = 23;
  CommandRouter* routerPtr_ = nullptr;
};

}  // namespace eggplan
