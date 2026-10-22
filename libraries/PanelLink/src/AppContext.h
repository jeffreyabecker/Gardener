#pragma once

#include "MachineContext.h"
#include "PanelContext.h"

namespace gardener {

// The two graphs the boot hands out, by reference. The machine's code reads the machine context,
// and the panel's code reads the panel context, which reaches the machine through machine().
class AppContext {
 public:
  AppContext(MachineContext& machine, PanelContext& panel) : machine_(machine), panel_(panel) {}

  MachineContext& machine() { return machine_; }
  PanelContext& panel() { return panel_; }

 private:
  MachineContext& machine_;
  PanelContext& panel_;
};

}  // namespace gardener
