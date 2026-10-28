#include "Build.h"

#if GARDENER_PANEL

#include "Bindings.h"

#include <berry.h>

#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {

// The pointers `Module.h` declares. install() is the one assignment.
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
  installSettingsModule(vm);
  installRegistrationClass(vm);
}

}  // namespace berry
}  // namespace gardener
#endif
