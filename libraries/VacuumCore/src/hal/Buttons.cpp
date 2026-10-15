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
  const int pins[kButtonCount] = {cfg::PIN_BTN_UP, cfg::PIN_BTN_DOWN, cfg::PIN_BTN_OK,
                                  cfg::PIN_BTN_BACK};
  for (uint8_t i = 0; i < kButtonCount; i++) {
    states_[i].pin = pins[i];
    pinMode(pins[i], INPUT_PULLUP);
    bool down = digitalRead(pins[i]) == LOW;
    states_[i].stable = down;
    states_[i].raw = down;
    states_[i].rawChangedMs = millis();
    if (down) {
      states_[i].pressedMs = millis();
    }
  }
  LOGI("buttons: up=%d down=%d ok=%d back=%d", cfg::PIN_BTN_UP, cfg::PIN_BTN_DOWN,
       cfg::PIN_BTN_OK, cfg::PIN_BTN_BACK);
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
  const QueuedEvent& e = queue_[queueHead_];
  id = e.id;
  event = e.event;
  heldMs = e.heldMs;
  queueHead_ = (uint8_t)((queueHead_ + 1) % kQueueSize);
  return true;
}

void Buttons::update(uint32_t nowMs) {
  nowMs_ = nowMs;
  for (uint8_t i = 0; i < kButtonCount; i++) {
    State& s = states_[i];
    ButtonId id = (ButtonId)i;
    bool down = digitalRead(s.pin) == LOW;

    if (down != s.raw) {
      s.raw = down;
      s.rawChangedMs = nowMs;
    }

    if (s.raw != s.stable && (nowMs - s.rawChangedMs) >= cfg::BUTTON_DEBOUNCE_MS) {
      s.stable = s.raw;
      lastChanged_ = id;
      if (s.stable) {
        s.pressedMs = nowMs;
        s.longFired = false;
        s.nextRepeatMs = nowMs + cfg::BUTTON_LONGPRESS_MS;
      } else {
        uint32_t held = nowMs - s.pressedMs;
        if (!s.longFired) {
          push(id, ButtonEvent::Click, held);
        }
      }
    }

    if (s.stable) {
      uint32_t held = nowMs - s.pressedMs;
      if (!s.longFired && held >= cfg::BUTTON_LONGPRESS_MS) {
        s.longFired = true;
        s.nextRepeatMs = nowMs + cfg::BUTTON_REPEAT_MS;
        push(id, ButtonEvent::LongPress, held);
      } else if (s.longFired && nowMs >= s.nextRepeatMs) {
        s.nextRepeatMs = nowMs + cfg::BUTTON_REPEAT_MS;
        push(id, ButtonEvent::Repeat, held);
      }
    }
  }
}

bool Buttons::isDown(ButtonId id) const {
  uint8_t i = (uint8_t)id;
  if (i >= kButtonCount) return false;
  return states_[i].stable;
}

uint32_t Buttons::heldMs(ButtonId id) const {
  uint8_t i = (uint8_t)id;
  if (i >= kButtonCount || !states_[i].stable) return 0;
  return nowMs_ - states_[i].pressedMs;
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
