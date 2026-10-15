#include "app/App.h"

#include <Wire.h>

#include "core/Log.h"
#include "core/Util.h"

namespace eggplan {

App* App::instance_ = nullptr;

bool App::begin() {
  logBegin(115200);
  delay(200);
  LOGI("eggplan-vacuum %s booting", cfg::FIRMWARE_VERSION);
  instance_ = this;

  Wire.begin(cfg::PIN_I2C_SDA, cfg::PIN_I2C_SCL, cfg::I2C_FREQ_HZ);

  bool displayOk = ui_.begin();
  bool storeOk = store_.begin();
  bool sensorOk = sensor_.begin(Wire);
  pump_.begin();
  buttons_.begin();

  ctx_.pump = &pump_;
  ctx_.sensor = &sensor_;
  ctx_.buttons = &buttons_;
  ctx_.ui = &ui_;
  ctx_.store = &store_;
  ctx_.control = &control_;
  ctx_.runtime = &nullRuntime_;
  ctx_.api = &api_;
  ctx_.ble = &ble_;

  control_.begin(ctx_);
  api_.setRuntime(ctx_.runtime);
  api_.setServiceHook(&App::tick);
  buttons_.setServiceHook(&App::tick);
  ctx_.runtime->setServiceHook(&App::tick);
  ctx_.runtime->begin(ctx_, api_);
  ble_.begin(ctx_);

  if (!displayOk) LOGE("app: display unavailable");
  if (!storeOk) LOGE("app: program store unavailable");
  if (!sensorOk) LOGE("app: pressure sensor unavailable");

  api_.captureAmbient();

  String autostart = store_.autostart();
  if (autostart.length()) {
    if (!ctx_.runtime->available()) {
      LOGW("app: autostart %s skipped, no script runtime", autostart.c_str());
    } else {
      String source;
      String error;
      if (store_.read(autostart, source) &&
          ctx_.runtime->start(autostart.c_str(), source, error)) {
        LOGI("app: autostarted %s", autostart.c_str());
      } else {
        LOGE("app: autostart failed: %s", error.c_str());
      }
    }
  }

  if (displayOk) {
    if (!sensorOk) {
      ui_.toast("no sensor", 3000);
    } else if (!storeOk) {
      ui_.toast("fs error", 3000);
    } else {
      ui_.toast("ready", 1200);
    }
  }

  ready_ = true;
  LOGI("app: ready, heap %u bytes", (unsigned)ESP.getFreeHeap());
  return true;
}

void App::tick() {
  if (!instance_ || !instance_->ready_) return;
  App* self = instance_;
  uint32_t now = millis();
  self->sensor_.update(now);
  self->pump_.update(now);
  self->control_.update(now);
  self->buttons_.update(now);
  self->ui_.setModel(self->buildModel());
  self->ui_.update(now);
  self->ble_.update(now);
}

DisplayUI::Model App::buildModel() {
  DisplayUI::Model model;
  model.pressureKpa = sensor_.kpa();
  model.vacuumKpa = sensor_.vacuumKpa();
  model.vacuumPercent = sensor_.vacuumPercent();
  model.ambientKpa = sensor_.ambientKpa();
  model.duty = pump_.dutyPercent();
  model.targetKpa = control_.targetKpa();
  model.targetActive = control_.targetActive();
  model.state = pump_.tripped() ? "FAULT" : control_.stateText();
  autostartCache_ = store_.autostart();
  model.autostart = autostartCache_.c_str();
  model.bleConnected = ble_.connected();
  model.sensorOk = sensor_.present();
  model.scriptRunning = ctx_.runtime->running();
  model.program = ctx_.runtime->running() ? ctx_.runtime->currentProgram() : "";
  model.uptimeMs = millis();
  model.programs = store_.list();
  model.programCount = store_.count();
  return model;
}

void App::stopEverything(const char* reason) {
  if (ctx_.runtime->running()) ctx_.runtime->stop(reason);
  control_.stop();
  pump_.stop();
}

void App::startSelectedProgram() {
  const ProgramInfo* info = store_.at(ui_.cursor());
  if (!info) {
    ui_.toast("no program", 1200);
    return;
  }
  if (!ctx_.runtime->available()) {
    ui_.toast("no script vm", 1500);
    return;
  }
  String source;
  if (!store_.read(info->name, source)) {
    ui_.toast("read failed", 1500);
    return;
  }
  if (ctx_.runtime->running()) ctx_.runtime->stop("replaced");
  api_.setProgramArgs("");
  String error;
  if (!ctx_.runtime->start(info->name.c_str(), source, error)) {
    ui_.toast(error.c_str(), 2000);
    return;
  }
  String message = String("run ") + info->name;
  ui_.toast(message.c_str(), 1200);
}

void App::onButtonEvent(ButtonId id, ButtonEvent event, uint32_t heldMs) {
  (void)heldMs;

  if (id == ButtonId::Ok && event == ButtonEvent::LongPress) {
    ui_.nextPage(1);
    ui_.toast(ui_.pageName(), 800);
    return;
  }
  if (id == ButtonId::Back) {
    stopEverything("user");
    ui_.toast("stopped", 900);
    return;
  }

  switch (ui_.page()) {
    case DisplayUI::Page::Programs:
      if (id == ButtonId::Up || id == ButtonId::Down) {
        ui_.moveCursor(id == ButtonId::Up ? -1 : 1, store_.count());
      } else if (id == ButtonId::Ok && event == ButtonEvent::Click) {
        startSelectedProgram();
      }
      break;

    case DisplayUI::Page::Setpoint:
      if (id == ButtonId::Up || id == ButtonId::Down) {
        float target = control_.targetKpa();
        if (target <= 0.0f && ctx_.sensor) target = ctx_.sensor->kpa();
        target += (id == ButtonId::Up ? -1.0f : 1.0f);
        target = clampf(target, cfg::CONTROL_MIN_TARGET_KPA, 150.0f);
        control_.setTarget(target);
        String message = String("target ") + formatFloat(target, 0);
        ui_.toast(message.c_str(), 900);
      } else if (id == ButtonId::Ok && event == ButtonEvent::Click) {
        if (control_.targetActive()) {
          control_.disableTarget();
          ui_.toast("hold off", 900);
        } else if (ctx_.sensor) {
          control_.setTarget(clampf(ctx_.sensor->kpa() - 10.0f,
                                    cfg::CONTROL_MIN_TARGET_KPA, 150.0f));
          ui_.toast("hold on", 900);
        }
      }
      break;

    case DisplayUI::Page::Status:
      if (id == ButtonId::Ok && event == ButtonEvent::Click) {
        if (control_.targetActive()) {
          control_.disableTarget();
          ui_.toast("hold off", 900);
        } else if (ctx_.sensor && ctx_.sensor->readingValid()) {
          control_.setTarget(clampf(ctx_.sensor->kpa() - 10.0f,
                                    cfg::CONTROL_MIN_TARGET_KPA, 150.0f));
          ui_.toast("hold on", 900);
        }
      } else if (id == ButtonId::Up || id == ButtonId::Down) {
        ui_.nextPage(id == ButtonId::Up ? -1 : 1);
        ui_.toast(ui_.pageName(), 600);
      }
      break;

    default:
      if (id == ButtonId::Up || id == ButtonId::Down || id == ButtonId::Ok) {
        ui_.nextPage(1);
        ui_.toast(ui_.pageName(), 600);
      }
      break;
  }
}

void App::update() {
  uint32_t now = millis();

  sensor_.update(now);
  pump_.update(now);
  control_.update(now);
  buttons_.update(now);

  ButtonId id = ButtonId::Up;
  ButtonEvent event = ButtonEvent::None;
  uint32_t held = 0;
  while (buttons_.popEvent(id, event, held)) {
    onButtonEvent(id, event, held);
  }

  ctx_.runtime->update();

  if (now - lastGraphSampleMs_ >= 100) {
    lastGraphSampleMs_ = now;
    ui_.pushPressure(sensor_.kpa());
  }

  ui_.setModel(buildModel());
  ui_.update(now);
  ble_.update(now);
}

}  // namespace eggplan
