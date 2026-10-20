#include "app/routines/Routines.h"

#include <berry.h>
#include <stdio.h>

#include "app/AppContext.h"
#include "app/Logger.h"
#include "app/routines/Bindings.h"
#include "app/storage/Ini.h"
#include "app/storage/Storage.h"

namespace gardener {
namespace {

// The routine object is held as a global as well as in the registry: Berry's collector only walks
// what the VM can see, and a std::vector is not one of those.
void globalKey(char* out, size_t size, size_t slot) {
  snprintf(out, size, "__routine%u", (unsigned int)slot);
}

// A routine has to look like one before it is kept, so a file that registers something else is
// refused at load rather than discovered on the first poll.
bool objectHasMethod(bvm* vm, int objectIndex, const char* name) {
  const int object = be_absindex(vm, objectIndex);
  const bool present = be_getmember(vm, object, name);
  const bool function = present && be_isfunction(vm, -1);
  be_pop(vm, 1);  // be_getmember always leaves a slot, nil when the member is not there
  return function;
}

}  // namespace

bool Routines::begin(AppContext& context, Storage& storage, Logger& logger) {
  logger_ = &logger;
  storage_ = &storage;

  vm_ = be_vm_new();
  if (vm_ == nullptr) {
    logger.error("vm: no memory, routines off");
    return false;
  }
  berry::install(vm_, context, *this, logger);

  // Modules first: a page may construct a class one of them defines, so those files have to have
  // run before the pages do.
  size_t moduleFiles = 0;
  const size_t modules = loadAll(storage, "/modules", moduleFiles);
  size_t routineFiles = 0;
  const size_t pages = loadAll(storage, "/routines", routineFiles);

  const size_t files = moduleFiles + routineFiles;
  String summary = "vm: ";
  summary += String((uint32_t)entries_.size());
  summary += " routine";
  if (entries_.size() != 1) summary += 's';
  if (modules > 0) {
    summary += ", ";
    summary += String((uint32_t)modules);
    summary += " module";
    if (modules != 1) summary += 's';
  }
  summary += " from ";
  summary += String((uint32_t)(modules + pages));
  summary += " of ";
  summary += String((uint32_t)files);
  summary += " file";
  if (files != 1) summary += 's';
  logger.info(summary.c_str());

  selectDefault();
  return true;
}

// The page's way out: whichever routine is active is left, and `default` takes the panel again. The
// files are not read again — everything was loaded at boot and is still registered, so what changes is
// which routine is showing. Anything queued for the page being left goes with it.
bool Routines::showDefault() {
  pressHead_ = 0;
  pressCount_ = 0;
  pressOverflow_ = false;
  hasPending_ = false;
  pendingActive_ = "";

  active_ = -1;
  if (!choose("default")) {
    logger_->warn("vm: no default routine, showing the mark");
    return false;
  }
  return startActive();
}

// --- loading ---------------------------------------------------------------

size_t Routines::loadAll(Storage& storage, const char* dir, size_t& found) {
  std::vector<String> files;
  storage.list(dir, ".be", files);
  found = files.size();

  size_t loaded = 0;
  for (const String& file : files) {
    String path = dir;
    path += '/';
    path += file;
    if (loadFile(storage, path)) loaded++;
  }
  return loaded;
}

bool Routines::loadFile(Storage& storage, const String& path) {
  String source;
  if (!storage.read(path.c_str(), source)) {
    String message = "vm: ";
    message += path;
    message += " unreadable";
    logger_->error(message.c_str());
    return false;
  }

  // Every entry records the stack depth it started at: a failure leaves whatever the interpreter
  // put there, and the VM is restored rather than trusted to be balanced.
  const int base = be_top(vm_);
  int rc = be_loadbuffer(vm_, path.c_str(), source.c_str(), source.length());
  if (rc != BE_OK) {
    reportError(path.c_str(), "compile");
    be_pop(vm_, be_top(vm_) - base);
    return false;
  }
  rc = be_pcall(vm_, 0);
  if (rc != BE_OK) {
    reportError(path.c_str(), "load");
    be_pop(vm_, be_top(vm_) - base);
    return false;
  }
  // The top level may leave the class it registered or defined; nothing needs it back here.
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

  if (!objectHasMethod(vm_, objectIndex, "activate") ||
      !objectHasMethod(vm_, objectIndex, "poll")) {
    String message = "vm: ";
    message += name;
    message += " needs activate() and poll()";
    logger_->error(message.c_str());
    return false;
  }

  int slot = -1;
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].name != name) continue;
    slot = (int)i;
    break;
  }
  if (slot < 0) {
    slot = (int)entries_.size();
    entries_.push_back(Entry());
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
  globalKey(key, sizeof key, (size_t)slot);
  be_pushvalue(vm_, objectIndex);
  be_setglobal(vm_, key);
  be_pop(vm_, 1);  // be_setglobal leaves the value it stored on the stack
  return true;
}

bool Routines::choose(const char* name) {
  if (name == nullptr) return false;
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].name != name) continue;
    active_ = (int)i;
    // A stop notice belongs to the page that was showing when the hold fired; a switch drops one
    // that page never took rather than handing it to the next one.
    stopped_ = false;
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
  return startActive();
}

// Runs the chosen routine's activate(), which is both what makes it the visible page and what lets it
// draw its first frame. A routine that raises here is dropped, exactly as one that raises in poll().
bool Routines::startActive() {
  if (active_ < 0) return false;
  if (invoke((size_t)active_, "activate", false, 0)) return true;
  active_ = -1;
  return false;
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

const char* Routines::nameAt(size_t index) const {
  return index < entries_.size() ? entries_[index].name.c_str() : "";
}

bool Routines::pushObject(const char* name) const {
  if (vm_ == nullptr || name == nullptr) return false;
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].name != name) continue;
    char key[16];
    globalKey(key, sizeof key, i);
    const bool found = be_getglobal(vm_, key);
    // be_getglobal pushes a nil when the name is not there; callers only want a stack slot when the
    // object was found.
    if (!found) be_pop(vm_, 1);
    return found;
  }
  return false;
}

// The object is left to the caller's Berry frame to call. Looking a method up here is only a read:
// the runtime still never enters the interpreter from a native call.
bool Routines::hasMethod(const char* name, const char* method) const {
  if (method == nullptr || !pushObject(name)) return false;
  const int object = be_absindex(vm_, -1);
  const bool present = be_getmember(vm_, object, method);
  const bool function = present && be_isfunction(vm_, -1);
  be_pop(vm_, 2);  // the member slot be_getmember always pushes, then the object
  return function;
}

// --- one entry per beat ----------------------------------------------------

bool Routines::invoke(size_t index, const char* method, bool withNow, uint32_t nowMs) {
  if (vm_ == nullptr || index >= entries_.size()) return false;

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
  if (!invoke((size_t)active_, "poll", true, nowMs)) {
    // A routine that raised is stopped for good: the page falls back to the mark, and the error is
    // already in the log.
    active_ = -1;
  }
  applyPending();
}

void Routines::enterPage() {
  if (vm_ == nullptr || active_ < 0) return;
  if (!invoke((size_t)active_, "activate", false, 0)) active_ = -1;
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

// The Back hold belongs to the device, not to the page, so the page is told after the fact. The
// notice waits for the page's poll: the loop may fire the hold while a controller owns the device,
// and the page that was showing is the one that has to hear about it when it resumes.
void Routines::notifyStopped() { stopped_ = true; }

bool Routines::takeStopped() {
  const bool stopped = stopped_;
  stopped_ = false;
  return stopped;
}

// --- the active routine's values -------------------------------------------

// Beside the routine rather than in a shared file: a routine's values are its own, a data-folder
// upload can ship initial ones as `/routines/<name>.ini`, and nothing has to be namespaced inside
// the document.
bool Routines::storePath(const char* routine, String& out) const {
  if (routine == nullptr) return false;

  // Only a registered name is a path: a caller that names another routine cannot address a file
  // that is not one of the pages the device loaded.
  bool registered = false;
  for (const Entry& entry : entries_) {
    if (entry.name == routine) {
      registered = true;
      break;
    }
  }
  if (!registered) return false;

  out = "/routines/";
  out += routine;
  out += ".ini";
  return true;
}

bool Routines::storeGet(const char* key, String& out) const {
  if (active_ < 0) return false;
  return storeGet(entries_[active_].name.c_str(), key, out);
}

bool Routines::storeSet(const char* key, const String& value) {
  if (active_ < 0) return false;
  return storeSet(entries_[active_].name.c_str(), key, value);
}

bool Routines::storeGet(const char* routine, const char* key, String& out) const {
  String path;
  if (storage_ == nullptr || key == nullptr || !storePath(routine, path)) return false;

  String text;
  if (!storage_->read(path.c_str(), text)) return false;

  // The file has no sections, so every key is a global one.
  Ini values;
  values.parse(text);
  if (!values.has("", key)) return false;
  out = values.get("", key, "");
  return true;
}

bool Routines::storeSet(const char* routine, const char* key, const String& value) {
  String path;
  if (storage_ == nullptr || key == nullptr || !storePath(routine, path)) return false;

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
