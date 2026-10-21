#pragma once

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/MachineBoot.h"
#include "app/storage/Storage.h"

namespace gardener {

class Logger;
class PanelContext;

// Boot. The panel's boot brings up the panel, the VM, its registrations and its task, on the links
// the machine's boot built. The full build's App::begin composes the two in order, mounting the
// panel's storage and settings itself first because the persisted knobs — the radio and the
// diagnostics — gate the machine's boot.
void panelBoot(PanelContext& context, Logger& logger, const MachineBoot& machine,
               Storage::Mount mount);

class App {
 public:
  explicit App(AppContext context);

  bool begin();

 private:
  AppContext context_;
};

}  // namespace gardener
