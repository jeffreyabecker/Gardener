#include "app/disconnected-ui/Routines.h"

#include <berry.h>
#include <stdio.h>

#include "app/AppContext.h"
#include "app/Ini.h"
#include "app/Storage.h"
#include "app/StreamLogger.h"
#include "app/disconnected-ui/Bindings.h"

namespace gardener {
namespace {

// The routine object is held as a global as well as in the registry: Berry's collector only walks
// what the VM can see, and a C array is not one of those.
void globalKey(char* out, size_t size, uint8_t slot) {
  snprintf(out, size, "__routine%u", (unsigned int)slot);
}

// A routine has to look like one before it is kept, so a file that registers something else is
// refused at load rather than discovered on the first poll.
bool hasMethod(bvm* vm, int objectIndex, const char* name) {
  const int object = be_absindex(vm, objectIndex);
  if (!be_getmember(vm, object, name)) return false;
  const bool function = be_isfunction(vm, -1);
  be_pop(vm, 1);
  return function;
}

}  // namespace

bool Routines::begin(AppContext& context, Storage& storage, StreamLogger& logger) {
  logger_ = &logger;
  storage_ = &storage;

  vm_ = be_vm_new();
  if (vm_ == nullptr) {
    logger.error("vm: no memory, routines off");
    return false;
  }
  berry::install(vm_, context, *this, logger);

  String files[Storage::kMaxNames];
  const uint8_t found = storage.list("/routines", ".be", files, Storage::kMaxNames);
  uint8_t loaded = 0;
  for (uint8_t i = 0; i < found; i++) {
    if (loadFile(storage, files[i])) loaded++;
  }

  String summary = "vm: ";
  summary += String((uint32_t)count_);
  summary += " routine";
  if (count_ != 1) summary += 's';
  summary += " from ";
  summary += String((uint32_t)loaded);
  summary += " of ";
  summary += String((uint32_t)found);
  summary += " file";
  if (found != 1) summary += 's';
  logger.info(summary.c_str());

  selectDefault();
  return true;
}

// --- loading ---------------------------------------------------------------

bool Routines::loadFile(Storage& storage, const String& file) {
  String path = "/routines/";
  path += file;

  String source;
  if (!storage.read(path.c_str(), source)) {
    String message = "vm: ";
    message += file;
    message += " unreadable";
    logger_->error(message.c_str());
    return false;
  }

  // Every entry records the stack depth it started at: a failure leaves whatever the interpreter
  // put there, and the VM is restored rather than trusted to be balanced.
  const int base = be_top(vm_);
  int rc = be_loadbuffer(vm_, file.c_str(), source.c_str(), source.length());
  if (rc != BE_OK) {
    reportError(file.c_str(), "compile");
    be_pop(vm_, be_top(vm_) - base);
    return false;
  }
  rc = be_pcall(vm_, 0);
  if (rc != BE_OK) {
    reportError(file.c_str(), "load");
    be_pop(vm_, be_top(vm_) - base);
    return false;
  }
  // The top level returns the class it registered; nothing needs it back here.
  be_pop(vm_, be_top(vm_) - base);
  return true;
}

// A failed call leaves two values: the exception and its argument. The argument is the message, and
// both are dropped so a broken file leaves the VM as it found it.
void Routines::reportError(const char* file, const char* what) {
  String message = "vm: ";
  message += file;
  message += ' ';
  message += what;
  message += ": ";
  if (be_isstring(vm_, -2)) {
    message += be_tostring(vm_, -2);
    message += ": ";
  }
  const char* detail = be_tostring(vm_, -1);
  message += detail != nullptr ? detail : "error";
  logger_->error(message.c_str());
  be_pop(vm_, 2);
}

// --- the registry ----------------------------------------------------------

bool Routines::registerRoutine(const char* name, const char* title, int objectIndex) {
  if (name == nullptr || title == nullptr || name[0] == '\0') return false;

  // The name is also the file the routine's values are kept in, so it has to be a plain word
  // rather than something that addresses another path.
  const String named(name);
  if (named.indexOf('/') >= 0 || named.indexOf('\\') >= 0) {
    String message = "vm: routine name ";
    message += name;
    message += " must not contain a path separator";
    logger_->error(message.c_str());
    return false;
  }

  if (!hasMethod(vm_, objectIndex, "activate") || !hasMethod(vm_, objectIndex, "poll")) {
    String message = "vm: ";
    message += name;
    message += " needs activate() and poll()";
    logger_->error(message.c_str());
    return false;
  }

  int8_t slot = -1;
  for (uint8_t i = 0; i < count_; i++) {
    if (entries_[i].name != name) continue;
    slot = (int8_t)i;
    break;
  }
  if (slot < 0) {
    if (count_ == kMaxRoutines) {
      logger_->error("vm: routine registry full, dropping the newest");
      return false;
    }
    slot = (int8_t)count_++;
  } else {
    // A name registered twice is one routine; the later file is the one that runs.
    String message = "vm: routine ";
    message += name;
    message += " replaced";
    logger_->warn(message.c_str());
  }

  entries_[slot].name = name;
  entries_[slot].title = title;

  char key[16];
  globalKey(key, sizeof key, (uint8_t)slot);
  be_pushvalue(vm_, objectIndex);
  be_setglobal(vm_, key);
  be_pop(vm_, 1);  // be_setglobal leaves the value it stored on the stack
  return true;
}

bool Routines::choose(const char* name) {
  if (name == nullptr) return false;
  for (uint8_t i = 0; i < count_; i++) {
    if (entries_[i].name != name) continue;
    active_ = (int8_t)i;
    return true;
  }
  return false;
}

// The rule: `default` is the page, and if it is not registered the mark is.
void Routines::selectDefault() {
  if (choose("default")) {
    logger_->info("vm: default active");
    return;
  }
  logger_->warn("vm: no default routine, showing the mark");
}

bool Routines::setActive(const char* name) {
  if (name == nullptr) return false;
  if (insideCall_) {
    pendingActive_ = name;
    hasPending_ = true;
    return true;
  }
  if (!choose(name)) return false;

  if (!invoke((uint8_t)active_, "activate", false, 0)) {
    active_ = -1;
    return false;
  }
  return true;
}

void Routines::applyPending() {
  if (!hasPending_ || insideCall_) return;
  hasPending_ = false;
  String name = pendingActive_;
  pendingActive_ = "";
  if (setActive(name.c_str())) return;

  // A name that is not registered changes nothing: the routine that was running keeps running.
  String message = "vm: no routine named ";
  message += name;
  logger_->warn(message.c_str());
}

const char* Routines::nameAt(uint8_t index) const {
  return index < count_ ? entries_[index].name.c_str() : "";
}

// --- one entry per beat ----------------------------------------------------

bool Routines::invoke(uint8_t index, const char* method, bool withNow, uint32_t nowMs) {
  if (vm_ == nullptr || index >= count_) return false;

  const int base = be_top(vm_);
  char key[16];
  globalKey(key, sizeof key, index);

  bool ok = false;
  if (!be_getglobal(vm_, key)) {
    logger_->error("vm: routine object missing");
  } else if (!be_getmethod(vm_, -1, method)) {
    String message = "vm: ";
    message += entries_[index].name;
    message += " has no ";
    message += method;
    logger_->error(message.c_str());
  } else {
    be_pushvalue(vm_, -2);  // self
    int argc = 1;
    if (withNow) {
      be_pushint(vm_, (bint)nowMs);
      argc++;
    }
    insideCall_ = true;
    const int rc = be_pcall(vm_, argc);
    insideCall_ = false;
    ok = rc == BE_OK;
    if (!ok) reportError(entries_[index].name.c_str(), method);
  }

  be_pop(vm_, be_top(vm_) - base);
  return ok;
}

void Routines::poll(uint32_t nowMs) {
  if (vm_ == nullptr || active_ < 0) return;
  if (!invoke((uint8_t)active_, "poll", true, nowMs)) {
    // A routine that raised is stopped for good: the page falls back to the mark, and the error is
    // already in the log.
    active_ = -1;
  }
  applyPending();
}

void Routines::enterPage() {
  if (vm_ == nullptr || active_ < 0) return;
  if (!invoke((uint8_t)active_, "activate", false, 0)) active_ = -1;
  applyPending();
}

// --- the press queue -------------------------------------------------------

void Routines::pushPress(Button button, Press press) {
  if (press == Press::None) return;

  if (pressCount_ == kMaxPresses) {
    // The oldest press goes so the newest still arrives, and the log says it once rather than on
    // every drop.
    if (!pressOverflow_) {
      pressOverflow_ = true;
      logger_->warn("vm: button queue full, dropping the oldest");
    }
    pressHead_ = (uint8_t)((pressHead_ + 1) % kMaxPresses);
    pressCount_--;
  }

  const uint8_t tail = (uint8_t)((pressHead_ + pressCount_) % kMaxPresses);
  presses_[tail].button = button;
  presses_[tail].press = press;
  pressCount_++;
}

bool Routines::takePress(Button& button, Press& press) {
  if (pressCount_ == 0) return false;
  button = presses_[pressHead_].button;
  press = presses_[pressHead_].press;
  pressHead_ = (uint8_t)((pressHead_ + 1) % kMaxPresses);
  pressCount_--;
  return true;
}

// --- the active routine's values -------------------------------------------

// Beside the routine rather than in a shared file: a routine's values are its own, a data-folder
// upload can ship initial ones as `/routines/<name>.ini`, and nothing has to be namespaced inside
// the document.
bool Routines::storePath(String& out) const {
  if (active_ < 0) return false;
  out = "/routines/";
  out += entries_[active_].name;
  out += ".ini";
  return true;
}

bool Routines::storeGet(const char* key, String& out) const {
  String path;
  if (storage_ == nullptr || key == nullptr || !storePath(path)) return false;

  String text;
  if (!storage_->read(path.c_str(), text)) return false;

  // The file has no sections, so every key is a global one.
  Ini values;
  values.parse(text);
  if (!values.has("", key)) return false;
  out = values.get("", key, "");
  return true;
}

bool Routines::storeSet(const char* key, const String& value) {
  String path;
  if (storage_ == nullptr || key == nullptr || !storePath(path)) return false;

  String text;
  Ini values;
  if (storage_->read(path.c_str(), text)) values.parse(text);

  // A set that repeats what is already there is not worth a flash write, which is what lets a
  // routine set the same value on every pass without wearing the partition.
  if (values.has("", key) && values.get("", key, "") == value) return true;
  if (!values.set("", key, value)) return false;
  return storage_->write(path.c_str(), values.text());
}

}  // namespace gardener
