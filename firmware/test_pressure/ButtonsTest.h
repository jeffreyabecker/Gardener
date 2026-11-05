#pragma once
#include <Gardener.h>
#include <Gfx.h>
#include <Runtime.h>

#include "hal/impl/Buttons.h"
#include "panel/parts/GestureClassifier.h"

// The panel keys through the driver the device reads them with, and the gestures the classifier
// makes of them. The register's three lines go to the constructor, which also takes the keys held at
// boot as levels rather than as presses. Every word the classifier answers is printed as it arrives
// - a single, a double, or a chord's parts in panel order - and a key held past the extended mark is
// printed as whole seconds once a second while it stays down, which is what a stuck key looks like.
// The keys down are printed at boot and again every thirty seconds.
class ButtonsTest {
 public:
  constexpr static uint32_t PollMs = 10;
  constexpr static uint32_t StateMs = 30000;

  ButtonsTest(uint8_t latch, uint8_t clock, uint8_t data)
      : buttons_(latch, clock, data), classifier_(buttons_) {}

  void setup() { printState(0); }

  void loop() {
    const uint32_t nowMs = millis();
    if (nowMs - lastPollMs_ < PollMs) return;
    lastPollMs_ = nowMs;

    // The classifier settles the reader itself, so the beat reads the keys through it rather than
    // beside it: a second reader on the same beat would take the presses before it saw them.
    const String gesture = classifier_.update();
    if (!gesture.isEmpty()) {
      String line = "gesture ";
      line += gesture;
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
  gardener::GestureClassifier classifier_;
  uint32_t lastPollMs_ = 0;
  uint32_t lastStateMs_ = 0;
};
