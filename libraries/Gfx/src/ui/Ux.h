#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <vector>

#include "ui/Overlay.h"
#include "ui/Scene.h"

namespace gardener::gfx::ui {

// The scene stack: what is showing, and the request a scene makes of it. A push or a pop asked for
// from inside a scene's own activate or poll is held and applied once that call returns, because a
// scene's poll ends by drawing: applying it there would paint the page that just left over the one
// that came back. One held request per beat, so a push from inside an activate is a beat later
// rather than a recursion.
//
// The stack owns its scenes: what is pushed goes away when it pops, after the pusher's callback has
// read what it answered. A scene therefore reaches the stack by unique_ptr, and top() is only ever
// a look at one the stack is holding.
//
// It also owns the band across the bottom of the glass: the cells the caller sets are drawn over the
// bottom rows after the page on every beat, so no page has to remember it. A stack with no cells
// draws none, and a stack showing nothing draws none either — the mark is what shows then.
class Ux {
 public:
  // The deepest stack the panel accepts. A scene that pushes from its own activate is a bug either
  // way; the cap is what keeps it a false answer instead of a stack exhausted in one beat.
  static constexpr int16_t MaxFrames = 8;

  // What the pusher runs when its scene pops, handed that scene: it reads the value the scene
  // answers before the stack lets it go.
  using Callback = std::function<void(Scene&)>;

  // The root scene at boot and on the go-home gesture; nullptr empties the stack and leaves the
  // mark. Answers whether a scene is showing.
  bool reset(std::unique_ptr<Scene> root);

  // One scene on top of the one showing, and the top one taken off. A pop answers false when only
  // the root is left, because the root is what popToRoot() is for.
  bool push(std::unique_ptr<Scene> scene, Callback callback = nullptr);
  bool pop();

  // Every scene above the root, in one move: none of their callbacks runs.
  bool popToRoot();

  Scene* top() const;
  int16_t depth() const;

  // The band's cells, left to right, each in an equal share of the panel's width.
  void setBand(const std::vector<Paint>& cells);

  // One call into the top scene: its activate() when the page becomes visible, and its poll each
  // beat. Both draw the band after the page has drawn. Answers whether a scene is showing.
  bool enter(Canvas& canvas);
  bool poll(Canvas& canvas, uint32_t nowMs);

 private:
  enum class Request : uint8_t { None, Push, Pop, ToRoot };

  bool place(std::unique_ptr<Scene> scene, const Callback& callback);
  bool take();
  bool home();
  void apply();
  void paintBand(Canvas& canvas);

  std::unique_ptr<Scene> frames_[MaxFrames];
  Callback callbacks_[MaxFrames];
  int16_t depth_ = 0;
  std::vector<Paint> band_;

  Request pending_ = Request::None;
  std::unique_ptr<Scene> pendingScene_;
  Callback pendingCallback_;

  bool polling_ = false;
};

}  // namespace gardener::gfx::ui
