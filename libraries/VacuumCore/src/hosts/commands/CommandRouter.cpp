#include "hosts/commands/CommandRouter.h"

#include <string.h>

#include "core/Config.h"
#include "core/Text.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "services/SnapshotText.h"
#include "services/VacuumControl.h"

namespace eggplan {
namespace {

constexpr size_t kMaxArgs = 3;  // verb [key] [value]; the grammar has nothing else

// Splits in place: every token is NUL terminated, so the caller's buffer is the token
// storage and nothing is allocated.
size_t splitLine(char* line, char* argv[], size_t maxArgs) {
  size_t argc = 0;
  char* cursor = line;
  while (*cursor != '\0' && argc < maxArgs) {
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == '\0') break;
    argv[argc++] = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') cursor++;
    if (*cursor != '\0') *cursor++ = '\0';
  }
  return argc;
}

void lower(char* text) {
  for (; *text != '\0'; text++) {
    if (*text >= 'A' && *text <= 'Z') *text = (char)(*text - 'A' + 'a');
  }
}

bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

// An argument that does not parse in full is a bad_argument, never a silent zero.
bool parseFloat(const char* text, float& out) {
  if (!text || *text == '\0') return false;
  const char* cursor = text;
  bool negative = false;
  if (*cursor == '+' || *cursor == '-') {
    negative = (*cursor == '-');
    cursor++;
  }

  float value = 0.0f;
  bool any = false;
  while (*cursor >= '0' && *cursor <= '9') {
    value = value * 10.0f + (float)(*cursor - '0');
    cursor++;
    any = true;
  }
  if (*cursor == '.') {
    cursor++;
    float scale = 0.1f;
    while (*cursor >= '0' && *cursor <= '9') {
      value += (float)(*cursor - '0') * scale;
      scale *= 0.1f;
      cursor++;
      any = true;
    }
  }
  if (!any || *cursor != '\0') return false;
  out = negative ? -value : value;
  return true;
}

bool parseU32(const char* text, uint32_t& out) {
  if (!text || *text < '0' || *text > '9') return false;
  uint32_t value = 0;
  for (const char* cursor = text; *cursor != '\0'; cursor++) {
    if (*cursor < '0' || *cursor > '9') return false;
    if (value > 429496729u) return false;  // would wrap
    value = value * 10u + (uint32_t)(*cursor - '0');
  }
  out = value;
  return true;
}

}  // namespace

CommandRouter::CommandRouter(VacuumControl& control, Pump& pump, PressureSensor& sensor,
                             LinkSettings& settings)
    : control_(control), pump_(pump), sensor_(sensor), settings_(settings) {}

void CommandRouter::reply(Print& out, const char* payload) {
  out.write((const uint8_t*)"ok ", 3);
  out.write((const uint8_t*)payload, strlen(payload));
  out.write((uint8_t)'\n');
}

void CommandRouter::fail(Print& out, const char* code) {
  out.write((const uint8_t*)"err ", 4);
  out.write((const uint8_t*)code, strlen(code));
  out.write((uint8_t)'\n');
}

void CommandRouter::emitHelp(Print& out) {
  // One line, because every command produces exactly one reply and that is what lets a
  // controller match replies to commands by counting. Argument syntax lives in
  // docs/serial-protocol.md rather than on the wire.
  reply(out, "help=info|status|help|start|stop|reset|reboot|ambient|set");
}

void CommandRouter::handleLine(char* line, Print& out, const SystemSnapshot& snapshot) {
  char* argv[kMaxArgs];
  size_t argc = splitLine(line, argv, kMaxArgs);
  if (argc == 0 || argv[0][0] == '#') return;  // blank line or a comment

  lower(argv[0]);
  const char* command = argv[0];

  if (same(command, "help")) {
    emitHelp(out);
    return;
  }
  if (same(command, "info") || same(command, "status")) {
    if (same(command, "info")) {
      snapshottext::infoBody(snapshot, settings_, body_, sizeof(body_));
    } else {
      snapshottext::statusBody(snapshot, body_, sizeof(body_));
    }
    reply(out, body_);
    return;
  }
  if (same(command, "start")) {
    if (!control_.startRun()) {
      fail(out, "not_ready");
      return;
    }
    reply(out, "started");
    return;
  }
  if (same(command, "stop")) {
    control_.stopRun();
    reply(out, "stopped");
    return;
  }
  if (same(command, "set")) {
    if (argc < 3) {
      fail(out, "bad_argument");
      return;
    }
    lower(argv[1]);
    lower(argv[2]);
    const char* key = argv[1];
    const char* value = argv[2];

    if (same(key, "target")) {
      if (same(value, "off")) {
        control_.clearTarget();
        reply(out, "target=off");
        return;
      }
      float kpa = 0.0f;
      if (!parseFloat(value, kpa)) {
        fail(out, "bad_argument");
        return;
      }
      if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
        fail(out, "out_of_range");
        return;
      }
      control_.setTarget(kpa);
      text::Buf payload(body_, sizeof(body_));
      payload << "target=";
      payload.add(kpa, 1);
      reply(out, body_);
      return;
    }
    if (same(key, "timer")) {
      if (same(value, "off")) {
        control_.setDurationSeconds(0);
        reply(out, "timer=off");
        return;
      }
      uint32_t seconds = 0;
      if (!parseU32(value, seconds)) {
        fail(out, "bad_argument");
        return;
      }
      control_.setDurationSeconds(seconds);
      text::Buf payload(body_, sizeof(body_));
      payload << "timer=" << seconds;
      reply(out, body_);
      return;
    }
    if (same(key, "ambient")) {
      float kpa = 0.0f;
      if (!parseFloat(value, kpa)) {
        fail(out, "bad_argument");
        return;
      }
      if (!sensor_.setAmbientKpa(kpa)) {
        fail(out, "out_of_range");
        return;
      }
      text::Buf payload(body_, sizeof(body_));
      payload << "ambient=";
      payload.add(sensor_.ambientKpa(), 2);
      reply(out, body_);
      return;
    }
    if (same(key, "log")) {
      if (!same(value, "on") && !same(value, "off")) {
        fail(out, "bad_argument");
        return;
      }
      settings_.logEnabled = same(value, "on");
      reply(out, settings_.logEnabled ? "log=on" : "log=off");
      return;
    }
    // v2 configured a telemetry period and could drive the pump directly. Both are gone,
    // and a client that still asks gets told that rather than that it typed it wrong.
    if (same(key, "period") || same(key, "duty")) {
      fail(out, "unsupported");
      return;
    }
    fail(out, "unknown_command");
    return;
  }
  if (same(command, "ambient")) {
    if (argc > 1) {
      lower(argv[1]);
      float kpa = 0.0f;
      if (!parseFloat(argv[1], kpa)) {
        fail(out, "bad_argument");
        return;
      }
      if (!sensor_.setAmbientKpa(kpa)) {
        fail(out, "out_of_range");
        return;
      }
    } else {
      sensor_.captureAmbient();
    }
    text::Buf payload(body_, sizeof(body_));
    payload << "ambient=";
    payload.add(sensor_.ambientKpa(), 2);
    reply(out, body_);
    return;
  }
  // v2 turned telemetry on and off by command; a controller polls `status` now.
  if (same(command, "telemetry") || same(command, "tel")) {
    fail(out, "unsupported");
    return;
  }
  if (same(command, "reset")) {
    pump_.resetFault();
    control_.clearFault();
    sensor_.clearErrors();
    reply(out, "reset");
    return;
  }
  if (same(command, "reboot")) {
    reply(out, "rebooting");
    delay(50);
    ESP.restart();
    return;
  }

  fail(out, "unknown_command");
}

}  // namespace eggplan
