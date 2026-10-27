#pragma once

#include <memory>
#include <string>
#include <vector>

#include "BitmapPanel.h"
#include "BuzzerPlayer.h"
#include "Logger.h"
#include "MachineCommand.h"
#include "MemoryFiles.h"
#include "PanelContext.h"
#include "VacuumSystem.h"
#include "hal/Battery.h"
#include "hal/Buzzer.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"
#include "hal/impl/AdafruitDisplay.h"
#include "routines/Routines.h"
#include "storage/Config.h"
#include "storage/Jingles.h"

struct bvm;

namespace gardener {

// The model chamber's ceiling, in kPa of depth: the deepest the modelled pump pulls it. A rig that
// reaches deeper or shallower is this number, and `--ceiling-kpa` sets it.
constexpr float ChamberCeilingKpa = 70.0f;

// The chamber the check pulls against: depth below ambient, pulled in proportion to the duty and
// vented while the valve is open. It is the one thing here that is a model rather than a part,
// because a real chamber cannot be pulled four hundred times in a second.
class Chamber {
 public:
  explicit Chamber(float ceilingKpa) : ceilingKpa_(ceilingKpa) {}
  void advance(uint32_t ms);
  void reset();
  float pressureKpa() const { return pressureKpa_; }
  void setDuty(float percent) { duty_ = percent; }
  void setVenting(bool venting) { venting_ = venting; }
  bool venting() const { return venting_; }

 private:
  const float ceilingKpa_;
  float pressureKpa_ = 0.0f;
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
  float pressureKpa() const override { return chamber_.pressureKpa(); }

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
  HarnessPanel(Display& display, Config& config, JinglesConfig& jingles,
                VacuumSystem& vacuum, Battery& battery,
                BuzzerPlayer& buzzer, Library& library, Logger& logger)
      : display_(display),
        config_(config),
        jingles_(jingles),
        vacuum_(vacuum),
        battery_(battery),
        buzzer_(buzzer),
        library_(library),
        logger_(logger) {}

  Display& display() override { return display_; }
  Config& config() override { return config_; }
  JinglesConfig& jingles() override { return jingles_; }
  VacuumSystem& vacuum() override { return vacuum_; }
  Battery& battery() override { return battery_; }
  BuzzerPlayer& buzzer() override { return buzzer_; }
  Library& library() override { return library_; }
  Logger& logger() override { return logger_; }
  // The host models a device with both links up, so the band shows both icons.
  bool serialConnected() const override { return true; }
  bool bluetoothConnected() const override { return true; }
  // The host has no queue to fill, so the command is always taken.
  bool post(const MachineCommand& command) override { return postMachineCommand(command); }

 private:
  Display& display_;
  Config& config_;
  JinglesConfig& jingles_;
  VacuumSystem& vacuum_;
  Battery& battery_;
  BuzzerPlayer& buzzer_;
  Library& library_;
  Logger& logger_;
};

// Everything the device's panel brings up, on the host: the image, the storage, the settings, the
// panel, the chamber, the interpreter.
class HarnessSession {
 public:
  explicit HarnessSession(std::string dataDir, float ceilingKpa);
  ~HarnessSession();

  HarnessSession(const HarnessSession&) = delete;
  HarnessSession& operator=(const HarnessSession&) = delete;

  // Loads the image, brings the parts up and loads the routines into the interpreter, in the order
  // Panel::boot does. False when the data directory is not there or the VM cannot be made.
  bool begin();

  bvm* vm() const { return vm_; }
  Routines& routines() { return routines_; }
  BitmapPanel& panel() { return display_.driver(); }
  MemoryFiles& files() { return files_; }
  BufferLogger& logger() { return logger_; }
  CountingBuzzer& buzzerPart() { return buzzerPart_; }

  // One beat: 250 ms of the device's loop, four control ticks and one page poll.
  void beat(int count);
  // The panel's own Back hold: the stop command, as panelTask fires it.
  void backHold();
  // Leaves the page under test and puts the chamber back at rest.
  void reset();
  bool active() const { return routines_.active(); }

  // --- capture -------------------------------------------------------------
  // Arms capture: every beat from now snapshots the frame, and endCapture() writes them as a GIF.
  void beginCapture(const char* path);
  // Writes the captured frames to the armed path; false when there was nothing to write.
  bool endCapture();
  // Writes the frame on screen now as a single-frame GIF, with no beat in between: the still the
  // inspection commands and the chrome/component cases save.
  bool saveStill(const char* path);
  // The stem the test runner names a per-case GIF from: build/runs.interval.a case.gif.
  void setCaptureStem(const std::string& stem) { captureStem_ = stem; }
  const std::string& captureStem() const { return captureStem_; }
  // Records a gesture for the callout strip, shown on the next GestureCalloutBeats captures.
  void pressGesture(const char* gesture);
  // Marks the overlay band in every frame captured from now on: the suite framework hashes those
  // rows, because the band is the panel's own and no case draws it.
  void markOverlay(int16_t top, int16_t rows);

 private:
  std::string dataDir_;
  std::string capturePath_;
  std::string captureStem_ = "build/runs";
  std::vector<std::vector<uint8_t>> captureFrames_;
  bool capturing_ = false;
  std::string gesture_;
  int gestureBeats_ = 0;
  int16_t overlayTop_ = -1;
  int16_t overlayRows_ = 0;

  MemoryFiles files_;
  BufferLogger logger_;
  Config config_;
  JinglesConfig jingles_;
  Chamber chamber_;
  ModelPump pump_;
  ModelSensor sensor_;
  ModelValve valve_;
  VacuumSystem vacuum_;
  AdafruitDisplay<BitmapPanel> display_;
  FixedBattery battery_;
  CountingBuzzer buzzerPart_;
  BuzzerPlayer buzzer_;
  Routines routines_;
  HarnessPanel context_;
  bvm* vm_ = nullptr;
};

void installHarnessModule(bvm* vm, HarnessSession& appSession);

}  // namespace gardener
