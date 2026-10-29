#pragma once

#include "routines/Interpreter.h"

struct bvm;

namespace gardener {

class Logger;

// The vendored Berry VM behind the core's Interpreter surface: the VM itself, the install that gives
// it the modules a page is written against, and the scene the image's root page is put on the panel
// through.
class BerryInterpreter : public Interpreter {
 public:
  bool begin(panel::PanelContext& context, Routines& routines) override;
  bool load(const String& path, const String& source) override;
  std::unique_ptr<panel::page::Scene> rootScene(panel::page::GestureQueue& gestures,
                                            panel::PanelContext& panel) override;

  // The interpreter itself, for the host harness, which loads its own driver into the same VM after
  // the image has loaded and reads the heap it holds.
  bvm* vm() const { return vm_; }

 private:
  void reportError(const char* file, const char* what);

  bvm* vm_ = nullptr;
  Logger* logger_ = nullptr;
};

}  // namespace gardener
