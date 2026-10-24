#include <Arduino.h>

#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

// A routine's own values, addressed by the name of the document they live in. The fallback is
// both the answer for a key nothing has written and the type the text is read back as, so a
// routine reads a number as a number and a missing key is not an error it has to test for.

int storeGet(bvm* vm) {
  if (be_top(vm) < 3 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "store.get(name, key, fallback)");
  }
  const bool hasFallback = be_top(vm) >= 3;
  const bool fallbackInt = hasFallback && be_isint(vm, 3);
  const bool fallbackReal = hasFallback && be_isreal(vm, 3);
  const bool fallbackBool = hasFallback && be_isbool(vm, 3);

  const char* document = argText(vm, 1);
  const char* key = argText(vm, 2);
  String text;
  const bool stored = gRoutines != nullptr && gRoutines->storeGet(document, key, text);

  if (!stored) {
    // Nothing under the key: the fallback the caller passed is the answer, as it stands.
    if (hasFallback) {
      be_pushvalue(vm, 3);
    } else {
      be_pushnil(vm);
    }
    be_return(vm);
  }

  if (fallbackInt) {
    be_pushint(vm, (bint)text.toInt());
  } else if (fallbackReal) {
    be_pushreal(vm, (breal)text.toFloat());
  } else if (fallbackBool) {
    be_pushbool(vm, text == "true" || text == "1");
  } else {
    be_pushstring(vm, text.c_str());
  }
  be_return(vm);
}

int storeSet(bvm* vm) {
  if (be_top(vm) < 3 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "store.set(name, key, value)");
  }

  const char* document = argText(vm, 1);
  const char* key = argText(vm, 2);
  // be_tostring converts a number or a bool in place, so every scalar has the text form the file
  // holds. The pointer is into the VM's string table, which is why it is used before anything else
  // is pushed.
  const char* value = be_tostring(vm, 3);
  const bool written =
      value != nullptr && gRoutines != nullptr &&
      gRoutines->storeSet(document, key, String(value));
  be_pushbool(vm, written);
  be_return(vm);
}

const bnfuncinfo kStoreLibrary[] = {
    {"get", storeGet},
    {"set", storeSet},
    {nullptr, nullptr},
};

}  // namespace

void installStoreModule(bvm* vm) { installModule(vm, "store", kStoreLibrary); }

}  // namespace berry
}  // namespace gardener
