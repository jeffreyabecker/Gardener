#pragma once

#include <stdint.h>

#include <functional>

#include "ui/Scene.h"

namespace gardener::gfx::ui {

// The scene stack: what is showing, and the request a scene makes of it. A push or a pop asked for
// from inside a scene's own activate or poll is held and applied once that call returns, because a
// scene's poll ends by drawing: applying it there would paint the page that just left over the one
// that came back. One held request per beat, so a push from inside an activate is a beat later
// rather than a recursion.
//
// The stack holds its scenes rather than owning them. The page that pushes one keeps it alive, and
// a scene taken off the stack is told with leave().
class Ux {
 public:
  // The deepest stack the panel accepts. A scene that pushes from its own activate is a bug either
  // way; the cap is what keeps it a false answer instead of a stack exhausted in one beat.
  static constexpr int16_t MaxFrames = 8;

  // What the pusher runs when its scene pops, handed that scene: it reads the value the scene
  // hands back through its own accessors rather than the stack carrying one.
  using Callback = std::function<void(Scene&)>;

  // The root scene at boot and on the go-home gesture; nullptr empties the stack and leaves the
  // mark. Answers whether a scene is showing.
  bool reset(Scene* root);

  // One scene on top of the one showing, and the top one taken off. A pop answers false when only
  // the root is left, because the root is what popToRoot() is for.
  bool push(Scene& scene, Callback callback = nullptr);
  bool pop();

  // Every scene above the root, in one move: none of their callbacks runs.
  bool popToRoot();

  Scene* top() const;
  int16_t depth() const;

  // One call into the top scene: its activate() when the page becomes visible, and its poll each
  // beat. Answers whether a scene is showing.
  bool enter();
  bool poll(Canvas& canvas, uint32_t nowMs);

 private:
  enum class Request : uint8_t { None, Push, Pop, ToRoot };

  bool place(Scene& scene, const Callback& callback);
  bool take();
  bool home();
  void apply();

  Scene* frames_[MaxFrames] = {};
  Callback callbacks_[MaxFrames];
  int16_t depth_ = 0;

  Request pending_ = Request::None;
  Scene* pendingScene_ = nullptr;
  Callback pendingCallback_;

  bool polling_ = false;
};

}  // namespace gardener::gfx::ui
