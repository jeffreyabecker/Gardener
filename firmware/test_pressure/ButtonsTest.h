#pragma once
#include <Gardener.h>
#include <Gfx.h>
#include <Runtime.h>

#include "hal/impl/Buttons.h"

// The panel keys through the driver the device reads them with. The register's three lines go to the
// constructor, which also takes the keys held at boot as levels rather than as presses. Every press
// the driver classifies is printed as it arrives - a single or a double - and a key held down is
// printed as whole seconds while it stays down, which is what a stuck key looks like. The keys down
// are printed at boot and again every thirty seconds.
class ButtonsTest {
 public:
  constexpr static uint32_t PollMs = 10;
  constexpr static uint32_t StateMs = 30000;

  ButtonsTest(uint8_t latch, uint8_t clock, uint8_t data) : buttons_(latch, clock, data) {}

  void setup() { printState(0); }

  void loop() {
    const uint32_t nowMs = millis();
    if (nowMs - lastPollMs_ < PollMs) return;
    lastPollMs_ = nowMs;

    for (uint8_t i = 0; i < gardener::ButtonCount; i++) {
      const gardener::Button button = (gardener::Button)i;

      const gardener::Press press = buttons_.consumePress(button);
      if (press != gardener::Press::None) {
        String line = "key ";
        line += gardener::buttonName(button);
        line += press == gardener::Press::Double ? " double" : " single";
        Serial.println(line);
      }

      // A hold is not a press, so the driver answers it as seconds held rather than delivering one;
      // a line a second is the report of a key that is still down.
      const uint32_t seconds = buttons_.heldMs(button) / 1000;
      if (seconds == heldSeconds_[i]) continue;
      heldSeconds_[i] = seconds;
      if (seconds == 0) continue;

      String line = "key ";
      line += gardener::buttonName(button);
      line += " held ";
      line += String(seconds);
      line += " s";
      Serial.println(line);
    }

    if (nowMs - lastStateMs_ < StateMs) return;
    lastStateMs_ = nowMs;
    printState(nowMs);
  }

 private:
  void printState(uint32_t nowMs) {
    String line = "keys at ";
    line += String(nowMs / 1000);
    line += " s, down:";
    bool any = false;
    for (uint8_t i = 0; i < gardener::ButtonCount; i++) {
      const gardener::Button button = (gardener::Button)i;
      if (!buttons_.pressed(button)) continue;
      line += ' ';
      line += gardener::buttonName(button);
      any = true;
    }
    if (!any) line += " none";
    Serial.println(line);
  }

  gardener::runtime::Buttons buttons_;
  uint32_t lastPollMs_ = 0;
  uint32_t lastStateMs_ = 0;
  uint32_t heldSeconds_[gardener::ButtonCount] = {};
};
