// Button check for an ESP32-S3 Super Mini.
//
// Independent of the Gardener firmware: no library, no data image, no board - one 74HC165 shift
// register with eight switches on it and the USB serial port. The full state prints at startup and
// every thirty seconds; in between, each press and release is reported as it settles.

#include <Arduino.h>

namespace {

// The register's lines by its datasheet's names. Three reach the ESP32 - /load on 10, clk on 11 and
// qh on 1 - and are the panel's wiring in gardener.ino, so the same harness drives both sketches.
// ser and clk_inh take no pin: both are tied to ground, ser because the cascade input shifts
// nothing into a lone register, clk_inh because low is the clock running.
constexpr uint8_t LoadPin = 10;
constexpr uint8_t ClkPin = 11;
constexpr uint8_t QhPin = 1;

constexpr uint8_t ButtonCount = 8;

// The register has no change output, so there is no edge to catch: it is read on this beat and a
// level is believed only once it has held still for the debounce, as the firmware's panel does.
constexpr uint32_t ReadMs = 10;
constexpr uint32_t DebounceMs = 20;

// The full state, at boot and then this often.
constexpr uint32_t FullStateMs = 30000;

uint8_t pending = 0;
uint8_t stable = 0;
uint32_t pendingSinceMs = 0;
uint32_t lastReadMs = 0;
uint32_t lastFullMs = 0;

// A low on qh is a closed switch, so a set bit here is a button held down. The register presents D7
// first and shifts towards D0, so bit 0 is input D7.
uint8_t readButtons() {
  digitalWrite(LoadPin, LOW);
  digitalWrite(LoadPin, HIGH);

  uint8_t mask = 0;
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if (digitalRead(QhPin) == LOW) mask |= (uint8_t)(1u << i);
    digitalWrite(ClkPin, HIGH);
    digitalWrite(ClkPin, LOW);
  }
  return mask;
}

void printState() {
  String line = "state ";
  for (uint8_t i = 0; i < ButtonCount; i++)
    line += (stable & (1u << i)) ? '1' : '0';

  line += " at ";
  line += String(millis() / 1000);
  line += " s, down:";
  bool any = false;
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if (!(stable & (1u << i))) continue;
    line += ' ';
    line += String(i + 1);
    any = true;
  }
  if (!any) line += " none";
  Serial.println(line);
}

// One line per button whose level changed, the down edges first so a press reads before its
// release.
void printEdges(uint8_t before, uint8_t after) {
  for (uint8_t i = 0; i < ButtonCount; i++) {
    const uint8_t bit = (uint8_t)(1u << i);
    if ((before & bit) || !(after & bit)) continue;
    String line = "button ";
    line += String(i + 1);
    line += " down, after ";
    line += String(millis() / 1000);
    line += " s";
    Serial.println(line);
  }
  for (uint8_t i = 0; i < ButtonCount; i++) {
    const uint8_t bit = (uint8_t)(1u << i);
    if (!(before & bit) || (after & bit)) continue;
    String line = "button ";
    line += String(i + 1);
    line += " up, after ";
    line += String(millis() / 1000);
    line += " s";
    Serial.println(line);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  // /load high and clk low is the shift state: a low pulse on /load loads the switches, and each
  // clock after it moves the next one out on qh.
  pinMode(LoadPin, OUTPUT);
  pinMode(ClkPin, OUTPUT);
  digitalWrite(LoadPin, HIGH);
  digitalWrite(ClkPin, LOW);
  // The register drives qh, so this is a plain input; the switches pull up at the register.
  pinMode(QhPin, INPUT);

  // A button held at boot is the starting level, not a press.
  stable = readButtons();
  pending = stable;
  printState();
}

void loop() {
  const uint32_t nowMs = millis();

  if (nowMs - lastReadMs < ReadMs) return;
  lastReadMs = nowMs;

  const uint8_t raw = readButtons();
  if (raw != pending) {
    pending = raw;
    pendingSinceMs = nowMs;
  } else if (pending != stable && nowMs - pendingSinceMs >= DebounceMs) {
    printEdges(stable, pending);
    stable = pending;
  }

  if (nowMs - lastFullMs < FullStateMs) return;
  lastFullMs = nowMs;
  printState();
}
