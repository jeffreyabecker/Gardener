#include <Arduino.h>

#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Values.h"

namespace gardener {
namespace berry {
namespace {

// A routine's own values, addressed by the name of the document they live in. The fallback is
// both the answer for a key nothing has written and the type the text is read back as, so a
// routine reads a number as a number and a missing key is not an error it has to test for. A map
// of defaults is the same rule for a whole document: each entry's type reads its own key.
//
// One key is one value; a map is the whole document, so a page that reads or writes every value
// pays one file read and at most one write.

// The type the stored text is read back as: the type of the fallback the caller passed.
enum class Kind { Int, Real, Bool, Text };

Kind kindOf(bvm* vm, int index) {
  if (be_isint(vm, index)) return Kind::Int;
  if (be_isreal(vm, index)) return Kind::Real;
  if (be_isbool(vm, index)) return Kind::Bool;
  return Kind::Text;
}

void pushTyped(bvm* vm, Kind kind, const String& text) {
  switch (kind) {
    case Kind::Int:
      be_pushint(vm, (bint)text.toInt());
      break;
    case Kind::Real:
      be_pushreal(vm, (breal)text.toFloat());
      break;
    case Kind::Bool:
      be_pushbool(vm, text == "true" || text == "1");
      break;
    case Kind::Text:
      be_pushstring(vm, text.c_str());
      break;
  }
}

// A fresh map as the answer, leaving its internal table on top: the insert calls fill that table,
// and dropping it again leaves the map itself, which is what the caller asked for.
void pushMap(bvm* vm) { be_newobject(vm, "map"); }

// The internal table of a map, which is what the iterator and the insert calls work on. It is
// pushed, so the caller drops it when it is done.
int pushMapData(bvm* vm, int map) {
  be_getmember(vm, map, ".p");
  return be_absindex(vm, -1);
}

// store.get(name): every value the document holds, each as the text the file was written with.
void pushAllValues(bvm* vm, const char* document) {
  Values held;
  if (routines != nullptr) routines->storeGet(document, held);

  pushMap(vm);
  const int data = be_absindex(vm, -1);
  for (uint8_t i = 0; i < held.count(); i++) {
    be_pushstring(vm, held.keyAt(i).c_str());
    be_pushstring(vm, held.valueAt(i).c_str());
    be_data_insert(vm, data);
    be_pop(vm, 2);
  }
  be_pop(vm, 1);
}

// store.get(name, defaults): one entry per key the map names, read back as that key's type, or the
// default itself when the document holds nothing for it. The document is read once, not per key.
void pushDefaultedValues(bvm* vm, const char* document, int defaults) {
  Values held;
  const bool stored = routines != nullptr && routines->storeGet(document, held);

  pushMap(vm);
  const int data = be_absindex(vm, -1);
  const int source = pushMapData(vm, defaults);
  const bool iterating = be_ismap(vm, source) && be_pushiter(vm, source);
  while (iterating && be_iter_hasnext(vm, source)) {
    be_iter_next(vm, source);
    const Kind kind = kindOf(vm, -1);
    // The key points into the VM's string table, so it is copied before anything else is pushed.
    const char* keyText = be_tostring(vm, -2);
    if (keyText == nullptr) {
      be_pop(vm, 2);
      continue;
    }
    const String key(keyText);
    if (stored && held.has(key.c_str())) {
      // The stored text stands in for the default, one slot for another.
      const String text = held.get(key.c_str(), "");
      be_pop(vm, 1);
      pushTyped(vm, kind, text);
    } else {
      // The default is the answer as it stands: a copy above it is the value to insert.
      be_pushvalue(vm, -1);
      be_remove(vm, -2);
    }
    be_data_insert(vm, data);
    be_pop(vm, 2);
  }
  // The iterator, the defaults' table and the new map's own, leaving the new map.
  be_pop(vm, iterating ? 3 : 2);
}

// store.set(name, values): every entry the map names, merged and written at most once.
void setAllValues(bvm* vm, const char* document, int values) {
  Values merged;
  bool written = routines != nullptr;
  const int source = pushMapData(vm, values);
  const bool iterating = be_ismap(vm, source) && be_pushiter(vm, source);
  while (iterating && be_iter_hasnext(vm, source)) {
    be_iter_next(vm, source);
    // Both point into the VM's string table, so they are copied before anything else is used.
    const char* keyText = be_tostring(vm, -2);
    const char* text = be_tostring(vm, -1);
    if (keyText == nullptr || text == nullptr || !merged.set(keyText, String(text))) {
      written = false;
    }
    be_pop(vm, 2);
  }
  // The iterator, when one was pushed, and the map's own table.
  be_pop(vm, iterating ? 2 : 1);

  if (written) written = routines->storeSet(document, merged);
  be_pushbool(vm, written);
}

int storeGet(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "store.get(name, key, fallback)");
  }
  const String document(argText(vm, 1));

  if (be_top(vm) < 2) {
    pushAllValues(vm, document.c_str());
    be_return(vm);
  }
  if (be_ismapinstance(vm, 2)) {
    pushDefaultedValues(vm, document.c_str(), be_absindex(vm, 2));
    be_return(vm);
  }
  if (be_top(vm) < 3 || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "store.get(name, key, fallback)");
  }

  const Kind kind = kindOf(vm, 3);
  const char* key = argText(vm, 2);
  String text;
  const bool stored = routines != nullptr && routines->storeGet(document.c_str(), key, text);

  if (!stored) {
    // Nothing under the key: the fallback the caller passed is the answer, as it stands.
    be_pushvalue(vm, 3);
    be_return(vm);
  }
  pushTyped(vm, kind, text);
  be_return(vm);
}

int storeSet(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "store.set(name, key, value)");
  }
  const String document(argText(vm, 1));

  if (be_ismapinstance(vm, 2)) {
    setAllValues(vm, document.c_str(), be_absindex(vm, 2));
    be_return(vm);
  }
  if (be_top(vm) < 3 || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "store.set(name, key, value)");
  }

  const char* key = argText(vm, 2);
  // be_tostring converts a number or a bool in place, so every scalar has the text form the file
  // holds. The pointer is into the VM's string table, which is why it is used before anything else
  // is pushed.
  const char* value = be_tostring(vm, 3);
  const bool written = value != nullptr && routines != nullptr &&
                       routines->storeSet(document.c_str(), key, String(value));
  be_pushbool(vm, written);
  be_return(vm);
}

const bnfuncinfo StoreLibrary[] = {
    {"get", storeGet},
    {"set", storeSet},
    {nullptr, nullptr},
};

}  // namespace

void installStoreModule(bvm* vm) { installModule(vm, "store", StoreLibrary); }

}  // namespace berry
}  // namespace gardener
