#pragma once

#include <Arduino.h>
#include <memory>

namespace gardener {
namespace panel {
namespace page {
class Scene;
}  // namespace page
}  // namespace panel

inline namespace routines {

// The interpreter the routines are compiled and run in, and the image's root page as a scene. It is
// the core's surface over the vendored VM: the core names this and never `<berry.h>`, and
// `BerryInterpreter.h` beside this file supplies the implementation.
//
// The loader owns the policy — which files, in what order — and the interpreter owns the VM: what a
// file compiled to, what a raise means and where the page it made is kept. The VM and the app behind
// it come up in the implementation's constructor, so there is no start step on this surface.
class Interpreter {
 public:
  virtual ~Interpreter() = default;

  // Creates the VM and installs the native modules a page is written against, the app they reach
  // being the one the interpreter was built with. False only when the VM cannot be allocated at all,
  // in which case nothing loads and the image leaves the panel its mark.
  virtual bool begin() = 0;

  // Compiles and runs one of the image's files, logging a compile or run failure. False when the
  // file did not load; reading it is the loader's.
  virtual bool load(const String& path, const String& source) = 0;

  // The image's root page as a scene, or nothing when it sets none. The global is read when it is
  // wanted rather than cached, so whatever a later load sets is the page the go-home gesture goes
  // to. The gestures and the state the scene is driven by are the interpreter's own, taken from the
  // app it was built with.
  virtual std::unique_ptr<panel::page::Scene> rootScene() = 0;
};

}  // namespace routines
}  // namespace gardener
