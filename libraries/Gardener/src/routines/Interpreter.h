#pragma once

#include <Arduino.h>
#include <memory>

namespace gardener {

namespace panel {
class GestureQueue;
class PanelContext;

namespace page {
class Scene;
}  // namespace page
}  // namespace panel

class Logger;
class Routines;
struct SystemState;

// The interpreter the routines are compiled and run in, and the image's root page as a scene. It is
// the core's surface over the vendored VM: the core names this and never `<berry.h>`, and
// `BerryInterpreter.h` beside this file supplies the implementation.
//
// The loader owns the policy — which files, in what order — and the interpreter owns the VM: what a
// file compiled to, what a raise means and where the page it made is kept.
class Interpreter {
 public:
  virtual ~Interpreter() = default;

  // Creates the VM and installs the native modules a page is written against, reporting through
  // `logger`. False only when the interpreter cannot be allocated at all.
  virtual bool begin(Logger& logger, panel::PanelContext& context, Routines& routines) = 0;

  // Compiles and runs one of the image's files, logging a compile or run failure. False when the
  // file did not load; reading it is the loader's.
  virtual bool load(const String& path, const String& source) = 0;

  // The image's root page as a scene, or nothing when it sets none. The global is read when it is
  // wanted rather than cached, so whatever a later load sets is the page the go-home gesture goes
  // to. The state is what the scene's tree is drawn from.
  virtual std::unique_ptr<panel::page::Scene> rootScene(panel::GestureQueue& gestures,
                                                        const SystemState& state) = 0;
};

}  // namespace gardener
