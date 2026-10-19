#include "app/BuzzerPlayer.h"

#include <math.h>

#include "core/Config.h"

namespace gardener {
namespace {

const char* skipSeparators(const char* text) {
  while (*text == ' ' || *text == '\t' || *text == ',' || *text == ':') text++;
  return text;
}

bool isSeparator(char c) {
  return c == ' ' || c == '\t' || c == ',' || c == ':';
}

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
  sequence_ = "";
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

bool BuzzerPlayer::play(const char* sequence) {
  std::vector<Note> notes;
  String canonical;
  if (!parse(sequence, notes, canonical) || notes.empty()) return false;

  notes_.swap(notes);
  sequence_ = canonical;
  noteIndex_ = 0;
  playing_ = true;
  timed_ = true;
  startNote(0, millis());
  return true;
}

void BuzzerPlayer::off() {
  notes_.clear();
  sequence_ = "";
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

// `note duration` pairs separated by whitespace, commas or colons. Note names are a-g, optional
// # or b, then one octave digit; `r` is a rest. The frequency uses equal temperament with A4 at
// 440 Hz, and a note outside the buzzer's range is a malformed sequence rather than a clamp.
bool BuzzerPlayer::parse(const char* text, std::vector<Note>& notes, String& canonical) {
  notes.clear();
  canonical = "";
  if (text == nullptr) return false;

  const char* cursor = text;
  bool any = false;

  for (;;) {
    cursor = skipSeparators(cursor);
    if (*cursor == '\0') break;

    const char* noteStart = cursor;
    uint16_t hz = 0;
    const char first = lowerChar(*cursor);

    if (first == 'r') {
      cursor++;
    } else {
      int semitone = semitoneFor(first);
      if (semitone < 0) return false;
      cursor++;
      if (*cursor == '#') {
        semitone++;
        cursor++;
      } else if (*cursor == 'b') {
        semitone--;
        cursor++;
      }
      if (*cursor < '0' || *cursor > '9') return false;
      const int octave = *cursor++ - '0';
      const int midi = (octave + 1) * 12 + semitone;
      if (midi < 0 || midi > 127) return false;
      const float frequency = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
      if (frequency < cfg::BUZZER_MIN_HZ || frequency > cfg::BUZZER_MAX_HZ) return false;
      hz = (uint16_t)(frequency + 0.5f);
    }
    const char* noteEnd = cursor;

    if (!isSeparator(*cursor)) return false;
    cursor = skipSeparators(cursor);

    if (*cursor < '0' || *cursor > '9') return false;
    uint32_t duration = 0;
    while (*cursor >= '0' && *cursor <= '9') {
      const uint32_t digit = (uint32_t)(*cursor - '0');
      if (duration > (UINT32_MAX - digit) / 10) return false;
      duration = duration * 10 + digit;
      cursor++;
    }
    if (duration == 0) return false;
    if (*cursor != '\0' && !isSeparator(*cursor)) return false;

    if (!notes.empty()) canonical += ',';
    for (const char* p = noteStart; p < noteEnd; p++) canonical += lowerChar(*p);
    canonical += ':';
    canonical += String(duration);

    notes.push_back({hz, duration});
    any = true;
  }

  return any;
}

}  // namespace gardener
