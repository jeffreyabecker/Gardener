#pragma once

#include <stdint.h>

// Forward declared so the vendored interpreter's header stops at Routines.cpp, the way LittleFS.h
// stops at Storage.cpp.
struct bvm;

namespace gardener {

// The Berry runtime behind the disconnected page: the VM, the routines it has loaded, and the one
// that is active. It is created once at boot and driven from the page's update(), and it owns the
// only path into the interpreter.
//
// Nothing else in the firmware includes berry.h, so no other file has to know the VM exists.
class Routines {
 public:
  // Creates the VM. False when the interpreter cannot be allocated, which is a degraded mode
  // rather than a boot failure: the page falls back to the mark.
  bool begin();

  // One entry into the VM: the active routine's poll(), or nothing when there is no active routine.
  // Called from the page, once per loop pass.
  void poll(uint32_t nowMs);

  bool active() const { return active_ != nullptr; }

 private:
  bvm* vm_ = nullptr;
  void* active_ = nullptr;
};

}  // namespace gardener
