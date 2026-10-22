#include <Arduino.h>

#include "app/routines/Routines.h"
#include "app/routines/bindings/Module.h"
#include "app/routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

// The active routine's own values. The fallback is both the answer for a key nothing has written
// and the type the text is read back as, so a routine reads a number as a number and a missing key
// is not an error it has to test for.

int storeGet(bvm* vm) {
  const int top = be_top(vm);
  const bool named = top >= 3;
  const int keyIndex = named ? 2 : 1;
  const int fallbackIndex = named ? 3 : 2;

  if (top < 1 || !be_isstring(vm, 1) || !be_isstring(vm, keyIndex)) {
    be_raise(vm, "type_error", "store.get(key, fallback) or store.get(name, key, fallback)");
  }

  const bool hasFallback = top >= fallbackIndex;
  const bool fallbackInt = hasFallback && be_isint(vm, fallbackIndex);
  const bool fallbackReal = hasFallback && be_isreal(vm, fallbackIndex);
  const bool fallbackBool = hasFallback && be_isbool(vm, fallbackIndex);

  const char* routine = named ? argText(vm, 1) : nullptr;
  const char* key = argText(vm, keyIndex);
  String text;
  const bool stored =
      gRoutines != nullptr &&
      (named ? gRoutines->storeGet(routine, key, text) : gRoutines->storeGet(key, text));

  if (!stored) {
    // Nothing under the key: the fallback the caller passed is the answer, as it stands.
    if (hasFallback) {
      be_pushvalue(vm, fallbackIndex);
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
  const int top = be_top(vm);
  const bool named = top >= 3;
  const int keyIndex = named ? 2 : 1;
  const int valueIndex = named ? 3 : 2;

  if (top < 2 || !be_isstring(vm, 1) || !be_isstring(vm, keyIndex)) {
    be_raise(vm, "type_error", "store.set(key, value) or store.set(name, key, value)");
  }

  const char* routine = named ? argText(vm, 1) : nullptr;
  const char* key = argText(vm, keyIndex);
  // be_tostring converts a number or a bool in place, so every scalar has the text form the file
  // holds. The pointer is into the VM's string table, which is why it is used before anything else
  // is pushed.
  const char* value = be_tostring(vm, valueIndex);
  const bool written =
      value != nullptr && gRoutines != nullptr &&
      (named ? gRoutines->storeSet(routine, key, String(value))
             : gRoutines->storeSet(key, String(value)));
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
