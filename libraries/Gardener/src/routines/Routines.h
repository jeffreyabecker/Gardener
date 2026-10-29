#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "panel/page/Ux.h"
#include "panel/LocklessGestureQueue.h"
#include "routines/Interpreter.h"
#include "routines/RoutineRegistry.h"
#include <Gfx.h>

namespace gardener {

namespace panel {
class PanelContext;
}  // namespace panel

class Logger;
class Library;

// The Berry runtime behind the routines: the files the interpreter compiles and the beat that drives
// the native scene stack over them. The interpreter itself is left to `Interpreter`, whose
// implementation is the host side's; the stack is `panel::page::Ux` and this side owns it: one
// entry per beat, the overlay the panel shows drawn over the page, and the beat's one flush — left
// here because the stack draws in the frame the display holds, and nothing else sends that frame.
//
// The beat is called from the panel task's own frame — never from inside a Berry call — so there is
// nothing suspended, no second core and no lock.
class Routines {
 public:
  explicit Routines(Interpreter& interpreter) : interpreter_(interpreter) {}

  // Starts the interpreter and loads the files `/system/order` names and then every
  // `/routines/*.be`, and resets the scene stack. The system comes first so a page can use what one
  // of its files defines. False only when the interpreter cannot be allocated at all.
  bool begin(panel::PanelContext& context);

  // The page's way out: leave whichever scenes are showing and give the panel back to the root.
  // False when no scene is left showing, which leaves the mark.
  bool showDefault();

  // One beat: the overlay built from this beat's values, the top scene's poll, the overlay drawn
  // over it, and the frame sent. True while a page is showing, so the caller knows whether to draw
  // its own frame.
  bool poll(uint32_t nowMs);

  // The page became visible: the top scene's activate and the frame with it, so a scene re-reads the
  // unit and redraws rather than trusting what it knew before. True while a page is showing.
  bool enterPage();

  // Whether a page is showing. The beat takes poll()'s answer instead; this is for the gesture path,
  // which asks once per press.
  bool active() const { return stack_.depth() > 0; }

  // What the load cost: the files that loaded of the files that were found. The difference is a file
  // that did not compile and a `.be` file `/system/order` does not name, and nothing else reads them:
  // the panel loads the image whatever it holds, so the device reports a page that is missing rather
  // than refusing to boot.
  size_t filesLoaded() const { return filesLoaded_; }
  size_t filesFound() const { return filesFound_; }

  // What the scenes take their gestures from. The queue `device.take_gesture()` drains is the same
  // one, so a press goes to whichever of the two is reading.
  panel::GestureQueue& presses() { return gestures_; }

  // What a settings row builds its editor on, and where a launch puts a page: the stack the beat
  // polls, so a page opened from a page is pushed over it rather than into a second one.
  panel::page::Ux& scenes() { return stack_; }

  // The routines the image registered, in the order the files registered them: what the `routines`
  // module puts a record into and reads a set back through, and what a row launches.
  RoutineRegistry& registry() { return *registry_; }

  // --- the `device` module's gesture surface ---
  void pushPress(const String& gesture) { gestures_.push(gesture); }
  bool takePress(String& gesture) { return gestures_.take(gesture); }

 private:
  size_t loadSystem(size_t& found);
  size_t loadAll(const char* folder, size_t& found);
  bool loadFile(const String& path);
  bool resetToRoot();

  // The beat's one send, and the only place the frame is flushed: the page and the overlay have
  // drawn into the panel's canvas, and this hands it to the glass once.
  void show();

  // The overlay from one reading: the unit's suffix, the two links, the run state and the gauge, as
  // they stood on this beat, in four equal cells over the bottom rows of the glass.
  void updateOverlay(uint32_t nowMs);

  // The VM the files are loaded into and the root page read from.
  Interpreter& interpreter_;
  Logger* logger_ = nullptr;
  // The files the interpreter loads and the routine documents it reads and writes, from the context
  // at begin(), so nothing here names a filesystem or a path.
  Library* library_ = nullptr;
  // Where the image's routine files put their records, built at begin() because the library and the
  // logger it reads documents through are the context's.
  std::unique_ptr<RoutineRegistry> registry_;
  // The panel's parts the beat reaches: the display it draws in and flushes, and the readings the
  // overlay shows.
  panel::PanelContext* panel_ = nullptr;
  panel::page::Ux stack_;
  panel::LocklessGestureQueue gestures_;
  // What begin() loaded of what it found, for the run that reports whether the image is all there.
  size_t filesLoaded_ = 0;
  size_t filesFound_ = 0;
};

}  // namespace gardener
