#include "HarnessSession.h"

#include <freertos/queue.h>

#include "Machine.h"
#include "MachineCommand.h"
#include "Tasks.h"
#include "routines/Bindings.h"

namespace gardener {

// The panel task's two counters. Only the stop notice is fired here; the go-home sequence belongs
// to the keys the drive presses itself.
volatile uint32_t stopSeq = 0;
volatile uint32_t goHomeSeq = 0;

}  // namespace gardener

namespace gardener {

namespace {

constexpr float kPullKpaPerSecond = 3.0f;   // at full duty
constexpr float kVentKpaPerSecond = 40.0f;
constexpr float kLeakKpaPerSecond = 0.2f;
constexpr float kChamberCeilingKpa = 70.0f;
constexpr uint32_t kBeatMs = 250;
constexpr uint32_t kTickMs = 50;

// The command queue's depth, as Tasks.cpp gives it.
constexpr UBaseType_t kCommandDepth = 8;

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
      settings_(&fs_),
      pump_(chamber_),
      sensor_(chamber_),
      valve_(chamber_),
      vacuum_(&pump_, &sensor_, &valve_),
      display_(
          [](BitmapPanel& panel) -> bool { return panel.begin(); },
          BitmapPanel::kWidth, BitmapPanel::kHeight),
      buzzer_(buzzerPart_),
      overlay_(&display_, &battery_, &vacuum_, &settings_),
      context_(overlay_, settings_, vacuum_, battery_, buzzer_, fs_, logger_) {
  machineCommandQueue = xQueueCreate(kCommandDepth, sizeof(MachineCommand));
}

HarnessSession::~HarnessSession() = default;

bool HarnessSession::begin() {
  if (!fs_.load(dataDir_)) return false;

  // The order Panel::boot brings them up in, minus the bus and the keys.
  settings_.begin();
  battery_.begin();
  buzzerPart_.begin();
  buzzer_.begin();
  overlay_.begin();
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
      MachineCommand command;
      while (xQueueReceive(machineCommandQueue, &command, 0) == pdTRUE) {
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
  }
}

void HarnessSession::backHold() {
  stopSeq++;
  const MachineCommand stop{MachineCommand::Op::Stop};
  xQueueSend(machineCommandQueue, &stop, 0);
}

void HarnessSession::reset() {
  // Leaving the page under test runs default's activate(), which is where the device would be.
  routines_.showDefault();

  const MachineCommand stop{MachineCommand::Op::Stop};
  xQueueSend(machineCommandQueue, &stop, 0);
  chamber_.reset();
  vacuum_.service(millis());
}

}  // namespace gardener
