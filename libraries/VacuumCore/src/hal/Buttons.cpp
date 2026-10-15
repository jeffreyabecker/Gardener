#include "hal/Buttons.h"

#include "core/Log.h"

namespace eggplan {

const char* Buttons::idName(ButtonId id) {
  switch (id) {
    case ButtonId::Up: return "up";
    case ButtonId::Down: return "down";
    case ButtonId::Ok: return "ok";
    case ButtonId::Back: return "back";
    default: return "?";
  }
}

const char* Buttons::eventName(ButtonEvent event) {
  switch (event) {
    case ButtonEvent::Click: return "click";
    case ButtonEvent::LongPress: return "long";
    case ButtonEvent::Repeat: return "repeat";
    default: return "none";
  }
}

void Buttons::begin() {
  const int pins[kButtonCount] = {config_.up, config_.down, config_.ok, config_.back};
  for (uint8_t i = 0; i < kButtonCount; i++) {
    states_[i].pin = pins[i];
    pinMode(pins[i], INPUT_PULLUP);
    bool down = digitalRead(pins[i]) == LOW;
    states_[i].stable = down;
    states_[i].raw = down;
    states_[i].rawChangedMs = millis();
    if (down) states_[i].pressedMs = millis();
  }
  LOGI("buttons: up=%d down=%d ok=%d back=%d", config_.up, config_.down, config_.ok,
       config_.back);
}

void Buttons::push(ButtonId id, ButtonEvent event, uint32_t heldMs) {
  uint8_t next = (uint8_t)((queueTail_ + 1) % kQueueSize);
  if (next == queueHead_) {
    // Queue full: drop the oldest entry, the newest press matters more.
    queueHead_ = (uint8_t)((queueHead_ + 1) % kQueueSize);
  }
  queue_[queueTail_] = {id, event, heldMs};
  queueTail_ = next;
}

bool Buttons::popEvent(ButtonId& id, ButtonEvent& event, uint32_t& heldMs) {
  if (queueHead_ == queueTail_) return false;
  const QueuedEvent& queued = queue_[queueHead_];
  id = queued.id;
  event = queued.event;
  heldMs = queued.heldMs;
  queueHead_ = (uint8_t)((queueHead_ + 1) % kQueueSize);
  return true;
}

void Buttons::update(uint32_t nowMs) {
  nowMs_ = nowMs;
  for (uint8_t i = 0; i < kButtonCount; i++) {
    State& state = states_[i];
    ButtonId id = (ButtonId)i;
    bool down = digitalRead(state.pin) == LOW;

    if (down != state.raw) {
      state.raw = down;
      state.rawChangedMs = nowMs;
    }

    if (state.raw != state.stable && (nowMs - state.rawChangedMs) >= config_.debounceMs) {
      state.stable = state.raw;
      lastChanged_ = id;
      if (state.stable) {
        state.pressedMs = nowMs;
        state.longFired = false;
        state.nextRepeatMs = nowMs + config_.longPressMs;
      } else if (!state.longFired) {
        push(id, ButtonEvent::Click, nowMs - state.pressedMs);
      }
    }

    if (state.stable) {
      uint32_t held = nowMs - state.pressedMs;
      if (!state.longFired && held >= config_.longPressMs) {
        state.longFired = true;
        state.nextRepeatMs = nowMs + config_.repeatMs;
        push(id, ButtonEvent::LongPress, held);
      } else if (state.longFired && nowMs >= state.nextRepeatMs) {
        state.nextRepeatMs = nowMs + config_.repeatMs;
        push(id, ButtonEvent::Repeat, held);
      }
    }
  }
}

bool Buttons::isDown(ButtonId id) const {
  uint8_t index = (uint8_t)id;
  if (index >= kButtonCount) return false;
  return states_[index].stable;
}

uint32_t Buttons::heldMs(ButtonId id) const {
  uint8_t index = (uint8_t)id;
  if (index >= kButtonCount || !states_[index].stable) return 0;
  return nowMs_ - states_[index].pressedMs;
}

bool Buttons::waitForEvent(ButtonId id, ButtonEvent& event, uint32_t timeoutMs) {
  uint32_t start = millis();
  for (;;) {
    update(millis());
    if (serviceHook_) serviceHook_();

    ButtonId gotId = ButtonId::Up;
    ButtonEvent gotEvent = ButtonEvent::None;
    uint32_t held = 0;
    // Drain the queue so unrelated presses do not pile up during the wait.
    while (popEvent(gotId, gotEvent, held)) {
      if (gotId == id) {
        event = gotEvent;
        return true;
      }
    }
    if (timeoutMs != 0 && (millis() - start) >= timeoutMs) return false;
    delay(2);
  }
}

}  // namespace eggplan
