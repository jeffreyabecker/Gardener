#pragma once

// The parts a case hands a domain or the control law: each implements the part interface it stands
// for and keeps the state a case reads back, so a case can assert what was done to the part and not
// only what was printed.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "core/Logger.h"
#include "core/PressureSensor.h"
#include "core/Pump.h"
#include "core/VentValve.h"

namespace gardener::test {

// The pump: whether it is being driven, and the calls in the order the loop made them. An optional
// event log is how a case checks that a release stops the pump before it opens the valve.
class FakePump : public Pump {
 public:
  explicit FakePump(std::vector<std::string>* events = nullptr) : events_(events) {}

  void begin() override {}
  void on() override {
    on_ = true;
    note("pump.on");
  }
  void off() override {
    on_ = false;
    note("pump.off");
  }
  bool active() const override { return on_; }

 private:
  void note(const char* what) {
    if (events_ != nullptr) events_->push_back(what);
  }

  std::vector<std::string>* events_;
  bool on_ = false;
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
    note("valve.open");
  }
  void close() override {
    vents_ = false;
    note("valve.close");
  }
  bool energiseWhenOpen() const override { return energiseWhenOpen_; }

  bool vents() const { return vents_; }

 private:
  void note(const char* what) {
    if (events_ != nullptr) events_->push_back(what);
  }

  bool energiseWhenOpen_;
  std::vector<std::string>* events_;
  bool vents_ = false;
};

// The one sink: every line a domain logs, in order, with the level it came in at.
class RecordingLogger : public Logger {
 public:
  void info(const char* text) override { log('I', text); }
  void warn(const char* text) override { log('W', text); }

  // Whether any line so far contains `text`.
  bool saw(const char* text) const {
    for (const std::string& line : lines_) {
      if (line.find(text) != std::string::npos) return true;
    }
    return false;
  }

  const std::vector<std::string>& lines() const { return lines_; }

 private:
  void log(char level, const char* text) {
    std::string line;
    line += level;
    line += ' ';
    line += text == nullptr ? "" : text;
    lines_.push_back(line);
  }

  std::vector<std::string> lines_;
};

}  // namespace gardener::test
