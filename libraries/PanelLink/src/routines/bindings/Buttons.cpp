#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "hal/Button.h"

namespace gardener {
namespace berry {
namespace {

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int buttonsTake(bvm* vm) {
  Button button = Button::Up;
  Press press = Press::None;
  if (gRoutines == nullptr || !gRoutines->takePress(button, press)) be_return_nil(vm);

  const int record = beginRecord(vm, "event");
  if (record < 0) be_return_nil(vm);
  setText(vm, record, "btn", buttonName(button));
  setText(vm, record, "press", pressName(press));
  endRecord(vm);
  be_return(vm);
}

const bnfuncinfo kButtonsLibrary[] = {
    {"take", buttonsTake},
    {nullptr, nullptr},
};

}  // namespace

void installButtonsModule(bvm* vm) { installModule(vm, "buttons", kButtonsLibrary); }

}  // namespace berry
}  // namespace gardener
