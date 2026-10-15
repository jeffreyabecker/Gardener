#include "hosts/commands/CommandRouter.h"

#include <ctype.h>
#include <string.h>

#include "core/Config.h"
#include "core/Format.h"
#include "core/Log.h"
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

// An argument that does not parse in full is a bad_argument, never a silent zero.
bool parseFloat(const String& text, float& out) {
  if (text.length() == 0) return false;
  if (!isdigit((unsigned char)text[0]) && text[0] != '-' && text[0] != '.') return false;
  char* end = nullptr;
  float value = strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0') return false;
  out = value;
  return true;
}

bool parseU32(const String& text, uint32_t& out) {
  if (text.length() == 0 || !isdigit((unsigned char)text[0])) return false;
  char* end = nullptr;
  unsigned long value = strtoul(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0') return false;
  out = (uint32_t)value;
  return true;
}

}  // namespace

CommandRouter::CommandRouter(VacuumControl& control, Pump& pump, PressureSensor& sensor,
                             LinkSettings& settings)
    : control_(control), pump_(pump), sensor_(sensor), settings_(settings) {}

void CommandRouter::reply(Print& out, const String& text) {
  out.print("ok ");
  out.print(text);
  out.print('\n');
}

void CommandRouter::fail(Print& out, const String& text) {
  out.print("err ");
  out.print(text);
  out.print('\n');
}

void CommandRouter::emitHelp(Print& out) {
  // One line, because every command produces exactly one reply and that is what lets a
  // controller match replies to commands by counting. Argument syntax lives in
  // docs/serial-protocol.md rather than on the wire.
  reply(out, "help=info|status|help|start|stop|reset|reboot|ambient|set");
}

void CommandRouter::handleLine(const String& rawLine, Print& out,
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

  if (command == "help") {
    emitHelp(out);
    return;
  }
  if (command == "info" || command == "status") {
    char body[224];
    if (command == "info") {
      snapshottext::infoBody(snapshot, settings_, body, sizeof(body));
    } else {
      snapshottext::statusBody(snapshot, body, sizeof(body));
    }
    reply(out, String(body));
    return;
  }
  if (command == "start") {
    if (!control_.startRun()) {
      fail(out, "not_ready");
      return;
    }
    reply(out, "started");
    return;
  }
  if (command == "stop") {
    control_.stopRun();
    reply(out, "stopped");
    return;
  }
  if (command == "set") {
    if (argc < 3) {
      fail(out, "bad_argument");
      return;
    }
    String key = argv[1];
    key.toLowerCase();
    String value = argv[2];
    value.toLowerCase();

    if (key == "target") {
      if (value == "off") {
        control_.clearTarget();
        reply(out, "target=off");
        return;
      }
      float kpa = 0.0f;
      if (!parseFloat(argv[2], kpa)) {
        fail(out, "bad_argument");
        return;
      }
      if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
        fail(out, "out_of_range");
        return;
      }
      control_.setTarget(kpa);
      reply(out, "target=" + formatFloat(kpa, 1));
      return;
    }
    if (key == "timer") {
      if (value == "off") {
        control_.setDurationSeconds(0);
        reply(out, "timer=off");
        return;
      }
      uint32_t seconds = 0;
      if (!parseU32(argv[2], seconds)) {
        fail(out, "bad_argument");
        return;
      }
      control_.setDurationSeconds(seconds);
      reply(out, "timer=" + String((unsigned)seconds));
      return;
    }
    if (key == "ambient") {
      float kpa = 0.0f;
      if (!parseFloat(argv[2], kpa)) {
        fail(out, "bad_argument");
        return;
      }
      if (!sensor_.setAmbientKpa(kpa)) {
        fail(out, "out_of_range");
        return;
      }
      reply(out, "ambient=" + formatFloat(sensor_.ambientKpa(), 2));
      return;
    }
    // v2 configured the telemetry period; telemetry is polled now, so the whole family
    // answers the same way an old `set duty` does.
    if (key == "period") {
      fail(out, "unsupported");
      return;
    }
    if (key == "log") {
      if (value != "on" && value != "off") {
        fail(out, "bad_argument");
        return;
      }
      settings_.logEnabled = (value == "on");
      reply(out, value == "on" ? "log=on" : "log=off");
      return;
    }
    // v2 could send commands with a `pump`/`manual` verb; the removed ones answer
    // unsupported rather than unknown_command so an old client learns why.
    if (key == "duty") {
      fail(out, "unsupported");
      return;
    }
    fail(out, "unknown_command");
    return;
  }
  if (command == "ambient") {
    if (argc > 1) {
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
    reply(out, "ambient=" + formatFloat(sensor_.ambientKpa(), 2));
    return;
  }
  // v2 turned telemetry on and off by command; a controller polls `status` now. Old
  // clients get a straight answer.
  if (command == "telemetry" || command == "tel") {
    fail(out, "unsupported");
    return;
  }
  if (command == "reset") {
    pump_.resetFault();
    control_.clearFault();
    sensor_.clearErrors();
    reply(out, "reset");
    return;
  }
  if (command == "reboot") {
    reply(out, "rebooting");
    delay(50);
    ESP.restart();
    return;
  }

  fail(out, "unknown_command");
}

}  // namespace eggplan
