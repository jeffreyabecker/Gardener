#pragma once

#include <stdint.h>

#include <any>

#include "Canvas.h"
#include "panel/GestureQueue.h"

namespace gardener::panel::page {

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
  void draw(gfx::Canvas& canvas, uint32_t nowMs);

  // One beat: drain the queue, tick, then draw when a gesture was taken or RepaintMs has passed.
  // Answers whether it drew.
  bool poll(gfx::Canvas& canvas, uint32_t nowMs);

  // Called when the scene goes on top, before the first draw. The Berry scene takes the pusher's
  // arguments; nothing here has anything to pass yet.
  virtual void activate() {}
  virtual void leave(){}

  // What the scene hands back when it pops: the value the page reads, in whatever type the setting
  // holds — a number, a key, a book. An empty any is no value at all, which is what a nullable
  // setting holding nothing answers; a scene that hands nothing back leaves it empty too.
  virtual std::any value() const { return std::any(); }

  // The scene itself when its body is a page an interpreter holds rather than C++ in this library,
  // which is how the interpreter reaches the page a frame shows — ux.top() is what asks, and it
  // casts the answer back to the class that gave it. Nothing for a scene written here. A pointer
  // rather than a type test, because the firmware build has RTTI off.
  virtual const void* scripted() const { return nullptr; }

  // Whether the scene may hold nothing at all: an editor over a nullable setting says so, and
  // isNull() answers whether it is holding nothing now.
  virtual bool nullable() const { return false; }
  bool isNull() const { return !value().has_value(); }


  uint32_t lastDraw() const { return lastDraw_; }

 protected:
  

  // The gesture the scene answers, or false for one it drops. The whole string is the gesture, so a
  // scene matches it in full or not at all.
  virtual bool handle(const char*) { return false; }

  virtual void drawPage(gfx::Canvas& canvas) { (void)canvas; }

  virtual void tick(uint32_t nowMs) { (void)nowMs; }

 private:
  bool takeEvents();

  GestureQueue& gestures_;
  uint32_t lastDraw_ = 0;
};

}  // namespace gardener::panel::page
