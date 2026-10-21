#pragma once

#include <stdint.h>

namespace gardener {

// The capability bits, one byte on both links.
enum : uint8_t {
  kCapPanel = 0x01,        // a panel (and with it keys, pages and the VM) is fitted
  kCapBattery = 0x02,      // a battery gauge is fitted; a wired-only device has none
  kCapOwnershipStop = 0x04,  // the machine stops itself when its owner leaves
};

// What the composition root built: which optional assemblies are fitted and
// how the device stops itself. One record, read by both links, so a host gets
// the same answer whichever transport it uses. The composition root sets it
// once at build time and it never changes after.
class Capabilities {
 public:
  void setPanel(bool fitted) { panel_ = fitted; }
  void setBattery(bool fitted) { battery_ = fitted; }
  void setOwnershipStop(bool enabled) { ownershipStop_ = enabled; }

  bool panel() const { return panel_; }
  bool battery() const { return battery_; }
  bool ownershipStop() const { return ownershipStop_; }

  uint8_t mask() const {
    uint8_t value = 0;
    if (panel_) value |= kCapPanel;
    if (battery_) value |= kCapBattery;
    if (ownershipStop_) value |= kCapOwnershipStop;
    return value;
  }

 private:
  bool panel_ = false;
  bool battery_ = false;
  bool ownershipStop_ = false;
};

}  // namespace gardener
