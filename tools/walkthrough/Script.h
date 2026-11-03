#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

namespace gardener::walkthrough {

// One line of a walkthrough, as the file states it: the verb and the arguments that verb takes. A
// walk is the list of them, run top to bottom, so the file says what is drawn and the tool only
// drives what it says.
struct Command {
  enum class Verb : uint8_t {
    Load,
    Send,
    Wait,
    Advance,
    SetRate,
    SetFont,
    SetNote,
    ClearNote,
    Show,
    Clear,
    Emit
  };

  Verb verb = Verb::Emit;
  // The line it was read from, which every error about it names.
  int line = 0;

  // A load: the scene's name — home, browser or a routine's key — and the settings it is built
  // with, as the file wrote them, unconverted.
  std::string scene;
  std::vector<std::pair<std::string, std::string>> settings;

  // A send: one gesture, spelled as the panel's classifier spells it.
  std::string gesture;

  // A wait or an advance, in whole beats of 250 ms: the device time the line asked for, rounded up.
  // The two are the same beats and differ only in whether the frames are kept.
  int beats = 0;

  // A set-rate: the model chamber's pull at full duty and its venting rate, in kPa per second.
  float pullKpaPerSecond = 0.0f;
  float ventKpaPerSecond = 0.0f;

  // A set-font: the design every leaf that names no family draws in, by the name the fonts module
  // answers for it.
  std::string font;

  // A set-note: the line the band under the glass carries, as the file wrote it. It is the walk's
  // own prose, so it takes no quoting and nothing in it is a key or an argument.
  std::string note;

  // A show or a clear: the gestures or the button names the line lists, each trimmed, in the order
  // the file wrote them. A clear with none lists nothing, which is every button.
  std::vector<std::string> marks;
};

// Reads a whole walkthrough file into its commands, in order. A blank line and a `#` line are not
// commands, and every other line is one: a line that names no verb, a verb given the wrong argument
// or an argument that is not a number is refused with its own line number and nothing is run. The
// file also has to name an `emit`, as the last command, because a walk that writes nothing is work
// nobody sees.
bool readScript(const std::string& path, std::vector<Command>& commands, std::string& error);

}  // namespace gardener::walkthrough
