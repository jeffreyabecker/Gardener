#include "HarnessSession.h"

#include <deque>

#include "GestureCallout.h"
#include "GifWriter.h"
#include "Machine.h"
#include "MachineCommand.h"
#include "PanelTasks.h"
#include "routines/Bindings.h"

namespace gardener {

// The panel task's two counters, defined here because the panel task is not. Only the stop notice
// is fired here; the go-home sequence belongs to the keys the drive presses itself.
volatile uint32_t stopSeq = 0;
volatile uint32_t goHomeSeq = 0;

namespace {

// The control task's queue, as a list. Everything that writes the machine posts here and the beat
// drains it before its own tick, so a write is seen on the next beat as it is on the device rather
// than the instant it is made.
std::deque<MachineCommand> pendingCommands;

}  // namespace

void postMachineCommand(const MachineCommand& command) { pendingCommands.push_back(command); }

}  // namespace gardener

namespace gardener {

namespace {

constexpr float kPullKpaPerSecond = 3.0f;   // at full duty
constexpr float kVentKpaPerSecond = 40.0f;
constexpr float kLeakKpaPerSecond = 0.2f;
constexpr float kChamberCeilingKpa = 70.0f;
constexpr uint32_t kBeatMs = 250;
constexpr uint32_t kTickMs = 50;

}  // namespace

void Chamber::advance(uint32_t ms) {
  const float seconds = (float)ms / 1000.0f;
  if (venting_) {
    kpa_ -= kVentKpaPerSecond * seconds;
  } else {
    kpa_ += (duty_ / 100.0f) * kPullKpaPerSecond * seconds;
    kpa_ -= kLeakKpaPerSecond * seconds;
  }
  if (kpa_ < 0.0f) kpa_ = 0.0f;
  if (kpa_ > kChamberCeilingKpa) kpa_ = kChamberCeilingKpa;
}

void Chamber::reset() {
  kpa_ = 0.0f;
  duty_ = 0.0f;
  venting_ = true;
}

void ModelPump::update(uint32_t nowMs) {
  const uint32_t elapsed = lastMs_ == 0 ? 0 : nowMs - lastMs_;
  lastMs_ = nowMs;

  // The driver ramps toward the request at the part's own limit; the model does the same so a page
  // sees a duty that moves rather than one that jumps.
  const float step = PUMP_RAMP_PERCENT_PER_SEC * (float)elapsed / 1000.0f;
  if (duty_ < requested_) {
    duty_ = requested_ < duty_ + step ? requested_ : duty_ + step;
  } else if (duty_ > requested_) {
    duty_ = requested_ > duty_ - step ? requested_ : duty_ - step;
  }
  if (duty_ > PUMP_DUTY_MAX_PERCENT) duty_ = PUMP_DUTY_MAX_PERCENT;
  if (duty_ < 0.0f) duty_ = 0.0f;
  chamber_.setDuty(duty_);
}

HarnessSession::HarnessSession(std::string dataDir)
    : dataDir_(std::move(dataDir)),
      settings_(&files_),
      pump_(chamber_),
      sensor_(chamber_),
      valve_(chamber_),
      vacuum_(&pump_, &sensor_, &valve_),
      display_(
          [](BitmapPanel& panel) -> bool { return panel.begin(); },
          BitmapPanel::kWidth, BitmapPanel::kHeight),
      buzzer_(buzzerPart_),
      overlay_(&display_, &battery_, &vacuum_, &settings_),
      context_(overlay_, settings_, vacuum_, battery_, buzzer_, files_, logger_) {}

HarnessSession::~HarnessSession() = default;

bool HarnessSession::begin() {
  if (!files_.load(dataDir_)) return false;

  // The order Panel::boot brings them up in, minus the bus and the keys.
  settings_.begin();
  battery_.begin();
  buzzerPart_.begin();
  buzzer_.begin();
  overlay_.begin();
  // The host models a device with both links up, so the band shows the serial and Bluetooth
  // icons the panel task would set them for.
  overlay_.setConnections(true, true);
  // Machine::begin's order: the parts first, then the control law over them.
  sensor_.begin();
  pump_.begin();
  valve_.begin();
  vacuum_.begin();

  if (!routines_.begin(context_)) return false;
  // The panel task's own UI runs the page's activate() when it comes up; the check does the same.
  routines_.enterPage();
  vm_ = routines_.vm();
  return true;
}

void HarnessSession::beat(int count) {
  for (int beat = 0; beat < count; beat++) {
    for (uint32_t tick = 0; tick < kBeatMs / kTickMs; tick++) {
      host::advance(kTickMs);
      const uint32_t now = millis();

      chamber_.advance(kTickMs);

      // controlTask's three calls, in its order, with its commands drained first.
      while (!pendingCommands.empty()) {
        const MachineCommand command = pendingCommands.front();
        pendingCommands.pop_front();
        applyMachineCommand(vacuum_, command);
      }
      sensor_.update(now);
      pump_.update(now);
      vacuum_.service(now);
    }

    // panelTask's own beat: the gauge, the buzzer, and one entry into the page.
    battery_.update(millis());
    buzzer_.update(millis());
    routines_.poll(millis());

    if (capturing_) {
      BitmapPanel& panel = display_.driver();
      std::vector<uint8_t> frame((size_t)panel.width() * panel.height() / 8);
      panel.packFrame(frame.data());
      if (gestureBeats_ > 0 && !gesture_.empty()) {
        drawGestureStrip(frame.data(), panel.width(), panel.height(), gestureLabel(gesture_));
      }
      captureFrames_.push_back(std::move(frame));
    }
    if (gestureBeats_ > 0) gestureBeats_--;
  }
}

void HarnessSession::pressGesture(const char* gesture) {
  gesture_ = gesture != nullptr ? gesture : "";
  gestureBeats_ = kGestureCalloutBeats;
}

void HarnessSession::backHold() {
  stopSeq++;
  postMachineCommand(MachineCommand{MachineCommand::Op::Stop});
}

void HarnessSession::reset() {
  // Leaving the page under test runs default's activate(), which is where the device would be.
  routines_.showDefault();

  postMachineCommand(MachineCommand{MachineCommand::Op::Stop});
  chamber_.reset();
  vacuum_.service(millis());
}

void HarnessSession::beginCapture(const char* path) {
  capturePath_ = path != nullptr ? path : "";
  captureFrames_.clear();
  capturing_ = true;
}

bool HarnessSession::endCapture() {
  capturing_ = false;
  // Nothing to write is not a write failure: a case that never beats has no motion to show.
  if (captureFrames_.empty()) {
    capturePath_.clear();
    return true;
  }
  BitmapPanel& panel = display_.driver();
  const bool ok = writeGif(capturePath_.c_str(), panel.width(), panel.height(), captureFrames_);
  captureFrames_.clear();
  capturePath_.clear();
  return ok;
}

}  // namespace gardener
