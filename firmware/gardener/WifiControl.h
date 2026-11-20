#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace gardener {

// One network a scan found, as a front end lists it.
struct WifiNetwork {
  String ssid;
  int32_t rssi = 0;
};

// Where the radio stands: the network it is set to, the mode it ended up in, the address it answers
// on, and whether the setup network outlives a successful join.
struct WifiStatus {
  String ssid;
  String address;
  const char* mode = "sta";
  bool keepPortal = false;
};

// What a front end may ask of the network. The radio is the board's and lives in the sketch, so this
// is the seam a front end holds rather than an implementation: the wire's wifi plugin and the page
// both reach the one web link through it.
class WifiControl {
 public:
  virtual ~WifiControl() = default;

  virtual WifiStatus status() const = 0;
  // Scans and stores up to `capacity` networks; returns how many it stored.
  virtual int scan(WifiNetwork* networks, int capacity) = 0;

  // The stored choice, a field at a time — each persists — and the join that applies it.
  virtual void setSsid(const String& ssid) = 0;
  virtual void setPass(const String& pass) = 0;
  virtual void setKeepPortal(bool keepPortal) = 0;
  virtual void join() = 0;
  virtual void forget() = 0;
};

}  // namespace gardener
