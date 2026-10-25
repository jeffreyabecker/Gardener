#pragma once

#include <berry.h>

namespace gardener {

class PanelContext;
class Routines;
class Logger;

namespace berry {

// One VM and one app, so the native callbacks -- plain C functions with no user data -- reach their
// owners through these. install() is the only writer.
extern PanelContext* context;
extern Routines* routines;
extern Logger* logger;

// --- building a module -----------------------------------------------------

inline void installModule(bvm* vm, const char* name, const bnfuncinfo* library) {
  be_newmodule(vm);
  const int module = be_absindex(vm, -1);
  for (const bnfuncinfo* entry = library; entry->name != nullptr; entry++) {
    be_pushntvfunction(vm, entry->function);
    // be_setmember copies into the module and leaves the value where it was, so the push is
    // undone here. Passing the absolute index keeps it valid across the push.
    be_setmember(vm, module, entry->name);
    be_pop(vm, 1);
  }
  be_setglobal(vm, name);
  // be_setglobal leaves the value it stored on the stack; the module is done with here.
  be_pop(vm, 1);
}

// --- filling a record ------------------------------------------------------
//
// A record is an instance of one of the classes Bindings.cpp compiles at boot: only the parser can
// declare an instance variable, so the classes are written in Berry and these fill what they make.

inline int beginRecord(bvm* vm, const char* className) {
  if (!be_getglobal(vm, className)) return -1;
  be_call(vm, 0);
  return be_absindex(vm, -1);
}

// The class value stays under the instance, so this removes the class and leaves the finished
// record for be_return().
inline void endRecord(bvm* vm) { be_remove(vm, -2); }

inline void setReal(bvm* vm, int record, const char* field, breal value) {
  be_pushreal(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

inline void setInt(bvm* vm, int record, const char* field, bint value) {
  be_pushint(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

inline void setText(bvm* vm, int record, const char* field, const char* value) {
  be_pushstring(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

inline void setFlag(bvm* vm, int record, const char* field, bool value) {
  be_pushbool(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

// --- argument helpers ------------------------------------------------------

inline breal argReal(bvm* vm, int index) { return be_toreal(vm, index); }
inline bint argInt(bvm* vm, int index) { return be_toint(vm, index); }
inline const char* argText(bvm* vm, int index) { return be_tostring(vm, index); }

}  // namespace berry
}  // namespace gardener
