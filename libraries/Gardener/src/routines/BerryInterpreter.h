#pragma once

#include "routines/Interpreter.h"

struct bvm;

namespace gardener {

namespace berry {
class BindingContext;
}

// The vendored Berry VM behind the core's Interpreter surface: the VM itself, the install that gives
// it the modules a page is written against, and the scene the image's root page is put on the panel
// through.
class BerryInterpreter : public Interpreter {
 public:
  // The app the native modules reach. The VM they are installed into comes up in begin(), so the
  // app this is built from is fixed from here on.
  explicit BerryInterpreter(berry::BindingContext& bindings);

  bool begin() override;
  bool load(const String& path, const String& source) override;
  std::unique_ptr<panel::page::Scene> rootScene() override;

  // The interpreter itself, for the host harness, which loads its own driver into the same VM after
  // the image has loaded and reads the heap it holds.
  bvm* vm() const { return vm_; }

 private:
  void reportError(const char* file, const char* what);
  // The six modules a page is written against, installed into the VM with this interpreter's app
  // behind them.
  void installModules();

  berry::BindingContext& bindings_;
  bvm* vm_ = nullptr;
};

}  // namespace gardener
