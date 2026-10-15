#include "app/App.h"

#include <Wire.h>

#include "core/Format.h"
#include "core/Log.h"
#include "core/Math.h"
#include "hal/Pins.h"
#include "services/SnapshotBuilder.h"

namespace eggplan {
namespace {

void serialLogSink(const char* text, size_t length) {
  Serial.write((const uint8_t*)text, length);
}

}  // namespace

App* App::instance_ = nullptr;

App::App()
    : pump_(pumpConfig()),
      display_(Wire, pins::OLED_I2C_ADDR, pins::OLED_WIDTH, pins::OLED_HEIGHT),
      store_(storage_),
      control_(pump_, sensor_, pidConfig()),
      api_(pump_, sensor_, buttons_, control_, overlay_, uiSettings_),
      renderer_(display_),
      ui_(renderer_, buttons_, uiSettings_),
      router_(store_, api_, runtime_, control_, pump_, sensor_, telemetry_, uiSettings_),
      uploader_(store_),
      ble_(router_, uploader_, telemetry_, replies_, inbound_, snapshot_) {}

PumpConfig App::pumpConfig() {
  PumpConfig config;
  config.pwmPin = pins::PUMP_PWM;
  config.tachPin = pins::TACH_ENABLED ? pins::PUMP_TACH : -1;
  config.tachPulsesPerRev = pins::TACH_PULSES_PER_REV;
  config.pwmFreqHz = pins::PUMP_PWM_FREQ_HZ;
  config.pwmResolution = pins::PUMP_PWM_RESOLUTION;
  return config;
}

PidConfig App::pidConfig() {
  PidConfig config;
  config.kp = cfg::PID_KP;
  config.ki = cfg::PID_KI;
  config.kd = cfg::PID_KD;
  config.integralLimit = cfg::PID_INTEGRAL_LIMIT;
  return config;
}

bool App::begin() {
  Serial.begin(115200);
  logBegin(&serialLogSink);
  delay(200);
  LOGI("eggplan-vacuum %s booting", cfg::FIRMWARE_VERSION);
  instance_ = this;

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, pins::I2C_FREQ_HZ);

  bool storageOk = storage_.begin();
  bool displayOk = display_.begin();
  bool sensorOk = sensor_.begin(Wire);
  pump_.begin();
  buttons_.begin();
  if (storageOk) store_.begin();
  control_.begin();
  ui_.begin();
  device_ = readDeviceInfo();

  api_.setRuntime(&runtime_);
  api_.setServiceHook(&App::tick);
  buttons_.setServiceHook(&App::tick);
  runtime_.setServiceHook(&App::tick);
  ble_.begin();

  if (sensorOk) sensor_.captureAmbient();
  refreshSnapshot();

  if (storageOk) {
    const char* autostart = store_.autostartName();
    if (autostart[0] != '\0') {
      int index = store_.indexOf(autostart);
      if (!runtime_.available()) {
        LOGW("app: autostart %s skipped, no script runtime", autostart);
      } else if (index < 0) {
        LOGW("app: autostart %s not found", autostart);
      } else {
        startProgram((size_t)index);
      }
    }
  }

  if (displayOk) {
    if (!sensorOk) {
      ui_.toast("no sensor", 3000);
    } else if (!storageOk) {
      ui_.toast("fs error", 3000);
    } else {
      ui_.toast("ready", 1200);
    }
  }

  ready_ = true;
  LOGI("app: ready, heap %u bytes", (unsigned)ESP.getFreeHeap());
  return true;
}

void App::refreshSnapshot() {
  snapshot_ = buildSnapshot(pump_, sensor_, control_, store_, device_);
  snapshot_.runtime = runtime_.name();
  snapshot_.scriptRunning = runtime_.running();
  snapshot_.program = runtime_.running() ? runtime_.currentProgram() : "";
  snapshot_.overlay = &overlay_;
  snapshot_.bleConnected = ble_.connected();
}

void App::tick() {
  if (!instance_ || !instance_->ready_) return;
  App* self = instance_;
  uint32_t now = millis();
  self->sensor_.update(now);
  self->pump_.update(now);
  self->control_.update(now);
  self->buttons_.update(now);
  self->refreshSnapshot();
  self->ui_.update(now, self->snapshot_);
  self->ble_.poll(now);
}

void App::stopEverything(const char* reason) {
  if (runtime_.running()) runtime_.stop(reason);
  control_.stop();
  pump_.stop();
}

void App::startProgram(size_t index) {
  const ProgramInfo* info = store_.at(index);
  if (!info) {
    ui_.toast("no program", 1200);
    return;
  }
  if (!runtime_.available()) {
    ui_.toast("no script vm", 1500);
    return;
  }
  String source;
  if (!store_.read(info->name, source)) {
    ui_.toast("read failed", 1500);
    return;
  }
  if (runtime_.running()) runtime_.stop("replaced");
  api_.setProgramArgs("");
  String error;
  if (!runtime_.start(info->name, source, error)) {
    ui_.toast(error.c_str(), 2000);
    return;
  }
  overlay_.clear();
  String message = String("run ") + info->name;
  ui_.toast(message.c_str(), 1200);
}

void App::onIntent(const Intent& intent) {
  switch (intent.kind) {
    case UiIntent::Stop:
      stopEverything("user");
      ui_.toast("stopped", 900);
      break;

    case UiIntent::CyclePage:
      ui_.cyclePage(intent.delta);
      ui_.toast(ui_.pageName(), 600);
      break;

    case UiIntent::MoveCursor:
      ui_.moveCursor(intent.delta, store_.count());
      break;

    case UiIntent::AdjustTarget: {
      float target = control_.targetActive() ? control_.targetKpa() : sensor_.kpa();
      target = clampf(target + (float)intent.delta, cfg::CONTROL_MIN_TARGET_KPA, 150.0f);
      control_.setTarget(target);
      String message = String("target ") + formatFloat(target, 1);
      ui_.toast(message.c_str(), 900);
      break;
    }

    case UiIntent::ToggleHold:
      if (control_.targetActive()) {
        control_.disableTarget();
        ui_.toast("hold off", 900);
      } else if (sensor_.readingValid()) {
        control_.setTarget(
            clampf(sensor_.kpa() - 10.0f, cfg::CONTROL_MIN_TARGET_KPA, 150.0f));
        ui_.toast("hold on", 900);
      }
      break;

    case UiIntent::RunSelected:
      startProgram(ui_.cursor());
      break;

    default:
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
    onIntent(ui_.onButton(id, event, store_.count()));
  }

  runtime_.update();
  refreshSnapshot();
  ui_.update(now, snapshot_);
  ble_.poll(now);
}

}  // namespace eggplan
