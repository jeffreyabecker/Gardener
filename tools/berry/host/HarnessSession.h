#pragma once

#include <FS.h>

#include <memory>
#include <string>

#include "BitmapPanel.h"
#include "BuzzerPlayer.h"
#include "Logger.h"
#include "MemoryFS.h"
#include "PanelContext.h"
#include "VacuumSystem.h"
#include "hal/Battery.h"
#include "hal/Buzzer.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"
#include "hal/impl/AdafruitDisplay.h"
#include "panel/Overlay.h"
#include "routines/Routines.h"
#include "storage/PanelSettings.h"

struct bvm;

namespace gardener {

// The chamber the check pulls against: depth below ambient, pulled in proportion to the duty and
// vented while the valve is open. It is the one thing here that is a model rather than a part,
// because a real chamber cannot be pulled four hundred times in a second.
class Chamber {
 public:
  void advance(uint32_t ms);
  void reset();
  float kpa() const { return kpa_; }
  void setDuty(float percent) { duty_ = percent; }
  void setVenting(bool venting) { venting_ = venting; }
  bool venting() const { return venting_; }

 private:
  float kpa_ = 0.0f;
  float duty_ = 0.0f;
  bool venting_ = true;
};

class ModelPump : public Pump {
 public:
  explicit ModelPump(Chamber& chamber) : chamber_(chamber) {}

  bool begin() override { return true; }
  void update(uint32_t nowMs) override;
  void setDuty(float percent) override { requested_ = percent; }
  void stop() override { requested_ = 0.0f; }
  void halt() override {
    requested_ = 0.0f;
    duty_ = 0.0f;
    chamber_.setDuty(0.0f);
  }
  float dutyPercent() const override { return duty_; }

 private:
  Chamber& chamber_;
  float requested_ = 0.0f;
  float duty_ = 0.0f;
  uint32_t lastMs_ = 0;
};

class ModelSensor : public PressureSensor {
 public:
  explicit ModelSensor(Chamber& chamber) : chamber_(chamber) {}

  bool begin() override {
    present_ = true;
    return true;
  }
  void update(uint32_t) override {}
  bool present() const override { return present_; }
  bool readingValid() const override { return present_; }
  float kpa() const override { return chamber_.kpa(); }

 private:
  Chamber& chamber_;
  bool present_ = false;
};

class ModelValve : public VentValve {
 public:
  explicit ModelValve(Chamber& chamber) : chamber_(chamber) {}

  void begin() override { open(); }
  void open() override {
    open_ = true;
    chamber_.setVenting(true);
  }
  void close() override {
    open_ = false;
    chamber_.setVenting(false);
  }
  bool energised() const override { return false; }
  bool energiseWhenOpen() const override { return false; }

 private:
  Chamber& chamber_;
  bool open_ = true;
};

// The piezo, counted rather than played: the driver checks that a jingle started, not what it
// sounded like.
class CountingBuzzer : public Buzzer {
 public:
  bool begin() override { return true; }
  void tone(uint16_t hz) override {
    if (hz == 0) {
      sounding_ = false;
      return;
    }
    sounding_ = true;
    frequency_ = hz;
    sounds_++;
  }
  void off() override { sounding_ = false; }
  bool sounding() const override { return sounding_; }
  uint16_t frequencyHz() const override { return frequency_; }

  int sounds() const { return sounds_; }

 private:
  bool sounding_ = false;
  uint16_t frequency_ = 0;
  int sounds_ = 0;
};

class FixedBattery : public Battery {
 public:
  bool begin() override { return true; }
  void update(uint32_t) override {}
  bool present() const override { return true; }
  bool low() const override { return false; }
  float volts() const override { return 4.0f; }
  uint8_t percent() const override { return 100; }
};

// The diagnostics the firmware writes, kept as the driver's log rather than printed as they happen,
// so a run's report reads in one block.
class BufferLogger : public Logger {
 public:
  String levels() const override { return levels_; }
  void setLevels(const String& levels) override { levels_ = levels; }

  void info(const char* text) override { append("[I] ", text); }
  void warn(const char* text) override { append("[W] ", text); }
  void error(const char* text) override { append("[E] ", text); }

  std::string text() const { return text_; }

 private:
  void append(const char* level, const char* text) {
    text_ += level;
    text_ += text != nullptr ? text : "";
    text_ += '\n';
  }

  String levels_ = "IWE";
  std::string text_;
};

// The parts a Panel is on the device, composed without one: the interpreter reads this, and it is
// the only thing the Berry modules know about their surroundings.
class HarnessPanel : public PanelContext {
 public:
  HarnessPanel(Display& display, PanelSettings& settings, VacuumSystem& vacuum, Battery& battery,
                BuzzerPlayer& buzzer, FS& fs, Logger& logger)
      : display_(display),
        settings_(settings),
        vacuum_(vacuum),
        battery_(battery),
        buzzer_(buzzer),
        fs_(fs),
        logger_(logger) {}

  Display& display() override { return display_; }
  PanelSettings& settings() override { return settings_; }
  VacuumSystem& vacuum() override { return vacuum_; }
  Battery& battery() override { return battery_; }
  BuzzerPlayer& buzzer() override { return buzzer_; }
  FS& fs() override { return fs_; }
  Logger& logger() override { return logger_; }

 private:
  Display& display_;
  PanelSettings& settings_;
  VacuumSystem& vacuum_;
  Battery& battery_;
  BuzzerPlayer& buzzer_;
  FS& fs_;
  Logger& logger_;
};

// Everything the device's panel brings up, on the host: the image, the storage, the settings, the
// panel, the chamber, the interpreter.
class HarnessSession {
 public:
  explicit HarnessSession(std::string dataDir);
  ~HarnessSession();

  HarnessSession(const HarnessSession&) = delete;
  HarnessSession& operator=(const HarnessSession&) = delete;

  // Loads the image, brings the parts up and loads the routines into the interpreter, in the order
  // Panel::boot does. False when the data directory is not there or the VM cannot be made.
  bool begin();

  bvm* vm() const { return vm_; }
  Routines& routines() { return routines_; }
  BitmapPanel& panel() { return panel_; }
  MemoryFS& fs() { return fs_; }
  BufferLogger& logger() { return logger_; }
  CountingBuzzer& buzzerPart() { return buzzerPart_; }

  // One beat: 250 ms of the device's loop, four control ticks and one page poll.
  void beat(int count);
  // The panel's own Back hold: the stop notice and the command, as panelTask fires them.
  void backHold();
  // Leaves the page under test and puts the chamber back at rest.
  void reset();
  bool active() const { return routines_.active(); }

 private:
  std::string dataDir_;

  MemoryFS fs_;
  BufferLogger logger_;
  PanelSettings settings_;
  Chamber chamber_;
  ModelPump pump_;
  ModelSensor sensor_;
  ModelValve valve_;
  VacuumSystem vacuum_;
  BitmapPanel panel_;
  AdafruitDisplay<BitmapPanel> display_;
  FixedBattery battery_;
  CountingBuzzer buzzerPart_;
  BuzzerPlayer buzzer_;
  Overlay overlay_;
  Routines routines_;
  HarnessPanel context_;
  bvm* vm_ = nullptr;
};

void installHarnessModule(bvm* vm, HarnessSession& session);

}  // namespace gardener
