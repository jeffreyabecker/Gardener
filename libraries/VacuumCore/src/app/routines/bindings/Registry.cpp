#include "app/routines/Routines.h"
#include "app/routines/bindings/Module.h"
#include "app/routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

int routinesRegister(bvm* vm) {
  if (be_top(vm) < 3 || !be_isstring(vm, 1) || !be_isstring(vm, 2) || !be_isinstance(vm, 3)) {
    be_raise(vm, "type_error", "routines.register(name, title, routine)");
  }
  const bool ok = gRoutines->registerRoutine(argText(vm, 1), argText(vm, 2), 3);
  be_pushbool(vm, ok);
  be_return(vm);
}

int routinesSetActive(bvm* vm) {
  const bool ok = be_isstring(vm, 1) && gRoutines->setActive(argText(vm, 1));
  be_pushbool(vm, ok);
  be_return(vm);
}

int routinesNames(bvm* vm) {
  // A list a routine can ask the size of and index, which means an instance of the `list` class: the
  // entries go into the class's own backing list and that is dropped, exactly as for a record.
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (size_t i = 0; i < gRoutines->count(); i++) {
    be_pushstring(vm, gRoutines->nameAt(i));
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

// The registered object itself, so a page can call one of its methods in its own frame. Returning
// the instance rather than calling describe() here is what keeps the runtime from re-entering the
// interpreter inside a native call.
int routinesGet(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1) || gRoutines == nullptr ||
      !gRoutines->pushObject(argText(vm, 1))) {
    be_return_nil(vm);
  }
  be_return(vm);
}

int routinesHas(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "routines.has(name, method)");
  }
  const bool found = gRoutines != nullptr && gRoutines->hasMethod(argText(vm, 1), argText(vm, 2));
  be_pushbool(vm, found);
  be_return(vm);
}

const bnfuncinfo kRoutinesLibrary[] = {
    {"register", routinesRegister},
    {"set_active", routinesSetActive},
    {"names", routinesNames},
    {"get", routinesGet},
    {"has", routinesHas},
    {nullptr, nullptr},
};

}  // namespace

void installRoutinesModule(bvm* vm) { installModule(vm, "routines", kRoutinesLibrary); }

}  // namespace berry
}  // namespace gardener
