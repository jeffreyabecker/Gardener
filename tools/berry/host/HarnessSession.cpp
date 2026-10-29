#include "HarnessSession.h"

#include <cstring>
#include <deque>

#include "BerryHost.h"
#include "Clock.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"

// The interpreter's heap count, which the report reads: a C API, so the header goes in as one.
extern "C" {
#include "be_gc.h"
}

namespace gardener {

namespace {

// The control task's queue, as a list. Everything that writes the machine posts here and the beat
// drains it before its own tick, so a write is seen on the next beat as it is on the device rather
// than the instant it is made. A deque has no depth, so a post is always taken.
std::deque<MachineCommand> pendingCommands;

// The interpreter's heap, read after a collection: the figures the check reports are all this.
size_t heapOf(bvm* vm) {
  if (vm == nullptr) return 0;
  be_gc_collect(vm);
  return be_gc_memcount(vm);
}

}  // namespace

bool postMachineCommand(const MachineCommand& command) {
  pendingCommands.push_back(command);
  return true;
}
}  // namespace gardener

namespace gardener {

namespace {

constexpr float PullKpaPerSecond = 3.0f;   // at full duty
constexpr float VentKpaPerSecond = 40.0f;
constexpr float LeakKpaPerSecond = 0.2f;
constexpr uint32_t BeatMs = 250;
constexpr uint32_t TickMs = 50;

}  // namespace

void Chamber::advance(uint32_t ms) {
  const float seconds = (float)ms / 1000.0f;
  if (venting_) {
    pressureKpa_ -= VentKpaPerSecond * seconds;
  } else {
    pressureKpa_ += (duty_ / 100.0f) * PullKpaPerSecond * seconds;
    pressureKpa_ -= LeakKpaPerSecond * seconds;
  }
  if (pressureKpa_ < 0.0f) pressureKpa_ = 0.0f;
  if (pressureKpa_ > ceilingKpa_) pressureKpa_ = ceilingKpa_;
}

void Chamber::reset() {
  pressureKpa_ = 0.0f;
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

HarnessSession::HarnessSession(std::string dataDir, float ceilingKpa)
    : dataDir_(std::move(dataDir)),
      config_(&files_),
      chamber_(ceilingKpa),
      pump_(chamber_),
      sensor_(chamber_),
      valve_(chamber_),
      vacuum_(&pump_, &sensor_, &valve_),
      canvas_(display_.rootCanvas()),
      buzzer_(buzzerPart_),
      context_(display_, canvas_, config_, vacuum_, battery_, buzzer_, files_, logger_) {}

HarnessSession::~HarnessSession() = default;

bool HarnessSession::begin() {
  files_.setReadObserver([this](const String& path) { sampleRead(path); });
  if (!files_.load(dataDir_)) return false;

  // The order Panel::boot brings them up in, minus the bus and the keys. The document is read before
  // the interpreter, which reads it when its modules are installed.
  config_.begin();
  battery_.begin();
  buzzerPart_.begin();
  buzzer_.begin();
  // Machine::begin's order: the parts first, then the control law over them.
  sensor_.begin();
  pump_.begin();
  valve_.begin();
  vacuum_.begin();

  if (!routines_.begin(context_)) return false;
  // The image's own files, before the panel's root scene is built: what the loader itself cost.
  loadedBytes_ = heapOf(interpreter_.vm());
  if (frameworkBytes_ == 0) frameworkBytes_ = loadedBytes_;
  // The panel task's own UI runs the page's activate() when it comes up; the check does the same.
  routines_.enterPage();
  imageBytes_ = heapBytes();
  return true;
}

void HarnessSession::beat(int count) {
  for (int beat = 0; beat < count; beat++) {
    for (uint32_t tick = 0; tick < BeatMs / TickMs; tick++) {
      gardener::host::advance(TickMs);
      const uint32_t now = millis();

      chamber_.advance(TickMs);

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

    // panelTask's own beat: the gauge, the buzzer, and one entry into the page. The page's poll is
    // the beat's one send to the glass, so a capture takes one frame from it rather than a second
    // flush here.
    battery_.update(millis());
    buzzer_.update(millis());
    routines_.poll(millis());

    // The run's high-water mark, sampled where the page has finished its own work for the beat.
    const size_t held = heapBytes();
    if (held > heapPeakBytes_) heapPeakBytes_ = held;
  }
}

size_t HarnessSession::heapBytes() { return heapOf(interpreter_.vm()); }

void HarnessSession::sampleRead(const String& path) {
  const char* file = path.c_str();
  if (file == nullptr) return;
  // The image's own files only: a `/cfg` document read while it loads is not a file it compiles.
  const bool system = strncmp(file, "/system/", 8) == 0;
  if (!system && strncmp(file, "/routines/", 10) != 0) return;

  const size_t held = heapOf(interpreter_.vm());
  if (readSamples_.empty()) vmBytes_ = held;
  // The first routine read is where the framework is finished and the pages have yet to start.
  if (frameworkBytes_ == 0 && !system) frameworkBytes_ = held;
  readSamples_.emplace_back(file, held);
}

std::vector<std::pair<std::string, size_t>> HarnessSession::fileCosts() const {
  std::vector<std::pair<std::string, size_t>> costs;
  for (size_t i = 0; i < readSamples_.size(); i++) {
    const size_t before = readSamples_[i].second;
    const size_t after = i + 1 < readSamples_.size() ? readSamples_[i + 1].second : loadedBytes_;
    costs.emplace_back(readSamples_[i].first, after > before ? after - before : 0);
  }
  return costs;
}

void HarnessSession::pressGesture(const char* gesture) { gestureStrip_.show(gesture); }

int HarnessSession::errorLines() const { return logger_.errors(); }

void HarnessSession::markOverlay(int16_t top, int16_t rows) { display_.markOverlay(top, rows); }

void HarnessSession::backHold() {
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
  // The picture a harness capture writes: the glass, with the gesture strip's rows under it, so the
  // press a frame carried reads below it rather than over it.
  const int16_t glassWidth = CaptureDisplay::Width;
  const int16_t glassHeight = CaptureDisplay::Height;
  const int16_t stripTop = glassHeight;
  display_.beginCapture(
      CaptureDisplay::Frame{glassWidth, static_cast<int16_t>(stripTop + GestureStrip::Rows),
                            gfx::Rect(0, 0, glassWidth, glassHeight)},
      {gestureStrip_.component(gfx::Rect(0, stripTop, glassWidth, GestureStrip::Rows))});
}

bool HarnessSession::endCapture() {
  // Nothing to write is not a write failure: a case that never beats has no motion to show.
  const bool ok = display_.frameCount() == 0 || display_.save(capturePath_.c_str());
  display_.endCapture();
  capturePath_.clear();
  return ok;
}

}  // namespace gardener
