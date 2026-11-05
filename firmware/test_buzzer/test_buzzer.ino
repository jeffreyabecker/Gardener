// Buzzer check for an ESP32-S3 Super Mini.
//
// Independent of the Gardener firmware: no library, no data image, no board - one piezo on one pin
// and the USB serial port. The jingle is `note:duration` pairs in the same syntax the firmware's
// jingles.ini uses; it plays a few times with a pause between passes, then stops for good.

#include <Arduino.h>
#include <math.h>

namespace {

// The piezo. A passive element driven by one LEDC channel, as PwmBuzzer drives the fitted part.
constexpr uint8_t BuzzerPin = 5;
constexpr uint32_t IdleHz = 1000;
constexpr uint8_t PwmResolution = 10;

// The range the fitted piezo reproduces. A note outside it is refused rather than clamped.
constexpr float BuzzerMinHz = 100.0f;
constexpr float BuzzerMaxHz = 10000.0f;

// The jingle: a-g, an optional # or b, one octave digit, `r` for a rest, then milliseconds.
const char* const Jingle = "E5:200 G5:200 E6:200 C6:200 D6:200 G6:200 G5:400 C6:200 G5:200 E5:200 "
                           "C5:200 F5:200 D5:200 C5:400 E5:200 G5:200 C6:200 E6:200 D6:200 C6:200 "
                           "B5:400 A5:200 C6:200 F6:200 A6:200 G6:200 F6:200 E6:400";

// The silence between one play and the next, and how many times the jingle plays at all.
constexpr uint32_t RepeatMs = 2000;
constexpr uint8_t Passes = 3;

constexpr uint16_t MaxNotes = 1024;

struct Note {
  uint16_t hz;  // zero is a rest
  uint32_t durationMs;
};

Note notes[MaxNotes];
uint16_t noteCount = 0;

uint16_t noteIndex = 0;
uint8_t passesPlayed = 0;
uint32_t noteEndsMs = 0;
uint32_t pauseEndsMs = 0;
bool playing = false;

char lowerChar(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

int semitoneFor(char note) {
  switch (note) {
    case 'c':
      return 0;
    case 'd':
      return 2;
    case 'e':
      return 4;
    case 'f':
      return 5;
    case 'g':
      return 7;
    case 'a':
      return 9;
    case 'b':
      return 11;
    default:
      return -1;
  }
}

// `note:duration` pairs separated by whitespace, and nothing else. The frequency uses equal
// temperament with A4 at 440 Hz.
bool parse(const char* text) {
  noteCount = 0;
  const char* cursor = text;

  for (;;) {
    while (*cursor == ' ' || *cursor == '\t')
      cursor++;
    if (*cursor == '\0') break;
    if (noteCount >= MaxNotes) return false;

    uint16_t hz = 0;
    if (lowerChar(*cursor) == 'r') {
      cursor++;
    } else {
      int semitone = semitoneFor(lowerChar(*cursor));
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
      const float frequency = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
      if (frequency < BuzzerMinHz || frequency > BuzzerMaxHz) return false;
      hz = (uint16_t)(frequency + 0.5f);
    }

    if (*cursor != ':') return false;
    cursor++;
    if (*cursor < '0' || *cursor > '9') return false;

    uint32_t duration = 0;
    while (*cursor >= '0' && *cursor <= '9')
      duration = duration * 10 + (uint32_t)(*cursor++ - '0');
    if (duration == 0) return false;
    if (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') return false;

    notes[noteCount++] = {hz, duration};
  }

  return noteCount > 0;
}

void tone(uint16_t hz) {
  if (hz == 0 || hz < BuzzerMinHz || hz > BuzzerMaxHz) {
    ledcWrite(BuzzerPin, 0);
    return;
  }
  ledcWriteTone(BuzzerPin, hz);
}

// One note at a time, advanced by loop(). The note's end is kept rather than a delay, so a late
// pass moves the rhythm on instead of stretching it.
void startNote(uint16_t index, uint32_t nowMs) {
  tone(notes[index].hz);
  noteEndsMs = nowMs + notes[index].durationMs;

  Serial.print("note ");
  Serial.print(index + 1);
  Serial.print('/');
  Serial.print(noteCount);
  Serial.print(", ");
  if (notes[index].hz == 0) {
    Serial.print("rest");
  } else {
    Serial.print(notes[index].hz);
    Serial.print(" Hz");
  }
  Serial.print(", ");
  Serial.print(notes[index].durationMs);
  Serial.println(" ms");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BuzzerPin, OUTPUT);
  digitalWrite(BuzzerPin, LOW);
  if (!ledcAttach(BuzzerPin, IdleHz, PwmResolution)) {
    Serial.println("buzzer: channel not attached");
    return;
  }
  ledcWrite(BuzzerPin, 0);

  if (!parse(Jingle)) {
    Serial.println("jingle: rejected");
    return;
  }
  Serial.println(Jingle);
  Serial.print("jingle: ");
  Serial.print(noteCount);
  Serial.println(" notes");

  // The first pass starts now; loop() handles the pauses between passes.
  playing = true;
  passesPlayed = 1;
  startNote(0, millis());
}

void loop() {
  const uint32_t nowMs = millis();

  if (!playing) {
    if (passesPlayed >= Passes) return;
    if (nowMs - pauseEndsMs < RepeatMs) return;
    playing = true;
    noteIndex = 0;
    passesPlayed++;
    startNote(0, nowMs);
    return;
  }

  // A signed difference, so one millis() wrap does not park the player on a note for 49 days.
  if ((int32_t)(nowMs - noteEndsMs) < 0) return;

  if (noteIndex + 1 < noteCount) {
    startNote(++noteIndex, noteEndsMs);
    return;
  }

  ledcWrite(BuzzerPin, 0);
  playing = false;
  pauseEndsMs = noteEndsMs;
  if (passesPlayed >= Passes) Serial.println("done");
}
