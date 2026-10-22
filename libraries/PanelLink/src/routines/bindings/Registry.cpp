#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

int routinesRegister(bvm* vm) {
  const int top = be_top(vm);
  if (top < 3 || !be_isstring(vm, 1) || !be_isstring(vm, 2) || !be_isinstance(vm, 3) ||
      (top >= 4 && !be_isbool(vm, 4) && !be_isnil(vm, 4)) ||
      (top >= 5 && !be_isinstance(vm, 5) && !be_isnil(vm, 5))) {
    be_raise(vm, "type_error", "routines.register(name, title, routine, hidden, icon)");
  }
  // The fourth argument is optional and defaults to false: a routine only leaves the browser's
  // list when its file says so. The fifth is the icon object the browser draws before the name.
  const bool hidden = top >= 4 && be_isbool(vm, 4) && be_tobool(vm, 4);
  const int iconIndex = top >= 5 && be_isinstance(vm, 5) ? 5 : 0;
  const bool ok =
      gRoutines->registerRoutine(argText(vm, 1), argText(vm, 2), 3, hidden, iconIndex);
  be_pushbool(vm, ok);
  be_return(vm);
}

int routinesSetActive(bvm* vm) {
  const int top = be_top(vm);
  if (top < 1 || !be_isstring(vm, 1) ||
      (top >= 2 && !be_ismap(vm, 2) && !be_isnil(vm, 2))) {
    be_raise(vm, "type_error", "routines.set_active(name, settings)");
  }
  // A nil means the same as leaving the argument out: the activation carries no settings, and the
  // routine reads its file through store.
  const int settingsIndex = top >= 2 && be_ismap(vm, 2) ? 2 : 0;
  const bool ok = gRoutines->setActive(argText(vm, 1), settingsIndex);
  be_pushbool(vm, ok);
  be_return(vm);
}

int routinesNames(bvm* vm) {
  // A list a routine can ask the size of and index, which means an instance of the `list` class: the
  // entries go into the class's own backing list and that is dropped, exactly as for a record.
  // Hidden routines are left out: this is the browser's list.
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (size_t i = 0; i < gRoutines->count(); i++) {
    if (gRoutines->hiddenAt(i)) continue;
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

// The icon the routine registered, or nil when it has none: the browser puts it before the name.
int routinesIcon(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1) || gRoutines == nullptr ||
      !gRoutines->pushIcon(argText(vm, 1))) {
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
    {"icon", routinesIcon},
    {"has", routinesHas},
    {nullptr, nullptr},
};

}  // namespace

void installRoutinesModule(bvm* vm) { installModule(vm, "routines", kRoutinesLibrary); }

}  // namespace berry
}  // namespace gardener
