#include "routines/Bindings.h"

#include <berry.h>

#include "Logger.h"
#include "PanelContext.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {

// The pointers `bindings/Module.h` declares. install() is the one assignment.
PanelContext* gContext = nullptr;
Routines* gRoutines = nullptr;
Logger* gLogger = nullptr;

void install(bvm* vm, PanelContext& context, Routines& routines, Logger& logger) {
  gContext = &context;
  gRoutines = &routines;
  gLogger = &logger;

  installVacuumModule(vm);
  installScreenModule(vm);
  installDeviceModule(vm);
  installButtonsModule(vm);
  installBuzzerModule(vm);
  installJinglesModule(vm, context.library());
  installUxModule(vm);
  installStoreModule(vm);
}

}  // namespace berry
}  // namespace gardener
