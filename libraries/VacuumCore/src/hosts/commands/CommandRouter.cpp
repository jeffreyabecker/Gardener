#include "hosts/commands/CommandRouter.h"

#include <ctype.h>
#include <string.h>

#include "core/Config.h"
#include "core/Format.h"
#include "core/Log.h"
#include "core/Math.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "services/SnapshotText.h"
#include "services/VacuumControl.h"

namespace eggplan {
namespace {

constexpr size_t kMaxArgs = 8;

void splitArgs(const String& line, String* argv, size_t maxArgs, size_t& argc,
               String& rest) {
  argc = 0;
  rest = "";
  int i = 0;
  int length = (int)line.length();
  while (i < length) {
    while (i < length && isspace((unsigned char)line[i])) i++;
    if (i >= length) break;
    int start = i;
    while (i < length && !isspace((unsigned char)line[i])) i++;
    if (argc < maxArgs) {
      argv[argc++] = line.substring(start, i);
    } else {
      rest = line.substring(start);
      break;
    }
  }
}

uint32_t parseU32Or(const String& s, uint32_t fallback) {
  if (s.length() == 0) return fallback;
  return (uint32_t)strtoul(s.c_str(), nullptr, 0);
}

float parseFloatOr(const String& s, float fallback) {
  if (s.length() == 0) return fallback;
  return strtof(s.c_str(), nullptr);
}

}  // namespace

CommandRouter::CommandRouter(VacuumControl& control, Pump& pump, PressureSensor& sensor,
                             TelemetrySettings& telemetry)
    : control_(control), pump_(pump), sensor_(sensor), telemetry_(telemetry) {}

void CommandRouter::reply(ReplyQueue& out, const String& text) {
  if (!out.push("ok " + text)) LOGW("router: reply queue full");
}

void CommandRouter::fail(ReplyQueue& out, const String& text) {
  if (!out.push("err " + text)) LOGW("router: reply queue full");
}

void CommandRouter::emitHelp(ReplyQueue& out) {
  reply(out, "help");
  reply(out, "  ping | info | status | help");
  reply(out, "  stop | reset | reboot");
  reply(out, "  pump <0-100|off> | target <kPa|off> | ambient [kPa]");
  reply(out, "  telemetry <on|off|ms>");
}

void CommandRouter::handleLine(const String& rawLine, ReplyQueue& out,
                               const SystemSnapshot& snapshot) {
  String line = rawLine;
  line.trim();
  if (line.length() == 0 || line.startsWith("#")) return;

  String argv[kMaxArgs];
  size_t argc = 0;
  String rest;
  splitArgs(line, argv, kMaxArgs, argc, rest);
  String command = argv[0];
  command.toLowerCase();

  if (command == "help" || command == "?") {
    emitHelp(out);
    return;
  }
  if (command == "ping") {
    reply(out, "pong " + String(cfg::FIRMWARE_VERSION));
    return;
  }
  if (command == "info" || command == "status") {
    char body[224];
    if (command == "info") {
      snapshottext::infoBody(snapshot, body, sizeof(body));
    } else {
      snapshottext::statusBody(snapshot, body, sizeof(body));
    }
    reply(out, String(body));
    return;
  }
  if (command == "stop") {
    control_.stop();
    reply(out, "stopped");
    return;
  }
  if (command == "pump" || command == "manual") {
    if (argc < 2) {
      fail(out, "usage: pump <0-100|off>");
      return;
    }
    String value = argv[1];
    value.toLowerCase();
    if (value == "off" || value == "stop") {
      control_.stop();
      reply(out, "pump off");
      return;
    }
    float percent = parseFloatOr(argv[1], -1.0f);
    if (percent < 0.0f) {
      fail(out, "bad_argument");
      return;
    }
    control_.setManualDuty(percent);
    reply(out,
          "duty " + formatFloat(clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT), 1));
    return;
  }
  if (command == "target" || command == "hold") {
    if (argc < 2) {
      fail(out, "usage: target <kPa|off>");
      return;
    }
    String value = argv[1];
    value.toLowerCase();
    if (value == "off") {
      control_.disableTarget();
      reply(out, "target off");
      return;
    }
    float kpa = parseFloatOr(argv[1], -1.0f);
    if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
      fail(out, "out_of_range");
      return;
    }
    control_.setTarget(kpa);
    reply(out, "target " + formatFloat(kpa, 1));
    return;
  }
  if (command == "ambient") {
    if (argc > 1) {
      sensor_.setAmbientKpa(parseFloatOr(argv[1], -1.0f));
    } else {
      sensor_.captureAmbient();
    }
    reply(out, "ambient " + formatFloat(sensor_.ambientKpa(), 2));
    return;
  }
  if (command == "telemetry" || command == "tel") {
    if (argc < 2) {
      fail(out, "usage: telemetry <on|off|ms>");
      return;
    }
    String value = argv[1];
    value.toLowerCase();
    if (value == "on") {
      telemetry_.enabled = true;
    } else if (value == "off") {
      telemetry_.enabled = false;
    } else {
      uint32_t period = parseU32Or(argv[1], 0);
      if (period < cfg::TELEMETRY_MIN_MS) period = cfg::TELEMETRY_MIN_MS;
      telemetry_.periodMs = period;
      telemetry_.enabled = true;
    }
    reply(out, String("telemetry ") + (telemetry_.enabled ? "on " : "off ") +
                   String((unsigned)telemetry_.periodMs));
    return;
  }
  if (command == "reset") {
    pump_.resetFault();
    control_.clearFault();
    sensor_.clearErrors();
    reply(out, "reset");
    return;
  }
  if (command == "reboot" || command == "restart") {
    reply(out, "rebooting");
    delay(50);
    ESP.restart();
    return;
  }

  fail(out, "unknown_command");
}

}  // namespace eggplan
