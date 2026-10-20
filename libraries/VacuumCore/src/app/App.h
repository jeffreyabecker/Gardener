#pragma once

#include <Arduino.h>

#include "app/AppContext.h"

// Boot. App brings up the parts, the links and the screens, then spawns the three tasks that own
// everything after that — the machine on the control task, the wire on the interface task, the
// panel and the VM on the panel task. It knows only the AppContext, so which parts are fitted is
// the build function's business.
namespace gardener {

class App {
 public:
  explicit App(AppContext context);

  bool begin();

 private:
  AppContext context_;
};

}  // namespace gardener
