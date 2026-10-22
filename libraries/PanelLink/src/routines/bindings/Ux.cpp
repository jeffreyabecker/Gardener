#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

// Pushing a scene puts it on top of the stack and makes it the one that is polled. The args, when
// there is a map, are that activation's alone: they go into activate() and are forgotten. The
// callback, when there is one, receives the value the scene returns when it is popped.
int uxPush(bvm* vm) {
  const int top = be_top(vm);
  if (top < 1 || !be_isinstance(vm, 1) ||
      (top >= 2 && !be_ismapinstance(vm, 2) && !be_isnil(vm, 2)) ||
      (top >= 3 && !be_isfunction(vm, 3) && !be_isnil(vm, 3))) {
    be_raise(vm, "type_error", "ux.push(scene, args, callback)");
  }
  const int args = top >= 2 && be_ismapinstance(vm, 2) ? 2 : 0;
  const int callback = top >= 3 && be_isfunction(vm, 3) ? 3 : 0;
  be_pushbool(vm, gRoutines->push(1, args, callback));
  be_return(vm);
}

// A scene leaves by popping itself; the value it hands back goes to the scene below's callback, or
// a re-activation when that scene was pushed without one.
int uxPop(bvm* vm) {
  const int valueIndex = be_top(vm) >= 1 ? 1 : 0;
  be_pushbool(vm, gRoutines->pop(valueIndex));
  be_return(vm);
}

// Back to the root in one move: every scene above the root is popped and the root re-activates.
int uxPopToRoot(bvm* vm) {
  const int valueIndex = be_top(vm) >= 1 ? 1 : 0;
  be_pushbool(vm, gRoutines->popToRoot(valueIndex));
  be_return(vm);
}

int uxTop(bvm* vm) {
  if (gRoutines == nullptr || !gRoutines->top()) be_return_nil(vm);
  be_return(vm);
}

int uxDepth(bvm* vm) {
  be_pushint(vm, gRoutines != nullptr ? (bint)gRoutines->depth() : 0);
  be_return(vm);
}

const bnfuncinfo kUxLibrary[] = {
    {"push", uxPush},
    {"pop", uxPop},
    {"pop_to_root", uxPopToRoot},
    {"top", uxTop},
    {"depth", uxDepth},
    {nullptr, nullptr},
};

}  // namespace

void installUxModule(bvm* vm) { installModule(vm, "ux", kUxLibrary); }

}  // namespace berry
}  // namespace gardener
