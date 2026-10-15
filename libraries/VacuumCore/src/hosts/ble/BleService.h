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
//
// Telemetry leaves on its own characteristic, and the client's CCCD subscription is
// what turns it on: the subscription is the only enable, so it can never disagree with
// a command.
class BleService {
 public:
  BleService(CommandRouter& router, TelemetrySettings& telemetry, ReplyQueue& replies,
             ReplyQueue& inbound, const SystemSnapshot& snapshot);

  void begin();
  void poll(uint32_t nowMs);
  bool connected() const;
  // One shot: the link was lost, by disconnect or by silence from a connected central.
  // App decides what that means (stop and vent); the transport only reports it.
  bool consumeLinkLost();
  const char* deviceName() const { return name_.c_str(); }

 private:
  class ServerCallbacks;
  class CharCallbacks;

  void handleConnect(uint16_t mtu);
  void handleDisconnect(int reason);
  void handleMtuChange(uint16_t mtu);
  void onCommandData(const uint8_t* data, size_t length);
  void onInfoRead();
  void onStateSubscribe(uint16_t subValue);
  void drainInbound();
  void drainReplies();
  void refreshStateValue();
  void transmit(NimBLECharacteristic* characteristic, const String& text);
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
  NimBLECharacteristic* stateChar_ = nullptr;
  NimBLECharacteristic* infoChar_ = nullptr;

  String name_;
  String lineBuffer_;
  bool started_ = false;
  bool linkLost_ = false;
  uint16_t mtu_ = 23;
  uint16_t stateSubscribers_ = 0;
  uint32_t lastTelemetryMs_ = 0;
  uint32_t lastWriteMs_ = 0;
};

}  // namespace eggplan
