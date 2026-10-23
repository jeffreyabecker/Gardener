#include "routines/Routines.h"

#include <berry.h>

#include <vector>

#include "Logger.h"
#include "PanelContext.h"
#include "routines/Bindings.h"
#include "storage/Library.h"
#include "storage/PanelSettings.h"
#include "storage/Values.h"

namespace gardener {
namespace {

// A routine's values are its own document at /routines/<name>.cfg. The name is a plain word: a
// path separator would address another file, so the path is only ever built from a name that has
// none.
bool plainWord(const char* name) {
  if (name == nullptr || name[0] == '\0') return false;
  const String named(name);
  return named.indexOf('/') < 0 && named.indexOf('\\') < 0;
}

// The document a routine's values live in. The name has to be a plain word first, so the path
// cannot escape /routines/.
String routinePath(const char* name) {
  String path = "/routines/";
  path += name;
  path += ".cfg";
  return path;
}

}  // namespace

bool Routines::begin(PanelContext& context) {
  logger_ = &context.logger();
  library_ = &context.library();

  vm_ = be_vm_new();
  if (vm_ == nullptr) {
    logger_->error("vm: no memory, routines off");
    return false;
  }
  berry::install(vm_, context, *this, context.logger());
  stack_.begin(vm_, *logger_);

  // Modules first: a scene may construct a class one of them defines, so those files have to have
  // run before the routine files do.
  size_t moduleFiles = 0;
  const size_t modules = loadAll("modules", moduleFiles);
  size_t routineFiles = 0;
  const size_t routines = loadAll("routines", routineFiles);

  const size_t files = moduleFiles + routineFiles;
  String summary = "vm: ";
  summary += String((uint32_t)modules);
  summary += " module";
  if (modules != 1) summary += 's';
  summary += ", ";
  summary += String((uint32_t)routines);
  summary += " routine";
  if (routines != 1) summary += 's';
  summary += " from ";
  summary += String((uint32_t)files);
  summary += " file";
  if (files != 1) summary += 's';
  logger_->info(summary.c_str());

  stack_.boot();
  return true;
}

bool Routines::showDefault() {
  stack_.showDefault();
  return stack_.active();
}

void Routines::poll(uint32_t nowMs) { stack_.poll(nowMs); }

void Routines::enterPage() { stack_.enterPage(); }

// --- loading ---------------------------------------------------------------

size_t Routines::loadAll(const char* folder, size_t& found) {
  std::vector<String> files;
  if (library_ != nullptr) library_->list(folder, files);
  found = files.size();

  size_t loaded = 0;
  for (const String& file : files) {
    String path = "/";
    path += folder;
    path += '/';
    path += file;
    if (loadFile(path)) loaded++;
  }
  return loaded;
}

bool Routines::loadFile(const String& path) {
  String source;
  if (library_ == nullptr || !library_->read(path, source)) {
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
  // The top level may leave a class or a global it set; nothing needs it back here.
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

// --- the routine's own values ----------------------------------------------

bool Routines::storeGet(const char* routine, const char* key, String& out) {
  if (!plainWord(routine) || key == nullptr || library_ == nullptr) return false;

  String text;
  if (!library_->read(routinePath(routine), text)) return false;

  Values values;
  values.parse(text);
  if (!values.has(key)) return false;
  out = values.get(key, "");
  return true;
}

bool Routines::storeSet(const char* routine, const char* key, const String& value) {
  if (!plainWord(routine) || key == nullptr || library_ == nullptr) return false;

  String text;
  Values values;
  if (library_->read(routinePath(routine), text)) values.parse(text);

  // A repeat value is not worth a flash write.
  if (values.has(key) && values.get(key, "") == value) return true;
  if (!values.set(key, value)) return false;
  return library_->write(routinePath(routine), values.text());
}

}  // namespace gardener
