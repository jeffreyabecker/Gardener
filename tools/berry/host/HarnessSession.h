#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "BerryInterpreter.h"
#include "BitmapPanel.h"
#include "core/Logger.h"
#include "gfx/Canvas.h"
#include "hal/Battery.h"
#include "hal/Buzzer.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "MemoryFiles.h"
#include "panel/BuzzerPlayer.h"
#include "panel/PanelContext.h"
#include "routines/Routines.h"
#include "storage/Config.h"

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

// The harness's queue, as a list: everything that writes the machine posts here and the beat drains
// it before its own tick, so a write is seen on the next beat as it is on the device rather than the
// instant it is made. A deque has no depth, so a post is always taken.
bool postMachineCommand(const MachineCommand& command);

// The parts a Panel is on the device, composed without one: the interpreter reads this, and it is
// the only thing the Berry modules know about their surroundings.
class HarnessPanel : public PanelContext {
 public:
  HarnessPanel(gfx::DisplayDevice& display, gfx::Canvas& canvas, Config& config,
               VacuumSystem& vacuum, Battery& battery, BuzzerPlayer& buzzer, Library& library,
               Logger& logger)
      : display_(display),
        canvas_(canvas),
        config_(config),
        vacuum_(vacuum),
        battery_(battery),
        buzzer_(buzzer),
        library_(library),
        logger_(logger) {}

  gfx::DisplayDevice& display() override { return display_; }
  gfx::Canvas& canvas() override { return canvas_; }
  Config& config() override { return config_; }
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
  gfx::DisplayDevice& display_;
  gfx::Canvas& canvas_;
  Config& config_;
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

  bvm* vm() const { return interpreter_.vm(); }
  Routines& routines() { return routines_; }
  MemoryFiles& files() { return files_; }
  BufferLogger& logger() { return logger_; }
  CountingBuzzer& buzzerPart() { return buzzerPart_; }

  // One beat: 250 ms of the device's loop, four control ticks and one page poll.
  void beat(int count);

  // The bytes the interpreter holds, read after a collection: everything it has allocated, so what
  // the image and the page under test keep rather than the garbage they have not dropped yet.
  size_t heapBytes();
  // The largest figure sampled at any beat of the run so far, which is the run's high-water mark.
  size_t heapPeak() const { return heapPeakBytes_; }

  // --- what the boot cost, sampled between the files the loader reads -------------------------
  // The heap before the image loads, after `/system` and once the root scene is built.
  size_t vmBytes() const { return vmBytes_; }
  size_t frameworkBytes() const { return frameworkBytes_; }
  size_t imageBytes() const { return imageBytes_; }
  // What each image file added, in the order they loaded, and then what the root scene added.
  std::vector<std::pair<std::string, size_t>> fileCosts() const;
  size_t sceneBytes() const { return imageBytes_ >= loadedBytes_ ? imageBytes_ - loadedBytes_ : 0; }
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
  // One read the loader makes: the heap before it, when the path is one of the image's own files.
  void sampleRead(const String& path);
  std::string dataDir_;
  std::string capturePath_;
  std::string captureStem_ = "build/runs";
  std::vector<std::vector<uint8_t>> captureFrames_;
  bool capturing_ = false;
  std::string gesture_;
  int gestureBeats_ = 0;
  size_t heapPeakBytes_ = 0;
  // The boot, as the heap it held before each image file was read, plus where it ended.
  std::vector<std::pair<std::string, size_t>> readSamples_;
  size_t vmBytes_ = 0;
  size_t frameworkBytes_ = 0;
  size_t loadedBytes_ = 0;
  size_t imageBytes_ = 0;
  int16_t overlayTop_ = -1;
  int16_t overlayRows_ = 0;

  MemoryFiles files_;
  BufferLogger logger_;
  Config config_;
  Chamber chamber_;
  ModelPump pump_;
  ModelSensor sensor_;
  ModelValve valve_;
  VacuumSystem vacuum_;
  BitmapPanel panel_;
  gfx::Canvas canvas_;
  FixedBattery battery_;
  CountingBuzzer buzzerPart_;
  BuzzerPlayer buzzer_;
  BerryInterpreter interpreter_;
  Routines routines_{interpreter_};
  HarnessPanel context_;
};

void installHarnessModule(bvm* vm, HarnessSession& appSession);

}  // namespace gardener
