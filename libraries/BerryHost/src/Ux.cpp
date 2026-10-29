#include "Build.h"

#if GARDENER_PANEL

#include <string.h>

#include <any>
#include <memory>
#include <utility>

#include <Arduino.h>
#include <berry.h>

#include <Gfx.h>

#include "BerryScene.h"
#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "panel/page/Ux.h"
#include "routines/Routines.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {
namespace {

// The functions the image pushed with a page, in a list the interpreter owns: a frame outlives the
// call that pushed it, and a value a C++ object holds is none of the collector's, so a callback
// waits here until it runs or the page that carried it goes.
constexpr const char* CallbackList = "__uxCallbacks";

// The scene a native factory built, carried into the image as an instance the one field holds: the
// stack owns it once it is pushed, and a handle that is never pushed frees the scene when the
// collector frees the instance.
constexpr const char* SceneRecord = "SceneHandle";

const bnfuncinfo SceneFields[] = {
    {"init", recordInit},
    {".p", nullptr},
    {nullptr, nullptr},
};

using SceneOwner = std::unique_ptr<panel::page::Scene>;

int sceneRelease(bvm* vm) {
  delete static_cast<SceneOwner*>(be_tocomptr(vm, 1));
  be_return_nil(vm);
}

// One slot of the callback list, emptied when the last copy of the callback it belongs to goes —
// whether the callback ran or the page left without it.
struct CallbackSlot {
  bvm* vm = nullptr;
  int slot = -1;

  ~CallbackSlot() {
    if (vm == nullptr || slot < 0) return;
    const int base = be_top(vm);
    if (be_getglobal(vm, CallbackList) && be_islist(vm, -1)) {
      const int list = be_absindex(vm, -1);
      be_pushint(vm, slot);
      be_pushnil(vm);
      be_setindex(vm, list);
    }
    be_pop(vm, be_top(vm) - base);
  }
};

// A value a scene answered, as the image reads it: the settled set a settings list hands back as a
// map, and the single value an editor hands back in the type its row holds it in.
void pushAnswer(bvm* vm, const std::any& value) {
  if (const panel::page::SettingValues* values =
          std::any_cast<panel::page::SettingValues>(&value)) {
    pushSettingsMap(vm, *values);
    return;
  }
  pushSettingValue(vm, value);
}

// The function at `index` held in the callback list, reusing the slot an earlier callback left, so
// the list is as long as the most callbacks alive at once rather than as many as were ever pushed.
std::shared_ptr<CallbackSlot> holdCallback(bvm* vm, int index) {
  auto held = std::make_shared<CallbackSlot>();
  held->vm = vm;
  if (!be_getglobal(vm, CallbackList) || !be_islist(vm, -1)) {
    be_pop(vm, 1);
    held->vm = nullptr;
    return held;
  }
  const int list = be_absindex(vm, -1);
  const int count = be_data_size(vm, list);
  int slot = -1;
  for (int i = 0; i < count && slot < 0; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    const bool free = be_isnil(vm, -1);
    be_pop(vm, 2);  // the value and the index the read left under it
    if (free) slot = i;
  }
  if (slot < 0) {
    be_pushvalue(vm, index);
    be_data_push(vm, list);
    slot = be_data_size(vm, list) - 1;
    be_pop(vm, 1);
  } else {
    be_pushint(vm, slot);
    be_pushvalue(vm, index);
    be_setindex(vm, list);
    be_pop(vm, 2);
  }
  held->slot = slot;
  be_pop(vm, 1);  // the list
  return held;
}

// The function at `slot` called with the answer the page handed back. A raise is one line and the
// call is dropped, the way a page's own hook reports one.
void runCallback(bvm* vm, int slot, const std::any& answer) {
  if (vm == nullptr || slot < 0) return;
  const int base = be_top(vm);
  if (be_getglobal(vm, CallbackList) && be_islist(vm, -1)) {
    const int list = be_absindex(vm, -1);
    be_pushint(vm, slot);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the index the read left under the function
    if (be_isfunction(vm, -1)) {
      pushAnswer(vm, answer);
      if (be_pcall(vm, 1) != BE_OK && logger != nullptr) {
        String message("vm: ux.push: ");
        const char* detail = be_tostring(vm, -1);
        message += detail != nullptr ? detail : "error";
        logger->error(message.c_str());
      }
    }
  }
  be_pop(vm, be_top(vm) - base);
}

// Whether the value at `index` is the handle this module made, which is what tells it apart from a
// page instance the image built.
bool isSceneHandle(bvm* vm, int index) {
  if (!be_isinstance(vm, index)) return false;
  const char* name = be_classname(vm, index);
  return name != nullptr && strcmp(name, SceneRecord) == 0;
}

// The scene a handle holds, moved out: a handle pushed twice answers nothing the second time.
std::unique_ptr<panel::page::Scene> takeSceneHandle(bvm* vm, int index) {
  if (!isSceneHandle(vm, index)) return nullptr;
  const int base = be_top(vm);
  std::unique_ptr<panel::page::Scene> scene;
  if (be_getmember(vm, index, ".p") && be_iscomobj(vm, -1)) {
    SceneOwner* owner = static_cast<SceneOwner*>(be_tocomptr(vm, -1));
    if (owner != nullptr) scene = std::move(*owner);
  }
  be_pop(vm, be_top(vm) - base);
  return scene;
}

// The page called before the stack moves: the panel's parts are what a scene and a handle are built
// from, and neither exists before the panel runs.
void requirePanel(bvm* vm, const char* call) {
  if (context == nullptr || routines == nullptr) {
    be_raise(vm, "runtime_error", call);
  }
}

// ux.push(page, callback): a page over the one showing, either a page instance the image built or a
// handle a native factory answered. The callback, when there is one, is handed what the page
// answered when it pops. Answers whether a page is showing: false for nil, for a page the stack
// refuses — an empty handle, one already pushed, or the eight-frame cap — and true when placed or
// held to the end of the beat.
int uxPush(bvm* vm) {
  requirePanel(vm, "ux.push: no panel");
  if (be_top(vm) < 1) {
    be_raise(vm, "type_error", "ux.push: push(page, callback)");
  }
  const bool handle = isSceneHandle(vm, 1);
  if (!handle && !be_isnil(vm, 1) && !be_isinstance(vm, 1)) {
    be_raise(vm, "type_error", "ux.push: the page is a scene or a page instance");
  }
  const bool callback = be_top(vm) >= 2 && !be_isnil(vm, 2);
  if (callback && !be_isfunction(vm, 2)) {
    be_raise(vm, "type_error", "ux.push: the callback is a function");
  }
  // The checks are above: a raise below would longjmp past the scene and the callback it holds.
  std::unique_ptr<panel::page::Scene> scene;
  if (handle) {
    scene = takeSceneHandle(vm, 1);
  } else if (be_isinstance(vm, 1)) {
    scene = std::make_unique<BerryScene>(routines->presses(), vm, 1, *context);
  }
  panel::page::Ux::Callback held;
  if (callback) {
    const std::shared_ptr<CallbackSlot> slot = holdCallback(vm, 2);
    held = [slot](panel::page::Scene& popped) {
      runCallback(slot->vm, slot->slot, popped.value());
    };
  }
  be_pushbool(vm, routines->ux().push(std::move(scene), std::move(held)));
  be_return(vm);
}

// ux.pop(): the top page taken off, its callback run. Answers false when only the root is left,
// which is what pop_to_root is for.
int uxPop(bvm* vm) {
  requirePanel(vm, "ux.pop: no panel");
  be_pushbool(vm, routines->ux().pop());
  be_return(vm);
}

// ux.pop_to_root(): every page above the root taken off in one move, no callback run. Answers
// whether a page is showing.
int uxPopToRoot(bvm* vm) {
  requirePanel(vm, "ux.pop_to_root: no panel");
  be_pushbool(vm, routines->ux().popToRoot());
  be_return(vm);
}

// ux.depth(): how many frames are showing, the root included — 1 at boot and after every reset, and
// 0 with nothing showing. The stack's own count, never more than its cap.
int uxDepth(bvm* vm) {
  requirePanel(vm, "ux.depth: no panel");
  be_pushint(vm, routines->ux().depth());
  be_return(vm);
}

// ux.top(): the page the top frame shows, as the instance the image built for it, so a case can set a
// field on it and call its methods. Nil for a scene a native factory answered, which no page wrote,
// and nil with nothing showing.
int uxTop(bvm* vm) {
  requirePanel(vm, "ux.top: no panel");
  const panel::page::Scene* showing = routines->ux().top();
  const BerryScene* page =
      showing == nullptr ? nullptr : static_cast<const BerryScene*>(showing->scripted());
  if (page == nullptr || !page->pushPage(vm)) be_return_nil(vm);
  be_return(vm);
}

// ux.overlay_start(): the row the overlay begins at, which is where the page above it stops — 0
// when the stack has no overlay. A case marks from there rather than assuming a height.
int uxOverlayStart(bvm* vm) {
  requirePanel(vm, "ux.overlay_start: no panel");
  be_pushint(vm, routines->ux().overlayStart());
  be_return(vm);
}

const bnfuncinfo UxLibrary[] = {
    {"push", uxPush},
    {"pop", uxPop},
    {"pop_to_root", uxPopToRoot},
    {"depth", uxDepth},
    {"top", uxTop},
    {"overlay_start", uxOverlayStart},
    {nullptr, nullptr},
};

}  // namespace

// The scene a native factory built, carried into the image as an instance the one field holds.
void pushSceneHandle(bvm* vm, std::unique_ptr<panel::page::Scene> scene) {
  const int record = beginRecord(vm, SceneRecord);
  be_newcomobj(vm, new SceneOwner(std::move(scene)), sceneRelease);
  be_setmember(vm, record, ".p");
  be_pop(vm, 1);
}

// The module a page moves the stack through: the two calls that take a page and the one that takes
// every page above the root, and the class a native factory answers a scene with.
void installUxModule(bvm* vm) {
  installRecord(vm, SceneRecord, SceneFields);
  installModule(vm, "ux", UxLibrary);
  // The callbacks a pushed page carries, in a list the interpreter owns for the life of the VM: the
  // frames outlive the call that pushed them, and a value a C++ object holds is none of the
  // collector's.
  be_newlist(vm);
  be_setglobal(vm, CallbackList);
  // be_setglobal leaves the value it stored on the stack; the list is done with here.
  be_pop(vm, 1);
}

}  // namespace berry
}  // namespace gardener
#endif
