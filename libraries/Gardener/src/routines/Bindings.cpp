#include "Bindings.h"

#include <berry.h>

#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "routines/bindings/BindingContext.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {

// The one `BindingContext.h` declares. install() is the one that builds it.
std::unique_ptr<BindingContext> bindings;

void install(bvm* vm, panel::PanelContext& appContext, Routines& appRoutines, Logger& appLogger) {
  bindings = std::make_unique<BindingContext>(appContext, appRoutines, appLogger);

  installVacuumModule(vm);
  installScreenModule(vm);
  installDeviceModule(vm);
  installSettingsModule(vm);
  installRoutinesModule(vm);
  installUxModule(vm);
}

}  // namespace berry
}  // namespace gardener
