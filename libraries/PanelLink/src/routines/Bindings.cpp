#include "routines/Bindings.h"

#include <berry.h>

#include "Logger.h"
#include "PanelContext.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {

// The pointers `bindings/Module.h` declares. install() is the one assignment.
PanelContext* context = nullptr;
Routines* routines = nullptr;
Logger* logger = nullptr;

void install(bvm* vm, PanelContext& appContext, Routines& appRoutines, Logger& appLogger) {
  context = &appContext;
  routines = &appRoutines;
  logger = &appLogger;

  installVacuumModule(vm);
  installScreenModule(vm);
  installDeviceModule(vm);
  installButtonsModule(vm);
  installBuzzerModule(vm);
  installJinglesModule(vm, appContext.library());
  installStoreModule(vm);
}

}  // namespace berry
}  // namespace gardener
