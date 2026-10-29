#include "Build.h"

#if GARDENER_PANEL

#include "BerryScene.h"

#include <berry.h>


#include "core/Logger.h"
#include "DrawTree.h"
#include "Module.h"

namespace gardener {
namespace {

// The pages the scenes hold, in a list the interpreter owns: a scene's page is reachable from the
// collector there for as long as the scene exists. A slot is the index its page was appended at,
// and it stays that index because a scene that goes clears its entry rather than closing the gap.
constexpr const char* PageList = "__pages";

}  // namespace

BerryScene::BerryScene(panel::page::GestureQueue& gestures, bvm* vm, int page,
                       panel::PanelContext& panel)
    : Scene(gestures), vm_(vm), panel_(panel) {
  const int base = be_top(vm_);
  if (!be_getglobal(vm_, PageList) || !be_islist(vm_, -1)) {
    // The lookup leaves a nil whether it found a global or not, and the list replaces it.
    be_pop(vm_, 1);
    be_newlist(vm_);
    be_setglobal(vm_, PageList);
  }
  const int list = be_absindex(vm_, -1);
  be_pushvalue(vm_, page);
  be_data_push(vm_, list);
  slot_ = be_data_size(vm_, list) - 1;
  be_pop(vm_, be_top(vm_) - base);
}

// The page goes with the scene: the slot is emptied rather than dropped, so no other scene's index
// moves.
BerryScene::~BerryScene() {
  if (vm_ == nullptr || slot_ < 0) return;
  const int base = be_top(vm_);
  if (be_getglobal(vm_, PageList) && be_islist(vm_, -1)) {
    const int list = be_absindex(vm_, -1);
    be_pushint(vm_, slot_);
    be_pushnil(vm_);
    be_setindex(vm_, list);
  }
  be_pop(vm_, be_top(vm_) - base);
}

bool BerryScene::pushPage(bvm* vm) const {
  if (vm_ == nullptr || slot_ < 0) return false;
  if (!be_getglobal(vm, PageList) || !be_islist(vm, -1)) return false;
  const int list = be_absindex(vm, -1);
  be_pushint(vm, slot_);
  be_getindex(vm, list);
  // be_getindex pushes the value it read and leaves the list under it; the value is the page.
  be_remove(vm, -2);
  if (be_isinstance(vm, -1)) return true;
  // A slot the scene already emptied: there is no page to answer with.
  be_pop(vm, 1);
  return false;
}

void BerryScene::activate() {
  const int base = be_top(vm_);
  if (beginCall("activate")) endCall("activate", 1);
  be_pop(vm_, be_top(vm_) - base);
}

// What the page answered, as the stack's truth value: a page that returns nothing took nothing.
bool BerryScene::handle(const char* gesture) {
  const int base = be_top(vm_);
  bool took = false;
  if (beginCall("handle")) {
    be_pushstring(vm_, gesture);
    took = endCall("handle", 2) && be_tobool(vm_, -1) != 0;
  }
  be_pop(vm_, be_top(vm_) - base);
  return took;
}

void BerryScene::tick(uint32_t nowMs) {
  const int base = be_top(vm_);
  if (beginCall("tick")) {
    be_pushint(vm_, (bint)nowMs);
    endCall("tick", 2);
  }
  be_pop(vm_, be_top(vm_) - base);
}

// The page answers the tree it wants drawn, and the scene draws it here rather than inside the call:
// a raise the walk met would otherwise unwind through C++ frames.
void BerryScene::drawPage(gfx::Canvas& canvas) {
  const int base = be_top(vm_);
  if (beginCall("draw_page")) {
    if (endCall("draw_page", 1)) {
      drawTree(vm_, be_absindex(vm_, -1), panel_, canvas);
    }
  }
  be_pop(vm_, be_top(vm_) - base);
}

bool BerryScene::beginCall(const char* method) {
  if (vm_ == nullptr || broken_ || slot_ < 0) return false;
  if (!be_getglobal(vm_, PageList) || !be_islist(vm_, -1)) return false;
  const int list = be_absindex(vm_, -1);
  be_pushint(vm_, slot_);
  be_getindex(vm_, list);
  // be_getindex pushes the value it read and leaves the index under it; the value is the page.
  be_remove(vm_, -2);
  if (!be_isinstance(vm_, -1)) return false;
  const int page = be_absindex(vm_, -1);
  if (!be_getmethod(vm_, page, method)) return false;
  // The member is the page's function rather than one bound to it, so self goes under whatever the
  // caller pushes as the arguments.
  be_pushvalue(vm_, page);
  return true;
}

bool BerryScene::endCall(const char* method, int argc) {
  const int rc = be_pcall(vm_, argc);
  if (rc != BE_OK) {
    // A failed call leaves the exception and its argument, the argument being the message. A page
    // that raised is not called again: the beat is 250 ms, so it would raise every beat.
    broken_ = true;
    if (berry::logger != nullptr) {
      String message = "vm: ";
      message += method;
      message += ": ";
      const char* detail = be_tostring(vm_, -1);
      message += detail != nullptr ? detail : "error";
      berry::logger->error(message.c_str());
    }
    be_pop(vm_, 2);
    return false;
  }
  // A protected call leaves its answer *under* the arguments it was handed — the harness reads its
  // own results the same way — so the answer is lifted over them and they are dropped, leaving what
  // a caller reads on top.
  be_pushvalue(vm_, -(argc + 1));
  for (int i = 0; i < argc + 1; i++) be_remove(vm_, -2);
  return true;
}

}  // namespace gardener
#endif
