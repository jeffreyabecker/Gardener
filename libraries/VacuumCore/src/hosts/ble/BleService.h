#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "core/ReplyQueue.h"
#include "core/Snapshot.h"

namespace eggplan {

class CommandRouter;

// Bluetooth LE transport: NimBLE wiring, notification chunking and inbound line
// assembly. Nothing else.
//
// Inbound command bytes are parked in a queue and drained from poll(), so all policy
// runs on the main loop while the NimBLE task only hands over bytes. Replies go the
// other way through a queue; the machine view arrives as a constructor reference, so
// the transport owns no view state and nothing points back at it.
class BleService {
 public:
  BleService(CommandRouter& router, TelemetrySettings& telemetry, ReplyQueue& replies,
             ReplyQueue& inbound, const SystemSnapshot& snapshot);

  void begin();
  void poll(uint32_t nowMs);
  bool connected() const;
  const char* deviceName() const { return name_.c_str(); }

 private:
  class ServerCallbacks;
  class CharCallbacks;

  void handleConnect(uint16_t mtu);
  void handleDisconnect(int reason);
  void handleMtuChange(uint16_t mtu);
  void onCommandData(const uint8_t* data, size_t length);
  void onInfoRead();
  void drainInbound();
  void drainReplies();
  void send(const String& text);
  void sendTelemetry();
  uint16_t chunkSize() const;

  CommandRouter& router_;
  TelemetrySettings& telemetry_;
  ReplyQueue& replies_;
  ReplyQueue& inbound_;
  const SystemSnapshot& snapshot_;

  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* cmdChar_ = nullptr;
  NimBLECharacteristic* rspChar_ = nullptr;
  NimBLECharacteristic* infoChar_ = nullptr;

  String name_;
  String lineBuffer_;
  bool started_ = false;
  uint16_t mtu_ = 23;
  uint32_t lastTelemetryMs_ = 0;
};

}  // namespace eggplan
