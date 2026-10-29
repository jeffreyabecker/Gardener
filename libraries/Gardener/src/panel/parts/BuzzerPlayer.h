#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "hal/Buzzer.h"

namespace gardener::panel::parts {

// The timed face of the piezo: one tone with a duration, or a sequence of note/duration pairs. It
// owns the schedule and drives Buzzer, which knows only a frequency and silence. Nothing here
// blocks: update(nowMs) is one entry per loop pass, like the rest of the app.
class BuzzerPlayer {
 public:
  explicit BuzzerPlayer(Buzzer& buzzer) : buzzer_(buzzer) {}

  bool begin();
  void update(uint32_t nowMs);

  // durationMs 0 means until off().
  void tone(uint16_t hz, uint32_t durationMs);
  // "a4:1000 c6:500 a4:500": note:duration pairs separated by spaces, and nothing else. False when
  // malformed, and `badToken` then points at the first token that did not parse; the current sound
  // is left alone in that case.
  bool play(const char* sequence, const char** badToken = nullptr);
  void off();

  // True when `sequence` parses as a note list, without starting it. The wire answers a refused
  // sequence with `badToken` at the first token that did not parse; the callers that enqueue a
  // play use this to build that reply, then the control task parses again inside play().
  static bool validate(const char* sequence, const char** badToken = nullptr);

  bool playing() const { return playing_; }
  bool sounding() const { return buzzer_.sounding(); }
  uint16_t frequencyHz() const { return buzzer_.frequencyHz(); }
  // Zero when nothing is playing or the tone is continuous.
  uint32_t remainingMs(uint32_t nowMs) const;

 private:
  struct Note {
    uint16_t hz;
    uint32_t durationMs;
  };

  static bool parse(const char* text, std::vector<Note>& notes, const char** badToken);
  void startNote(size_t index, uint32_t nowMs);

  Buzzer& buzzer_;
  std::vector<Note> notes_;
  size_t noteIndex_ = 0;
  uint32_t noteEndsMs_ = 0;
  bool playing_ = false;
  bool timed_ = false;
};

}  // namespace gardener::panel::parts
