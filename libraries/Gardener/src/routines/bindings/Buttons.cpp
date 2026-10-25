#include "Build.h"

#if GARDENER_PANEL

#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int buttonsTake(bvm* vm) {
  String gesture;
  if (routines == nullptr || !routines->takePress(gesture)) be_return_nil(vm);
  be_pushstring(vm, gesture.c_str());
  be_return(vm);
}

const bnfuncinfo ButtonsLibrary[] = {
    {"take", buttonsTake},
    {nullptr, nullptr},
};

}  // namespace

void installButtonsModule(bvm* vm) { installModule(vm, "buttons", ButtonsLibrary); }

}  // namespace berry
}  // namespace gardener
#endif
