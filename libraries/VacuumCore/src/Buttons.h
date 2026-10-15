#pragma once

#include <Arduino.h>

#include "VacuumConfig.h"

namespace eggplan {

enum class ButtonId : uint8_t { Up = 0, Down = 1, Ok = 2, Back = 3, Count = 4 };
enum class ButtonEvent : uint8_t { None = 0, Click, LongPress, Repeat };

// Four active low buttons with debounce, long press and auto repeat.
//
// Events are queued so that a press that happens while the firmware is busy is
// not lost, and long/blocking waits keep the rest of the application alive via
// an optional service hook.
class Buttons {
 public:
  void begin();
  void update(uint32_t nowMs);

  bool isDown(ButtonId id) const;
  uint32_t heldMs(ButtonId id) const;
  ButtonId lastChanged() const { return lastChanged_; }

  bool popEvent(ButtonId& id, ButtonEvent& event, uint32_t& heldMs);
  bool waitForEvent(ButtonId id, ButtonEvent& event, uint32_t timeoutMs);

  // Called from waitForEvent() so sensors, UI and BLE keep running.
  void setServiceHook(void (*hook)()) { serviceHook_ = hook; }

  static const char* idName(ButtonId id);
  static const char* eventName(ButtonEvent event);

 private:
  static constexpr uint8_t kButtonCount = (uint8_t)ButtonId::Count;
  static constexpr uint8_t kQueueSize = 16;

  struct State {
    int pin = -1;
    bool stable = false;
    bool raw = false;
    uint32_t rawChangedMs = 0;
    uint32_t pressedMs = 0;
    bool longFired = false;
    uint32_t nextRepeatMs = 0;
  };

  struct QueuedEvent {
    ButtonId id;
    ButtonEvent event;
    uint32_t heldMs;
  };

  void push(ButtonId id, ButtonEvent event, uint32_t heldMs);

  State states_[kButtonCount];
  QueuedEvent queue_[kQueueSize];
  uint8_t queueHead_ = 0;
  uint8_t queueTail_ = 0;
  uint32_t nowMs_ = 0;
  ButtonId lastChanged_ = ButtonId::Up;
  void (*serviceHook_)() = nullptr;
};

}  // namespace eggplan
