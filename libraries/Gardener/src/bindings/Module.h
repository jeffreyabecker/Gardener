#pragma once

#include <berry.h>

namespace gardener {
namespace bindings {

class BindingContext;

// --- the app a callback reaches --------------------------------------------
//
// A callback is a plain C function with no user data, so the app it reaches is the one upvalue of
// the native closure it was installed as: installModule() and pushClass() capture it, and a
// callback asks boundContext() for it. Nothing names a global, so the modules of one VM cannot
// answer with the parts of another.

// Captures the app in the native closure on top of the stack.
inline void captureBindingContext(bvm* vm, BindingContext& bindings) {
  const int closure = be_absindex(vm, -1);
  be_pushcomptr(vm, &bindings);
  // be_setupval copies the top into the upvalue and leaves it there, so the push is undone here.
  be_setupval(vm, closure, 0);
  be_pop(vm, 1);
}

// The app the running callback was installed with, and nothing for one installed without it. Only a
// callback's own frame has the closure to read it from: a C++ helper is handed the pointer by its
// caller.
inline BindingContext* boundContext(bvm* vm) {
  be_getupval(vm, 0, 0);
  BindingContext* bindings =
      be_iscomptr(vm, -1) ? static_cast<BindingContext*>(be_tocomptr(vm, -1)) : nullptr;
  be_pop(vm, 1);
  return bindings;
}

// --- building a module -----------------------------------------------------

inline void installModule(bvm* vm, const char* name, const bnfuncinfo* library,
                          BindingContext& bindings) {
  be_newmodule(vm);
  const int module = be_absindex(vm, -1);
  for (const bnfuncinfo* entry = library; entry->name != nullptr; entry++) {
    be_pushntvclosure(vm, entry->function, 1);
    captureBindingContext(vm, bindings);
    // be_setmember copies into the module and leaves the value where it was, so the push is
    // undone here. Passing the absolute index keeps it valid across the push.
    be_setmember(vm, module, entry->name);
    be_pop(vm, 1);
  }
  be_setglobal(vm, name);
  // be_setglobal leaves the value it stored on the stack; the module is done with here.
  be_pop(vm, 1);
}

// --- declaring a record ----------------------------------------------------
//
// A record is a class whose fields are all it has, and the module whose callbacks fill it declares it:
// an entry of the table with no function is an instance variable rather than a method, which is the
// same class the parser's `var` makes. The names a page reads sit beside the code that sets them, and
// the record is a global because that code reaches it by name.

// The constructor a record needs before it can be built by name. A class with no `init` cannot be
// constructed at all — the VM finds no constructor and the call leaves the stack as it found it — so a
// table whose callbacks set every field starts with this one, which answers the instance the call
// made. A record with fields to take from the call has an init of its own.
inline int recordInit(bvm* vm) {
  be_pushvalue(vm, 1);
  be_return(vm);
}

// What a class call left on the stack, from C: a class answers in the slot its callable had, with the
// arguments it shifted above it. Dropping one value per argument leaves that answer on top, which is
// the instance the class built — the same value a page reads from a Berry call, and not the init's
// own return.
inline int settleClassCall(bvm* vm, int argc) {
  be_pop(vm, argc);
  return be_absindex(vm, -1);
}

// The methods a class is pushed with arrive as plain native functions, which capture nothing; each
// is replaced here by a closure over the app the module's own functions hold, so a record's init
// can reach it. The entries with no function are the instance variables, and are left as they are.
inline void bindMethods(bvm* vm, int record, const bnfuncinfo* fields, BindingContext& bindings) {
  for (const bnfuncinfo* entry = fields; entry->name != nullptr; entry++) {
    if (entry->function == nullptr) continue;
    be_pushntvclosure(vm, entry->function, 1);
    captureBindingContext(vm, bindings);
    be_setmember(vm, record, entry->name);
    be_pop(vm, 1);
  }
}

// Pushes the class `fields` declares, every method bound to the app, and leaves it on top.
inline void pushClass(bvm* vm, const char* name, const bnfuncinfo* fields,
                      BindingContext& bindings) {
  be_pushclass(vm, name, fields);
  bindMethods(vm, be_absindex(vm, -1), fields, bindings);
}

inline void installRecord(bvm* vm, const char* name, const bnfuncinfo* fields,
                          BindingContext& bindings) {
  pushClass(vm, name, fields, bindings);
  be_setglobal(vm, name);
  // be_setglobal leaves the value it stored on the stack; the record is done with here.
  be_pop(vm, 1);
}

// --- filling a record ------------------------------------------------------

inline int beginRecord(bvm* vm, const char* className) {
  be_getglobal(vm, className);
  // The class the call is made on is replaced by the instance it makes, so the record is on top of
  // the stack for the set calls below and for be_return().
  be_call(vm, 0);
  return be_absindex(vm, -1);
}

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

// The value at `index` as the record's field, for one that is not a plain number or string: a list
// of children, or a record the caller passed in.
inline void setValue(bvm* vm, int record, const char* field, int index) {
  be_pushvalue(vm, index);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

// --- argument helpers ------------------------------------------------------

inline breal argReal(bvm* vm, int index) { return be_toreal(vm, index); }
inline bint argInt(bvm* vm, int index) { return be_toint(vm, index); }
inline const char* argText(bvm* vm, int index) { return be_tostring(vm, index); }

}  // namespace bindings
}  // namespace gardener
