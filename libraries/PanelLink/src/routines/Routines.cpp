#include "routines/Routines.h"

#include <berry.h>
#include <stddef.h>

#include <vector>

#include "Logger.h"
#include "PanelContext.h"
#include "routines/Bindings.h"
#include "storage/Library.h"
#include "storage/PanelSettings.h"
#include "storage/Values.h"

namespace gardener {
namespace {

// The scene stack every page calls is `/system/ux.be`'s global `ux`: the frames, the callbacks and
// the one-beat deferral are Berry's, and this side only makes the calls that a beat needs.
constexpr const char* kUxGlobal = "ux";
constexpr const char* kRootScene = "__rootScene";

// A document's values live at /routines/<document>.cfg, so the document has to be a plain word: a
// path separator would address another file, and the path is only ever built from a name that has
// none.
bool plainWord(const char* name) {
  if (name == nullptr || name[0] == '\0') return false;
  const String named(name);
  return named.indexOf('/') < 0 && named.indexOf('\\') < 0;
}

// The file a document's values live in. The document has to be a plain word first, so the path
// cannot escape /routines/.
String documentPath(const char* document) {
  String path = "/routines/";
  path += document;
  path += ".cfg";
  return path;
}

// The system is the files the interpreter needs before anything else: the records a native call
// answers with, the components and the scene every page is written against, and the registry. Its
// order is not the folder's: Berry resolves a global when it compiles a file, so a class has to be
// a global by the time a file that names it is compiled. `/system/order` states that order, one
// path per line relative to `/system`.
constexpr const char* kSystemFolder = "system";
constexpr const char* kSystemOrder = "/system/order";

// A file dropped into a system folder and forgotten in the order would otherwise be a class that
// quietly never exists, so every `.be` beside the ones the order names is reported. The folders are
// the ones the named paths are in, `/system` itself included.
void reportUnlisted(Library& library, Logger& logger, const std::vector<String>& listed) {
  std::vector<String> folders;
  folders.push_back("");
  for (const String& path : listed) {
    const int slash = path.lastIndexOf('/');
    if (slash < 0) continue;
    const String folder = path.substring(0, slash);
    bool known = false;
    for (const String& seen : folders) {
      if (seen == folder) known = true;
    }
    if (!known) folders.push_back(folder);
  }

  for (const String& folder : folders) {
    String path = kSystemFolder;
    if (!folder.isEmpty()) {
      path += '/';
      path += folder;
    }
    std::vector<String> files;
    library.list(path, files);
    for (const String& file : files) {
      const String key = folder.isEmpty() ? file : folder + "/" + file;
      bool named = false;
      for (const String& entry : listed) {
        if (entry == key) named = true;
      }
      if (named) continue;
      String message = "vm: /system/";
      message += key;
      message += " is not in /system/order";
      logger.error(message.c_str());
    }
  }
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
  gestures_.begin(*logger_);

  // The system first, in the order `/system/order` states: a scene may construct a class one of
  // those files defines — or answer with a record one of them declares — so they have to have run
  // before the module and routine files do.
  size_t systemFiles = 0;
  const size_t system = loadSystem(systemFiles);
  size_t moduleFiles = 0;
  const size_t modules = loadAll("modules", moduleFiles);
  size_t routineFiles = 0;
  const size_t routines = loadAll("routines", routineFiles);

  const size_t files = systemFiles + moduleFiles + routineFiles;
  String summary = "vm: ";
  summary += String((uint32_t)system);
  summary += " system, ";
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

  resetToRoot();
  return true;
}

bool Routines::showDefault() {
  gestures_.clear();
  return resetToRoot();
}

bool Routines::poll(uint32_t nowMs) {
  const int base = be_top(vm_);
  be_pushint(vm_, (bint)nowMs);
  const bool showing = callUx("poll", be_absindex(vm_, -1));
  be_pop(vm_, be_top(vm_) - base);
  return showing;
}

bool Routines::enterPage() { return callUx("enter", 0); }

bool Routines::active() const { return callUx("depth", 0); }

// The root scene, as a push of it and a clear of the stack when there is none: the same call at
// boot and on the go-home gesture.
bool Routines::resetToRoot() {
  const int base = be_top(vm_);
  if (!be_getglobal(vm_, kRootScene)) {
    logger_->warn("vm: no root scene, showing the mark");
  } else if (!be_isinstance(vm_, -1)) {
    logger_->warn("vm: root scene is not an instance, showing the mark");
    be_pop(vm_, 1);
    be_pushnil(vm_);
  }
  const bool showing = callUx("reset", be_absindex(vm_, -1));
  be_pop(vm_, be_top(vm_) - base);
  return showing;
}

// Calls `method` on the `ux` global and answers its result as a truth value. `argIndex` is a value
// already on the VM stack to pass as the one argument, or 0 for none. A raise is reported the way a
// scene's own raise was when the stack was native: `vm: <method>: <message>`.
bool Routines::callUx(const char* method, int argIndex) const {
  const int base = be_top(vm_);
  bool answer = false;
  if (!be_getglobal(vm_, kUxGlobal) || !be_getmethod(vm_, -1, method)) {
    logger_->error("vm: ux is missing");
  } else {
    be_pushvalue(vm_, -2);  // the instance, as self
    int argc = 1;
    if (argIndex > 0) {
      be_pushvalue(vm_, argIndex);
      argc++;
    }
    if (be_pcall(vm_, argc) == BE_OK) {
      answer = be_tobool(vm_, -1) != 0;
    } else {
      String message = "vm: ";
      message += method;
      message += ": ";
      const char* detail = be_tostring(vm_, -1);
      message += detail != nullptr ? detail : "error";
      logger_->error(message.c_str());
    }
  }
  be_pop(vm_, be_top(vm_) - base);
  return answer;
}

// --- loading ---------------------------------------------------------------

// The order file is a plain list: a `#` line and a blank line are skipped, and every other line is
// a path relative to /system, so `components/text.be` and `scene.be` are the same kind of entry.
size_t Routines::loadSystem(size_t& found) {
  found = 0;

  String order;
  if (library_ == nullptr || !library_->read(kSystemOrder, order)) {
    logger_->error("vm: /system/order unreadable, no system files");
    return 0;
  }

  std::vector<String> listed;
  size_t loaded = 0;
  int start = 0;
  while (start <= (int)order.length()) {
    const int end = order.indexOf('\n', start);
    String line = end < 0 ? order.substring(start) : order.substring(start, end);
    line.trim();
    if (!line.isEmpty() && !line.startsWith("#")) {
      listed.push_back(line);
      String path = "/";
      path += kSystemFolder;
      path += '/';
      path += line;
      if (loadFile(path)) loaded++;
      found++;
    }
    if (end < 0) break;
    start = end + 1;
  }

  reportUnlisted(*library_, *logger_, listed);
  return loaded;
}

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

// --- a document's own values ------------------------------------------------

bool Routines::storeGet(const char* document, const char* key, String& out) {
  if (!plainWord(document) || key == nullptr || library_ == nullptr) return false;

  String text;
  if (!library_->read(documentPath(document), text)) return false;

  Values values;
  values.parse(text);
  if (!values.has(key)) return false;
  out = values.get(key, "");
  return true;
}

bool Routines::storeSet(const char* document, const char* key, const String& value) {
  if (!plainWord(document) || key == nullptr || library_ == nullptr) return false;

  String text;
  Values values;
  if (library_->read(documentPath(document), text)) values.parse(text);

  // A repeat value is not worth a flash write.
  if (values.has(key) && values.get(key, "") == value) return true;
  if (!values.set(key, value)) return false;
  return library_->write(documentPath(document), values.text());
}

}  // namespace gardener
