#include "Build.h"

#if GARDENER_PANEL

#include "panel/parts/BuzzerPlayer.h"

#include <math.h>

namespace gardener {
namespace {

const char* skipSpaces(const char* text) {
  while (*text == ' ' || *text == '\t') text++;
  return text;
}

bool isSpace(char c) { return c == ' ' || c == '\t'; }

char lowerChar(char c) {
  return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

int semitoneFor(char note) {
  switch (note) {
    case 'c': return 0;
    case 'd': return 2;
    case 'e': return 4;
    case 'f': return 5;
    case 'g': return 7;
    case 'a': return 9;
    case 'b': return 11;
    default: return -1;
  }
}

bool fail(const char** badToken, const char* at) {
  if (badToken) *badToken = at;
  return false;
}

}  // namespace

bool BuzzerPlayer::begin() { return buzzer_.begin(); }

// One note at a time, advanced by update(). A sequence keeps its own cursor, so a late loop pass
// moves the rhythm on rather than stretching it.
void BuzzerPlayer::update(uint32_t nowMs) {
  if (!playing_ || !timed_) return;
  if ((int32_t)(nowMs - noteEndsMs_) < 0) return;

  if (noteIndex_ + 1 < notes_.size()) {
    noteIndex_++;
    startNote(noteIndex_, noteEndsMs_);
    return;
  }
  off();
}

void BuzzerPlayer::tone(uint16_t hz, uint32_t durationMs) {
  if (hz == 0) {
    off();
    return;
  }

  notes_.clear();
  noteIndex_ = 0;
  buzzer_.tone(hz);
  if (!buzzer_.sounding()) {
    off();
    return;
  }
  playing_ = true;
  timed_ = durationMs > 0;
  noteEndsMs_ = millis() + durationMs;
}

bool BuzzerPlayer::play(const char* sequence, const char** badToken) {
  std::vector<Note> notes;
  if (!parse(sequence, notes, badToken)) return false;

  notes_.swap(notes);
  noteIndex_ = 0;
  playing_ = true;
  timed_ = true;
  startNote(0, millis());
  return true;
}

bool BuzzerPlayer::validate(const char* sequence, const char** badToken) {
  std::vector<Note> notes;
  return parse(sequence, notes, badToken);
}

void BuzzerPlayer::off() {
  notes_.clear();
  noteIndex_ = 0;
  playing_ = false;
  timed_ = false;
  buzzer_.off();
}

uint32_t BuzzerPlayer::remainingMs(uint32_t nowMs) const {
  if (!playing_ || !timed_) return 0;
  const int32_t left = (int32_t)(noteEndsMs_ - nowMs);
  return left > 0 ? (uint32_t)left : 0;
}

void BuzzerPlayer::startNote(size_t index, uint32_t nowMs) {
  if (notes_[index].hz == 0) {
    buzzer_.off();
  } else {
    buzzer_.tone(notes_[index].hz);
  }
  noteEndsMs_ = nowMs + notes_[index].durationMs;
}

// `note:duration` pairs separated by whitespace, and nothing else. Note names are a-g, optional
// # or b, then one octave digit; `r` is a rest. The frequency uses equal temperament with A4 at
// 440 Hz, and a note outside the buzzer's range is a malformed sequence rather than a clamp.
bool BuzzerPlayer::parse(const char* text, std::vector<Note>& notes, const char** badToken) {
  notes.clear();
  if (badToken) *badToken = nullptr;
  if (text == nullptr) return false;

  const char* cursor = text;
  bool any = false;

  for (;;) {
    cursor = skipSpaces(cursor);
    if (*cursor == '\0') break;

    const char* noteStart = cursor;
    uint16_t hz = 0;
    const char first = lowerChar(*cursor);

    if (first == 'r') {
      cursor++;
    } else {
      int semitone = semitoneFor(first);
      if (semitone < 0) return fail(badToken, noteStart);
      cursor++;
      if (*cursor == '#') {
        semitone++;
        cursor++;
      } else if (*cursor == 'b') {
        semitone--;
        cursor++;
      }
      if (*cursor < '0' || *cursor > '9') return fail(badToken, noteStart);
      const int octave = *cursor++ - '0';
      const int midi = (octave + 1) * 12 + semitone;
      if (midi < 0 || midi > 127) return fail(badToken, noteStart);
      const float frequency = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
      if (frequency < BuzzerMinHz || frequency > BuzzerMaxHz) {
        return fail(badToken, noteStart);
      }
      hz = (uint16_t)(frequency + 0.5f);
    }

    if (*cursor != ':') return fail(badToken, noteStart);
    cursor++;

    if (*cursor < '0' || *cursor > '9') return fail(badToken, noteStart);
    uint32_t duration = 0;
    while (*cursor >= '0' && *cursor <= '9') {
      const uint32_t digit = (uint32_t)(*cursor - '0');
      if (duration > (UINT32_MAX - digit) / 10) return fail(badToken, noteStart);
      duration = duration * 10 + digit;
      cursor++;
    }
    if (duration == 0) return fail(badToken, noteStart);
    if (*cursor != '\0' && !isSpace(*cursor)) return fail(badToken, noteStart);

    notes.push_back({hz, duration});
    any = true;
  }

  if (!any) return fail(badToken, text);
  return true;
}

}  // namespace gardener
#endif
