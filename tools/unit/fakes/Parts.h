#pragma once

// The parts a case hands a domain or the control law: each implements the hal/ interface it stands
// for and keeps the state a case reads back, so a case can assert what was done to the part and not
// only what was printed.

#include <stdint.h>

#include <string>
#include <vector>

#include "core/Logger.h"
#include "hal/Buzzer.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace gardener::test {

// The pump: the duty it was last asked for, and whether it was told to stop or halt. An optional
// event log records the calls in the order they came, which is how a case checks that a release
// stops the pump before it opens the valve.
class FakePump : public Pump {
 public:
  explicit FakePump(std::vector<std::string>* events = nullptr) : events_(events) {}

  bool begin() override { return true; }
  void update(uint32_t) override {}
  void setDuty(float percent) override {
    duty_ = percent;
    note("pump.duty");
  }
  void stop() override {
    stopped_ = true;
    halted_ = false;
    duty_ = 0.0f;
    note("pump.stop");
  }
  void halt() override {
    halted_ = true;
    stopped_ = false;
    duty_ = 0.0f;
    note("pump.halt");
  }
  float dutyPercent() const override { return duty_; }

  void clear() {
    stopped_ = false;
    halted_ = false;
    duty_ = 0.0f;
  }

  float duty() const { return duty_; }
  bool stopped() const { return stopped_; }
  bool halted() const { return halted_; }

 private:
  void note(const char* what) {
    if (events_ != nullptr) events_->push_back(what);
  }

  std::vector<std::string>* events_;
  float duty_ = 0.0f;
  bool stopped_ = false;
  bool halted_ = false;
};

// The sensor: a depth a case sets, and flags for the two ways it goes quiet.
class FakeSensor : public PressureSensor {
 public:
  bool begin() override { return true; }
  void update(uint32_t) override {}

  bool present() const override { return present_; }
  bool readingValid() const override { return valid_; }
  float pressureKpa() const override { return valid_ ? kpa_ : 0.0f; }

  // A reading the part is confident in.
  void read(float kpa) {
    kpa_ = kpa;
    present_ = true;
    valid_ = true;
  }
  // The part stops answering: present() false and no reading.
  void detach() {
    present_ = false;
    valid_ = false;
  }
  // The part answers but has no filtered sample yet.
  void warmUp() {
    present_ = true;
    valid_ = false;
  }

 private:
  float kpa_ = 0.0f;
  bool present_ = true;
  bool valid_ = false;
};

// The valve: whether it is being driven to vent, and which way the fitted coil works.
class FakeValve : public VentValve {
 public:
  explicit FakeValve(bool energiseWhenOpen = false, std::vector<std::string>* events = nullptr)
      : energiseWhenOpen_(energiseWhenOpen), events_(events) {}

  void begin() override {}
  void open() override {
    vents_ = true;
    energised_ = energiseWhenOpen_;
    note("valve.open");
  }
  void close() override {
    vents_ = false;
    energised_ = !energiseWhenOpen_;
    note("valve.close");
  }
  bool energised() const override { return energised_; }
  bool energiseWhenOpen() const override { return energiseWhenOpen_; }

  bool vents() const { return vents_; }

 private:
  void note(const char* what) {
    if (events_ != nullptr) events_->push_back(what);
  }

  bool energiseWhenOpen_;
  std::vector<std::string>* events_;
  bool vents_ = false;
  bool energised_ = false;
};

// The piezo: the level it is actually driving, which a case reads back.
class FakeBuzzer : public Buzzer {
 public:
  bool begin() override { return true; }
  void tone(uint16_t hz) override {
    hz_ = hz;
    sounding_ = hz != 0;
  }
  void off() override {
    hz_ = 0;
    sounding_ = false;
  }
  bool sounding() const override { return sounding_; }
  uint16_t frequencyHz() const override { return hz_; }

 private:
  uint16_t hz_ = 0;
  bool sounding_ = false;
};

// The one sink: every line a domain logs, in order, with the level it came in at.
class RecordingLogger : public Logger {
 public:
  String levels() const override { return levels_; }
  void setLevels(const String& levels) override { levels_ = levels; }
  void info(const char* text) override { log('I', text); }
  void warn(const char* text) override { log('W', text); }
  void error(const char* text) override { log('E', text); }

  // Whether any line so far contains `text`.
  bool saw(const char* text) const {
    for (const std::string& line : lines_) {
      if (line.find(text) != std::string::npos) return true;
    }
    return false;
  }

  const std::vector<std::string>& lines() const { return lines_; }
  void clear() { lines_.clear(); }

 private:
  void log(char level, const char* text) {
    std::string line;
    line += level;
    line += ' ';
    line += text == nullptr ? "" : text;
    lines_.push_back(line);
  }

  String levels_ = "IWE";
  std::vector<std::string> lines_;
};

}  // namespace gardener::test
