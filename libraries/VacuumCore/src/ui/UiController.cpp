#include "ui/UiController.h"

#include <string.h>

namespace eggplan {
namespace {
constexpr uint32_t kSamplePeriodMs = 100;
constexpr int kTargetStepKpa = 1;
}

bool UiController::begin() { return renderer_.begin(); }

const char* UiController::pageName(Page page) {
  switch (page) {
    case Page::Status: return "status";
    case Page::Programs: return "programs";
    case Page::Setpoint: return "setpoint";
    case Page::Graph: return "graph";
    case Page::Info: return "info";
    default: return "?";
  }
}

const char* UiController::pageName() const { return pageName(view_.page); }

void UiController::cyclePage(int delta) {
  int count = (int)Page::Count;
  int index = (int)view_.page + delta;
  while (index < 0) index += count;
  view_.page = (Page)(index % count);
}

void UiController::moveCursor(int delta, size_t count) {
  if (count == 0) {
    view_.cursor = 0;
    return;
  }
  int value = (int)view_.cursor + delta;
  while (value < 0) value += (int)count;
  view_.cursor = (size_t)(value % (int)count);
}

void UiController::toast(const char* text, uint32_t durationMs) {
  if (!text) return;
  strncpy(settings_.toast, text, sizeof(settings_.toast) - 1);
  settings_.toast[sizeof(settings_.toast) - 1] = '\0';
  settings_.toastUntilMs = millis() + durationMs;
}

void UiController::update(uint32_t nowMs, const SystemSnapshot& snapshot) {
  // A host can ask the local UI to move on without knowing what a page is.
  if (settings_.pageStep != 0) {
    cyclePage(settings_.pageStep);
    settings_.pageStep = 0;
  }

  if (nowMs - lastSampleMs_ >= kSamplePeriodMs) {
    lastSampleMs_ = nowMs;
    renderer_.pushSample(snapshot.kpa);
  }

  renderer_.render(view_, snapshot, settings_);
}

Intent UiController::onButton(ButtonId id, ButtonEvent event, size_t programCount) {
  Intent intent;

  if (id == ButtonId::Ok && event == ButtonEvent::LongPress) {
    intent.kind = UiIntent::CyclePage;
    intent.delta = 1;
    return intent;
  }
  if (id == ButtonId::Back) {
    intent.kind = UiIntent::Stop;
    return intent;
  }

  switch (view_.page) {
    case Page::Programs:
      if (id == ButtonId::Up || id == ButtonId::Down) {
        intent.kind = UiIntent::MoveCursor;
        intent.delta = id == ButtonId::Up ? -1 : 1;
      } else if (id == ButtonId::Ok && event == ButtonEvent::Click) {
        intent.kind = UiIntent::RunSelected;
      }
      break;

    case Page::Setpoint:
      if (id == ButtonId::Up || id == ButtonId::Down) {
        intent.kind = UiIntent::AdjustTarget;
        intent.delta = id == ButtonId::Up ? -kTargetStepKpa : kTargetStepKpa;
      } else if (id == ButtonId::Ok && event == ButtonEvent::Click) {
        intent.kind = UiIntent::ToggleHold;
      }
      break;

    case Page::Status:
      if (id == ButtonId::Ok && event == ButtonEvent::Click) {
        intent.kind = UiIntent::ToggleHold;
      } else if (id == ButtonId::Up || id == ButtonId::Down) {
        intent.kind = UiIntent::CyclePage;
        intent.delta = id == ButtonId::Up ? -1 : 1;
      }
      break;

    default:
      if (id == ButtonId::Up || id == ButtonId::Down || id == ButtonId::Ok) {
        intent.kind = UiIntent::CyclePage;
        intent.delta = 1;
      }
      break;
  }

  (void)programCount;
  return intent;
}

}  // namespace eggplan
