#pragma once

namespace gardener::panel::page {

// What a page takes its gestures from: the panel's queue behind an interface, so the drawing side
// names nothing of the machine that fills it. A page takes one gesture at a time, oldest first.
class GestureQueue {
 public:
  virtual ~GestureQueue() = default;

  GestureQueue(const GestureQueue&) = delete;
  GestureQueue& operator=(const GestureQueue&) = delete;

  // The oldest gesture the page has not taken, or nullptr when there is none. The pointer is the
  // queue's own and stays valid until the next call, so a caller compares or copies it and does not
  // keep it.
  virtual const char* take() = 0;

 protected:
  GestureQueue() = default;
};

}  // namespace gardener::panel::page
