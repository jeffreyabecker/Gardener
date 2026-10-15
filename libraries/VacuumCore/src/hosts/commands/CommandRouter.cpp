#include "hosts/commands/CommandRouter.h"

#include <ctype.h>
#include <string.h>

#include "core/Config.h"
#include "core/Format.h"
#include "core/Log.h"
#include "core/Math.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hosts/script/ScriptApi.h"
#include "hosts/script/ScriptRuntime.h"
#include "services/ProgramStore.h"
#include "services/SnapshotText.h"
#include "services/VacuumControl.h"

namespace eggplan {
namespace {

constexpr size_t kMaxArgs = 8;
// cat is paged: a reply that big would have to sit in the reply queue whole.
constexpr uint32_t kCatDefaultBytes = 512;
constexpr uint32_t kCatMaxBytes = 2048;

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

int parseIntOr(const String& s, int fallback) {
  if (s.length() == 0) return fallback;
  char* end = nullptr;
  long value = strtol(s.c_str(), &end, 10);
  return end == s.c_str() ? fallback : (int)value;
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

CommandRouter::CommandRouter(ProgramStore& store, ScriptApi& api, ScriptRuntime& runtime,
                             VacuumControl& control, Pump& pump, PressureSensor& sensor,
                             TelemetrySettings& telemetry, UiSettings& ui)
    : store_(store),
      api_(api),
      runtime_(runtime),
      control_(control),
      pump_(pump),
      sensor_(sensor),
      telemetry_(telemetry),
      ui_(ui) {}

void CommandRouter::reply(ReplyQueue& out, const String& text) {
  if (!out.push("ok " + text)) LOGW("router: reply queue full");
}

void CommandRouter::fail(ReplyQueue& out, const String& text) {
  if (!out.push("err " + text)) LOGW("router: reply queue full");
}

void CommandRouter::emitHelp(ReplyQueue& out) {
  reply(out, "help");
  reply(out, "  ping | info | status | log [n] | help");
  reply(out, "  list | cat <name> [offset] [len] | rm <name>");
  reply(out, "  run <name> [args] | stop | eval <berry>");
  reply(out, "  pump <0-100|off> | target <kPa|off> | ambient [kPa]");
  reply(out, "  autostart [<name>|none] | telemetry <on|off|ms> | reset");
}

void CommandRouter::emitList(ReplyQueue& out, const SystemSnapshot& snapshot) {
  String rows = snapshottext::programRows(snapshot);
  reply(out, "programs " + String((unsigned)snapshot.programCount));
  if (rows.length()) out.push(rows);
  out.push("eof");
}

void CommandRouter::emitCat(const String& name, uint32_t offset, uint32_t length,
                            ReplyQueue& out) {
  String contents;
  if (!store_.read(name.c_str(), contents)) {
    fail(out, "not found");
    return;
  }
  uint32_t total = contents.length();
  if (offset > total) offset = total;
  uint32_t available = total - offset;
  if (length == 0) length = kCatDefaultBytes;
  if (length > kCatMaxBytes) length = kCatMaxBytes;
  if (length > available) length = available;

  char header[96];
  snprintf(header, sizeof(header), "file %s %u %u %u", name.c_str(), (unsigned)offset,
           (unsigned)length, (unsigned)total);
  reply(out, String(header));
  out.push(contents.substring(offset, offset + length));
  out.push("eof");
}

void CommandRouter::emitLog(int lines, ReplyQueue& out) {
  if (lines <= 0) lines = 20;
  const char* tail = logTail();
  int count = 0;
  size_t length = strlen(tail);
  size_t index = length;
  while (index > 0 && count <= lines) {
    index--;
    if (tail[index] == '\n' && index != length - 1) count++;
  }
  reply(out, "log");
  out.push(String(tail + index));
  out.push("eof");
}

void CommandRouter::cmdRun(const String& name, const String& args, ReplyQueue& out) {
  if (!runtime_.available()) {
    fail(out, "script runtime not built in");
    return;
  }
  String source;
  if (!store_.read(name.c_str(), source)) {
    fail(out, "not found");
    return;
  }
  if (runtime_.running()) runtime_.stop("replaced");
  api_.setProgramArgs(args);
  String error;
  if (!runtime_.start(name.c_str(), source, error)) {
    fail(out, error);
    return;
  }
  reply(out, "running " + name);
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
  if (command == "log") {
    emitLog(argc > 1 ? parseIntOr(argv[1], 20) : 20, out);
    return;
  }
  if (command == "list" || command == "ls") {
    emitList(out, snapshot);
    return;
  }
  if (command == "cat") {
    if (argc < 2) {
      fail(out, "usage: cat <name> [offset] [len]");
      return;
    }
    emitCat(argv[1], argc > 2 ? parseU32Or(argv[2], 0) : 0,
            argc > 3 ? parseU32Or(argv[3], 0) : 0, out);
    return;
  }
  if (command == "rm" || command == "del") {
    if (argc < 2 || !store_.remove(argv[1].c_str())) {
      fail(out, "not found");
      return;
    }
    reply(out, "removed " + argv[1]);
    return;
  }
  if (command == "run") {
    if (argc < 2) {
      fail(out, "usage: run <name> [args]");
      return;
    }
    cmdRun(argv[1], rest, out);
    return;
  }
  if (command == "stop") {
    runtime_.stop("stopped by command");
    control_.stop();
    pump_.stop();
    reply(out, "stopped");
    return;
  }
  if (command == "eval" || command == "repl") {
    if (!runtime_.available()) {
      fail(out, "script runtime not built in");
      return;
    }
    String result;
    if (!runtime_.evaluate(rest, result)) {
      fail(out, result);
      return;
    }
    reply(out, result);
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
      fail(out, "bad duty");
      return;
    }
    control_.setManualDuty(percent);
    reply(out, "duty " + formatFloat(clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT), 1));
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
      fail(out, "bad target");
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
  if (command == "autostart") {
    if (argc < 2) {
      const char* current = store_.autostartName();
      reply(out, String("autostart ") + (current[0] ? current : "none"));
      return;
    }
    String value = argv[1];
    if (!store_.setAutostart(value == "none" ? "" : value.c_str())) {
      fail(out, "not found");
      return;
    }
    reply(out, "autostart " + value);
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
  if (command == "buttons") {
    // Asks the local UI to move on; the router does not know what a page is.
    ui_.pageStep = 1;
    reply(out, "page next");
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

  fail(out, "unknown command");
}

}  // namespace eggplan
