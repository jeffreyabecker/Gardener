#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "routines/GestureQueue.h"
#include "routines/Interpreter.h"
#include <Gfx.h>

namespace gardener {

class PanelContext;
class Logger;
class Library;

// The Berry runtime behind the routines: the files the interpreter compiles and the beat that drives
// the native scene stack over them. The interpreter itself is left to `Interpreter`, whose
// implementation is the host side's; the stack is `gfx::ui::Ux` and this side owns it: one entry per
// beat, the band the panel shows drawn over the page, and the flush the panel task leaves here
// because the stack draws in the frame the display holds.
//
// The beat is called from the panel task's own frame — never from inside a Berry call — so there is
// nothing suspended, no second core and no lock.
class Routines {
 public:
  explicit Routines(Interpreter& interpreter) : interpreter_(interpreter) {}

  // Starts the interpreter and loads the files `/system/order` names and then every
  // `/routines/*.be`, and resets the scene stack. The system comes first so a page can use what one
  // of its files defines. False only when the interpreter cannot be allocated at all.
  bool begin(PanelContext& context);

  // The page's way out: leave whichever scenes are showing and give the panel back to the root.
  // False when no scene is left showing, which leaves the mark.
  bool showDefault();

  // One beat: the band's reading from this beat's values, the top scene's poll, the band drawn over
  // it, and the frame sent. True while a page is showing, so the caller knows whether to draw its
  // own frame.
  bool poll(uint32_t nowMs);

  // The page became visible: the top scene's activate and the frame with it, so a scene re-reads the
  // unit and redraws rather than trusting what it knew before. True while a page is showing.
  bool enterPage();

  // Whether a page is showing. The beat takes poll()'s answer instead; this is for the gesture path,
  // which asks once per press.
  bool active() const { return stack_.depth() > 0; }

  // What the scenes take their gestures from. The queue `device.take_gesture()` drains is the same
  // one, so a press goes to whichever of the two is reading.
  gfx::ui::GestureQueue& presses() { return gestures_; }

  // What a settings row builds its editor on: the stack the beat polls, so a list opened from a
  // page is pushed over it rather than into a second one.
  gfx::ui::Ux& scenes() { return stack_; }

  // --- the `device` module's gesture surface ---
  void pushPress(const String& gesture) { gestures_.push(gesture); }
  bool takePress(String& gesture) { return gestures_.take(gesture); }

 private:
  size_t loadSystem(size_t& found);
  size_t loadAll(const char* folder, size_t& found);
  bool loadFile(const String& path);
  bool resetToRoot();

  // The band's cells from one reading: the unit's suffix, the two links, the run state and the
  // gauge, as they stood on this beat.
  void updateBand(uint32_t nowMs);

  // The VM the files are loaded into and the root page read from.
  Interpreter& interpreter_;
  Logger* logger_ = nullptr;
  // The files the interpreter loads and the routine documents it reads and writes, from the context
  // at begin(), so nothing here names a filesystem or a path.
  Library* library_ = nullptr;
  // The panel's parts the beat reaches: the display it draws in and flushes, and the readings the
  // band shows.
  PanelContext* panel_ = nullptr;
  gfx::ui::Ux stack_;
  GestureQueue gestures_;
};

}  // namespace gardener
