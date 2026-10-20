#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app/storage/Ini.h"
#include "app/storage/Settings.h"
#include "app/storage/Storage.h"

namespace gardener {

// The store queue depth, from docs/plans/free-rtos/queues.md. One producer (the panel task) and one
// consumer (the interface task), both draining every pass.
constexpr UBaseType_t StoreQueueDepth = 4;

struct StoreResult {
  bool ok = false;
  String value;
};

// The one request the VM task sends the interface task for flash-backed reads and writes. Get and
// Set carry the routine's registered name, already resolved by the caller.
struct StoreRequest {
  enum class Op : uint8_t { Get, Set, SetUnit, SetTarget, SetBluetooth };
  Op op;
  String routine;                // Get/Set: the routine's registered name
  String key;                    // Get/Set
  String value;                  // Set
  uint8_t unit = 0;              // SetUnit: DisplayUnit
  float targetKpa = 0.0f;        // SetTarget
  bool bluetooth = false;        // SetBluetooth
  StoreResult* result = nullptr; // Get/Set: the client's slot, filled by the server
  TaskHandle_t waiter = nullptr; // Get/Set: the task to notify when the result is ready
};

// The panel side: the VM's one way to reach flash and the settings. get/set round-trip because
// their Berry value is the file's answer; the three setters are one-way because those calls are
// void. The result slot belongs to the client and is read only after the notification, so nothing
// locks it.
class StoreClient {
 public:
  explicit StoreClient(QueueHandle_t queue) : queue_(queue) {}

  bool get(const char* routine, const char* key, String& out) {
    StoreRequest request;
    request.op = StoreRequest::Op::Get;
    request.routine = routine;
    request.key = key;
    request.result = &result_;
    request.waiter = xTaskGetCurrentTaskHandle();
    if (xQueueSend(queue_, &request, 0) != pdTRUE) return false;  // full: refuse, like "not stored"
    xTaskNotifyWait(0, 0, nullptr, portMAX_DELAY);
    if (!result_.ok) return false;
    out = result_.value;
    return true;
  }

  bool set(const char* routine, const char* key, const String& value) {
    StoreRequest request;
    request.op = StoreRequest::Op::Set;
    request.routine = routine;
    request.key = key;
    request.value = value;
    request.result = &result_;
    request.waiter = xTaskGetCurrentTaskHandle();
    if (xQueueSend(queue_, &request, 0) != pdTRUE) return false;
    xTaskNotifyWait(0, 0, nullptr, portMAX_DELAY);
    return result_.ok;
  }

  void setUnit(DisplayUnit unit) {
    StoreRequest request;
    request.op = StoreRequest::Op::SetUnit;
    request.unit = static_cast<uint8_t>(unit);
    xQueueSend(queue_, &request, 0);
  }

  void setTarget(float kpa) {
    StoreRequest request;
    request.op = StoreRequest::Op::SetTarget;
    request.targetKpa = kpa;
    xQueueSend(queue_, &request, 0);
  }

  void setBluetooth(bool enabled) {
    StoreRequest request;
    request.op = StoreRequest::Op::SetBluetooth;
    request.bluetooth = enabled;
    xQueueSend(queue_, &request, 0);
  }

 private:
  QueueHandle_t queue_;
  StoreResult result_;  // one slot; the VM task is sequential and never has two in flight
};

// The interface side: the only place the VM's flash traffic runs. drain() is called once per
// interface pass.
class StoreServer {
 public:
  StoreServer(QueueHandle_t queue, Settings& settings, Storage& storage)
      : queue_(queue), settings_(settings), storage_(storage) {}

  void drain() {
    StoreRequest request;
    while (xQueueReceive(queue_, &request, 0) == pdTRUE) handle(request);
  }

 private:
  void handle(const StoreRequest& request) {
    bool ok = false;
    String out;
    switch (request.op) {
      case StoreRequest::Op::Get: {
        String path = "/routines/" + request.routine + ".ini";
        String text;
        if (storage_.read(path.c_str(), text)) {
          Ini values;
          values.parse(text);
          if (values.has("", request.key.c_str())) {
            out = values.get("", request.key.c_str(), "");
            ok = true;
          }
        }
        break;
      }
      case StoreRequest::Op::Set: {
        String path = "/routines/" + request.routine + ".ini";
        String text;
        Ini values;
        if (storage_.read(path.c_str(), text)) values.parse(text);
        // A repeat value is not worth a flash write, as before.
        if (values.has("", request.key.c_str()) &&
            values.get("", request.key.c_str(), "") == request.value) {
          ok = true;
        } else if (values.set("", request.key.c_str(), request.value)) {
          ok = storage_.write(path.c_str(), values.text());
        }
        break;
      }
      case StoreRequest::Op::SetUnit:
        settings_.setUnit(static_cast<DisplayUnit>(request.unit));
        return;
      case StoreRequest::Op::SetTarget:
        settings_.setTarget(request.targetKpa);
        return;
      case StoreRequest::Op::SetBluetooth:
        settings_.setBluetooth(request.bluetooth);
        return;
    }
    if (request.result) {
      request.result->ok = ok;
      request.result->value = out;
      xTaskNotifyGive(request.waiter);
    }
  }

  QueueHandle_t queue_;
  Settings& settings_;
  Storage& storage_;
};

}  // namespace gardener
