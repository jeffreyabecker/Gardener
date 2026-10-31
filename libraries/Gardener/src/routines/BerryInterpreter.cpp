#include "BerryInterpreter.h"

#include <berry.h>

#include <memory>

#include "bindings/BerryScene.h"
#include "bindings/BindingContext.h"
#include "bindings/Modules.h"

namespace gardener {
inline namespace routines {
namespace {

// The root page the image answers: the Berry instance the system sets here is what the panel shows
// when nothing else is up. An image that sets none leaves the mark.
constexpr const char* RootPage = "__rootScene";

}  // namespace

BerryInterpreter::BerryInterpreter(bindings::BindingContext& bindings) : bindings_(bindings) {}

bool BerryInterpreter::begin() {
  vm_ = be_vm_new();
  // A VM that could not be allocated leaves every load below refusing, so the image runs nothing and
  // the panel keeps its mark rather than a page nothing can compile.
  if (vm_ == nullptr) return false;
  installModules();
  return true;
}

void BerryInterpreter::installModules() {
  bindings::installVacuumModule(vm_, bindings_);
  bindings::installScreenModule(vm_, bindings_);
  bindings::installDeviceModule(vm_, bindings_);
  bindings::installSettingsModule(vm_, bindings_);
  bindings::installRoutinesModule(vm_, bindings_);
  bindings::installUxModule(vm_, bindings_);
}

bool BerryInterpreter::load(const String& path, const String& source) {
  if (vm_ == nullptr) return false;

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

std::unique_ptr<panel::page::Scene> BerryInterpreter::rootScene() {
  if (vm_ == nullptr) return nullptr;
  const int base = be_top(vm_);
  std::unique_ptr<panel::page::Scene> scene;
  if (be_getglobal(vm_, RootPage) && be_isinstance(vm_, -1)) {
    scene = std::make_unique<bindings::BerryScene>(vm_, be_absindex(vm_, -1), bindings_);
  }
  be_pop(vm_, be_top(vm_) - base);
  return scene;
}

// A failed call leaves two values: the exception and its argument. The argument is the message, and
// both are dropped so a broken file leaves the VM as it found it.
void BerryInterpreter::reportError(const char* file, const char* what) {
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
  bindings_.error(message.c_str());
  be_pop(vm_, 2);
}

}  // namespace routines
}  // namespace gardener
