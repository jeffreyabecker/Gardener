#include "app/disconnected-ui/Routines.h"

#include <berry.h>

namespace gardener {

bool Routines::begin() {
  vm_ = be_vm_new();
  return vm_ != nullptr;
}

// No routines are loaded yet: this is the VM the loader will fill, and the seat the page's poll
// will take. Called every pass, so it stays free until a routine is active.
void Routines::poll(uint32_t nowMs) {
  (void)nowMs;
}

}  // namespace gardener
