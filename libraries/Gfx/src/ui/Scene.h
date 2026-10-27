#pragma once

#include <stdint.h>

#include "gfx/Canvas.h"
#include "ui/GestureQueue.h"

namespace gardener::gfx::ui {

// The lifecycle a page shares: the gesture drain, the repaint cadence and the hooks a page
// overrides. A page extends it and draws in drawPage; poll is what the host calls on each beat.
//
// A scene draws on the sub canvas the host made for it with Canvas::child, so it works in its own
// coordinates and never asks where it sits; there is no box to stamp.
//
// Nothing here reads a clock or an Arduino: the canvas and the beat arrive as arguments, which is
// what lets the class run under tools/gfx/check.py with a stub display and a fake queue.
class Scene {
 public:
  // The redraw cadence: a scene that nothing has asked for is drawn again this often, so a reading
  // that changes on its own still appears.
  static constexpr uint32_t RepaintMs = 250;

  explicit Scene(GestureQueue& gestures);
  virtual ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;

  // Clears the canvas to paper, draws the page on it, and stamps the beat so the cadence counts
  // from here. Only the scene's own canvas is cleared: a scene opened over another leaves what is
  // around it on the glass.
  void draw(Canvas& canvas, uint32_t nowMs);

  // One beat: drain the queue, tick, then draw when a gesture was taken or RepaintMs has passed.
  // Answers whether it drew.
  bool poll(Canvas& canvas, uint32_t nowMs);

  uint32_t lastDraw() const { return lastDraw_; }

 protected:
  // Called when the scene goes on top, before the first draw. The Berry scene takes the pusher's
  // arguments; nothing here has anything to pass yet.
  virtual void activate() {}

  // The gesture the scene answers, or false for one it drops. The whole string is the gesture, so a
  // scene matches it in full or not at all.
  virtual bool handle(const char*) { return false; }

  virtual void drawPage(Canvas& canvas) { (void)canvas; }

  virtual void tick(uint32_t nowMs) { (void)nowMs; }

 private:
  bool takeEvents();

  GestureQueue& gestures_;
  uint32_t lastDraw_ = 0;
};

}  // namespace gardener::gfx::ui
