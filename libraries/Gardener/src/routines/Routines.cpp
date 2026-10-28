#include "Build.h"

#if GARDENER_PANEL

#include "routines/Routines.h"

#include <berry.h>
#include <stddef.h>

#include <BerryHost.h>

#include <memory>
#include <vector>

#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "gfx/Canvas.h"
#include "hal/Battery.h"
#include "panel/Band.h"
#include "panel/MarkScene.h"
#include "panel/Units.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"
#include "storage/Library.h"

namespace gardener {
namespace {

// The system is the files the interpreter needs before anything else: the records a native call
// answers with, the registry and the pages the browser opens. Its order is not the folder's: Berry
// resolves a global when it compiles a file, so a class has to be a global by the time a file that
// names it is compiled. `/system/order` states that order, one path per line relative to `/system`.
constexpr const char* SystemFolder = "system";
constexpr const char* SystemOrder = "/system/order";

// The root page the image answers: the Berry instance the system sets here is what the panel shows
// when nothing else is up. An image that sets none leaves the mark.
constexpr const char* RootPage = "__rootScene";

// The image's root page as a scene, or nothing when the image sets none. The global is read when it
// is wanted rather than cached, so whatever a later load sets is the page the go-home gesture goes
// to.
std::unique_ptr<gfx::ui::Scene> rootPage(bvm* vm, gfx::ui::GestureQueue& gestures,
                                         PanelContext& panel) {
  const int base = be_top(vm);
  std::unique_ptr<gfx::ui::Scene> scene;
  if (be_getglobal(vm, RootPage) && be_isinstance(vm, -1)) {
    scene = std::make_unique<BerryScene>(gestures, vm, be_absindex(vm, -1), panel);
  }
  be_pop(vm, be_top(vm) - base);
  return scene;
}

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
    String path = SystemFolder;
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
  panel_ = &context;

  vm_ = be_vm_new();
  if (vm_ == nullptr) {
    logger_->error("vm: no memory, routines off");
    return false;
  }
  berry::install(vm_, context, *this, context.logger());
  gestures_.begin(*logger_);

  // The system first, in the order `/system/order` states: a scene may construct a class one of
  // those files defines — or answer with a record one of them declares — so they have to have run
  // before the routine files do.
  size_t systemFiles = 0;
  const size_t system = loadSystem(systemFiles);
  size_t routineFiles = 0;
  const size_t routines = loadAll("routines", routineFiles);

  const size_t files = systemFiles + routineFiles;
  String summary = "vm: ";
  summary += String((uint32_t)system);
  summary += " system, ";
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

// The band is the stack's, so it is set from this beat's reading before the page is polled and drawn
// with it in the same frame: the values the machine has now, not the ones it had when the page went
// up.
bool Routines::poll(uint32_t nowMs) {
  if (panel_ == nullptr) return false;
  updateBand(nowMs);
  if (!stack_.poll(panel_->canvas(), nowMs)) return false;
  panel_->canvas().flush(panel_->display());
  return true;
}

bool Routines::enterPage() {
  if (panel_ == nullptr) return false;
  const uint32_t now = millis();
  updateBand(now);
  if (!stack_.enter(panel_->canvas(), now)) return false;
  panel_->canvas().flush(panel_->display());
  return true;
}

// The root scene, at boot and on the go-home gesture: the image's own root page, which is the home
// page, and the device's mark when the image sets none. The stack draws it, so the band is drawn
// under it like any other page.
bool Routines::resetToRoot() {
  if (panel_ == nullptr) return stack_.reset(nullptr);
  if (std::unique_ptr<gfx::ui::Scene> page = rootPage(vm_, gestures_, *panel_)) {
    if (stack_.reset(std::move(page))) return true;
  }
  return stack_.reset(std::make_unique<MarkScene>(gestures_));
}

void Routines::updateBand(uint32_t nowMs) {
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(panel_->config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  const Battery& battery = panel_->battery();
  const int8_t percent = battery.present() ? static_cast<int8_t>(battery.percent()) : -1;
  stack_.setBand({
      bandUnit(displayUnitInfo(unit).suffix),
      bandLinks(panel_->serialConnected(), panel_->bluetoothConnected()),
      bandState(panel_->vacuum().runState(), nowMs),
      bandBattery(percent, battery.low(), nowMs),
  });
}

// --- loading ---------------------------------------------------------------

// The order file is a plain list: a `#` line and a blank line are skipped, and every other line is
// a path relative to /system, so `records.be` and a file in a subfolder are the same kind of entry.
size_t Routines::loadSystem(size_t& found) {
  found = 0;

  String order;
  if (library_ == nullptr || !library_->read(SystemOrder, order)) {
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
      path += SystemFolder;
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

}  // namespace gardener
#endif
