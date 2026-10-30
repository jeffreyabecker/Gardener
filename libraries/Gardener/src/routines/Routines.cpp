#include "Build.h"

#if GARDENER_PANEL

#include "routines/Routines.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "Canvas.h"
#include "panel/glass/Overlay.h"
#include "panel/page/MarkScene.h"
#include "core/SystemState.h"
#include "core/Units.h"
#include "core/Library.h"

namespace gardener {
namespace {

// The system is the files the interpreter needs before anything else: the registry and the pages the
// browser opens. Its order is not the folder's: Berry resolves a global when it compiles a file, so a
// class has to be a global by the time a file that names it is compiled. `/system/order` states that
// order, one path per line relative to `/system`.
constexpr const char* SystemFolder = "system";
constexpr const char* SystemOrder = "/system/order";

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

bool Routines::begin(panel::PanelContext& context) {
  logger_ = &context.logger();
  library_ = &context.library();
  panel_ = &context;
  // Before a file loads: a routine file registers itself as the interpreter runs it.
  registry_ = std::make_unique<RoutineRegistry>(context.library(), context.logger());

  if (!interpreter_.begin(context, *this)) {
    logger_->error("vm: no memory, routines off");
    return false;
  }
  gestures_.begin(*logger_);

  // The system first, in the order `/system/order` states: a scene may construct a class one of
  // those files defines — or answer with a record one of them declares — so they have to have run
  // before the routine files do.
  size_t systemFiles = 0;
  const size_t system = loadSystem(systemFiles);
  size_t routineFiles = 0;
  const size_t routines = loadAll("routines", routineFiles);

  const size_t files = systemFiles + routineFiles;
  filesLoaded_ = system + routines;
  filesFound_ = files;
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

// The overlay is the stack's, so it is set from this beat's reading before the page is polled and
// drawn with it in the same frame: the values the machine has now, not the ones it had when the
// page went up.
bool Routines::poll(uint32_t nowMs) {
  if (panel_ == nullptr) return false;
  updateOverlay(nowMs);
  if (!ux_.poll(panel_->canvas(), nowMs)) return false;
  show();
  return true;
}

bool Routines::enterPage() {
  if (panel_ == nullptr) return false;
  const uint32_t now = millis();
  updateOverlay(now);
  if (!ux_.enter(panel_->canvas(), now)) return false;
  show();
  return true;
}

// The beat's one send: whatever the page and the overlay drew goes to the glass from the panel's
// own canvas. A scene draws into the canvas and never sends it, and no caller sends it after the
// beat, so a beat is exactly one flush however many draws went into it.
void Routines::show() { panel_->canvas().flush(panel_->display()); }

// The root scene, at boot and on the go-home gesture: the image's own root page, which is the home
// page, and the device's mark when the image sets none. The stack draws it, so the overlay is drawn
// over it like any other page.
bool Routines::resetToRoot() {
  if (panel_ == nullptr) return ux_.reset(nullptr);
  if (std::unique_ptr<panel::page::Scene> page = interpreter_.rootScene(gestures_, *panel_)) {
    if (ux_.reset(std::move(page))) return true;
  }
  return ux_.reset(std::make_unique<panel::page::MarkScene>(gestures_));
}

void Routines::updateOverlay(uint32_t nowMs) {
  // One copy of the state the panel was handed, so the four cells are drawn from one instant rather
  // than from four reads of an object a task is publishing into.
  const SystemState state = panel_->systemState();
  const int8_t percent = state.batteryPresent ? static_cast<int8_t>(state.batteryPercent) : -1;

  // The strip clears its own rows first: the page canvas stops where the overlay starts, so nothing
  // else touches them and they keep whatever the last frame left.
  const int16_t start =
      static_cast<int16_t>(panel_->canvas().height() - panel::glass::OverlayHeight);
  ux_.setOverlay(start,
                    gfx::underlay(gfx::row(1.0f, {
                        gfx::Child{gfx::Cell(0.25f, 1.0f),
                                   panel::glass::overlayUnit(displayUnitInfo(state.unit).suffix)},
                        gfx::Child{gfx::Cell(0.25f, 1.0f),
                                   panel::glass::overlayLinks(state.serialConnected,
                                                              state.bluetoothConnected)},
                        gfx::Child{gfx::Cell(0.25f, 1.0f),
                                   panel::glass::overlayState(state.runState, nowMs)},
                        gfx::Child{gfx::Cell(0.25f, 1.0f),
                                   panel::glass::overlayBattery(percent, state.batteryLow, nowMs)},
                    })));
}

// --- loading ---------------------------------------------------------------

// The order file is a plain list: a `#` line and a blank line are skipped, and every other line is
// a path relative to /system, so `home.be` and a file in a subfolder are the same kind of entry.
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

  return interpreter_.load(path, source);
}

}  // namespace gardener
#endif
