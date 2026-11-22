#pragma once

// The network a case hands the wire: it records what it was told, answers what it was given, and
// keeps the one thing the wire's replies are about — the stored choice and whether it was joined.

#include <stdint.h>

#include <vector>

#include "WifiControl.h"

namespace gardener::test {

class FakeWifi : public WifiControl {
 public:
  WifiStatus status() const override {
    WifiStatus now;
    now.ssid = ssid_;
    now.address = joined_ ? "10.0.0.9" : "-";
    now.mode = joined_ ? "sta" : "ap";
    now.keepPortal = keepPortal_;
    return now;
  }

  int scan(WifiNetwork* networks, int capacity) override {
    int stored = 0;
    for (const WifiNetwork& network : visible_) {
      if (stored >= capacity) break;
      networks[stored++] = network;
    }
    return stored;
  }

  void setSsid(const String& ssid) override { ssid_ = ssid; }
  void setPass(const String& pass) override { pass_ = pass; }
  void setKeepPortal(bool keepPortal) override { keepPortal_ = keepPortal; }
  void join() override { joined_ = true; }

  void forget() override {
    ssid_ = "";
    pass_ = "";
    keepPortal_ = false;
    joined_ = false;
  }

  // What a scan will find, in the order it finds it.
  void visible(std::vector<WifiNetwork> networks) { visible_ = networks; }

  const String& ssid() const { return ssid_; }
  const String& pass() const { return pass_; }
  bool keepPortal() const { return keepPortal_; }
  bool joined() const { return joined_; }

 private:
  String ssid_;
  String pass_;
  bool keepPortal_ = false;
  bool joined_ = false;
  std::vector<WifiNetwork> visible_;
};

}  // namespace gardener::test
