#include <Arduino.h>
#include <stdint.h>

#include "core/Button.h"
#include "core/Gestures.h"
#include "machine/MachineCommand.h"
#include "panel/GestureQueue.h"
#include "serial/SerialLink.h"

namespace gardener {
namespace {

// The button a part names, or -1 when the panel has none. The index is the panel's order, which is
// the order a chord's parts must stand in.
int8_t buttonIndex(const String& name) {
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if (name == buttonName((Button)i)) return (int8_t)i;
  }
  return -1;
}

// A form after the colon: the two presses, or whole seconds. Digits are checked one by one because
// toInt() reads `2s` as two and whatever it cannot parse as zero.
bool isForm(const String& form) {
  if (form == "single" || form == "double") return true;
  if (form.isEmpty()) return false;
  for (size_t i = 0; i < form.length(); i++) {
    if (form.charAt(i) < '0' || form.charAt(i) > '9') return false;
  }
  return form.toInt() >= 1;
}

// True when the word is one or more `<button>:<form>` parts joined by '+', in the order the buttons
// sit on the panel and with no button twice. `button <= previous` is both the order and the repeat.
bool isButtonChord(const String& word) {
  int8_t previous = -1;
  size_t start = 0;
  for (;;) {
    const int plus = word.indexOf('+', start);
    const String part = plus < 0 ? word.substring(start) : word.substring(start, (size_t)plus);
    const int colon = part.indexOf(':');
    if (colon <= 0) return false;

    const int8_t button = buttonIndex(part.substring(0, (size_t)colon));
    const String form = part.substring((size_t)colon + 1);
    if (button < 0 || button <= previous || !isForm(form)) return false;

    previous = button;
    if (plus < 0) return true;
    start = (size_t)plus + 1;
  }
}

// The words a link may send. A `cmd:` word is the routine machine's and is carried unchanged; the
// two `system:` words are the panel's safety holds. Neither may be part of a chord.
bool isLinkWord(const String& word) {
  if (word.isEmpty()) return false;
  if (word.startsWith("cmd:")) return word.length() > 4 && word.indexOf('+') < 0;
  if (word.startsWith("system:")) {
    return Gestures::is(word.c_str(), Gestures::SystemReboot) ||
           Gestures::is(word.c_str(), Gestures::SystemQuit);
  }
  return isButtonChord(word);
}

}  // namespace

// The panel's input as one word: a link presses a key and the word is the press. Nothing is answered
// — a press is an event, not a request — and a word the grammar will not take is dropped without a
// sound. The two safety words are the link's because a posted word never passes the classifier that
// names them; everything else goes to the queue the keys fill.
void SerialLink::executeGesture(Stream& io) {
  String word = io.readStringUntil('\n');
  word.trim();

  // A line no grammar can hold is the link's own error, as it is for the other four.
  if (word.length() > CommandLineMax) {
    io.print("err bad_argument\n");
    return;
  }
  if (!isLinkWord(word)) return;

  if (Gestures::is(word.c_str(), Gestures::SystemReboot)) {
    queue_.post(MachineCommand{MachineCommand::Op::Reboot});
    return;
  }
  if (Gestures::is(word.c_str(), Gestures::SystemQuit)) {
    // The panel's hold stops, vents and asks for the root page; a word over the wire can do only the
    // first. Sending the screen home as well is still to do.
    queue_.post(MachineCommand{MachineCommand::Op::Stop});
    return;
  }

  parts_.presses.post(word.c_str());
}

}  // namespace gardener
