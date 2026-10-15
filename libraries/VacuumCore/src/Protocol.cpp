#include "Protocol.h"

#include <ctype.h>

#include "BleService.h"
#include "Buttons.h"
#include "DisplayUI.h"
#include "Log.h"
#include "PressureSensor.h"
#include "Pump.h"
#include "ScriptRuntime.h"
#include "Util.h"
#include "VacuumController.h"

namespace eggplan {
namespace {

constexpr size_t kMaxArgs = 8;

void splitArgs(const String& line, String* argv, size_t maxArgs, size_t& argc,
               String& rest) {
  argc = 0;
  rest = "";
  int i = 0;
  int len = (int)line.length();
  while (i < len) {
    while (i < len && isspace((unsigned char)line[i])) i++;
    if (i >= len) break;
    int start = i;
    while (i < len && !isspace((unsigned char)line[i])) i++;
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
  long v = strtol(s.c_str(), &end, 10);
  if (end == s.c_str()) return fallback;
  return (int)v;
}

uint32_t parseU32Or(const String& s, uint32_t fallback) {
  if (s.length() == 0) return fallback;
  return (uint32_t)strtoul(s.c_str(), nullptr, 0);
}

float parseFloatOr(const String& s, float fallback) {
  if (s.length() == 0) return fallback;
  return strtof(s.c_str(), nullptr);
}

String ok(const String& text) { return "ok " + text; }
String err(const String& text) { return "err " + text; }

}  // namespace

// ---------------------------------------------------------------------------
// Program transfer
// ---------------------------------------------------------------------------

String ProgramUploader::handle(const uint8_t* data, size_t len) {
  if (len == 0) return err("empty frame");
  if (!store_) return err("no store");
  uint8_t opcode = data[0];
  const uint8_t* payload = data + 1;
  size_t payloadLen = len - 1;

  switch (opcode) {
    case progproto::kBegin: {
      String name((const char*)payload, payloadLen);
      name.trim();
      if (!ProgramStore::validName(name)) {
        return err("invalid name");
      }
      if (!store_->writeBegin(name)) {
        return err("cannot start upload");
      }
      chunks_ = 0;
      return ok("begin " + name);
    }
    case progproto::kData: {
      if (!store_->writing()) return err("no upload in progress");
      if (!store_->writeAppend(payload, payloadLen)) {
        abort("write failed");
        return err("write failed");
      }
      chunks_++;
      return ok("data " + String((unsigned)store_->writeBytes()));
    }
    case progproto::kEnd: {
      if (!store_->writing()) return err("no upload in progress");
      uint32_t crc = 0;
      uint32_t size = 0;
      if (payloadLen >= 4) memcpy(&crc, payload, 4);
      if (payloadLen >= 8) memcpy(&size, payload + 4, 4);
      uint32_t bytes = store_->writeBytes();
      String name = store_->writingName();
      String error;
      if (!store_->writeCommit(crc, size, error)) {
        return err(error);
      }
      return ok("stored " + name + " " + String((unsigned)bytes));
    }
    case progproto::kAbort:
      abort("client aborted");
      return ok("aborted");
    default:
      return err("unknown opcode");
  }
}

void ProgramUploader::abort(const char* reason) {
  if (store_ && store_->writing()) {
    store_->writeAbort();
    LOGW("upload: aborted (%s)", reason ? reason : "?");
  }
  chunks_ = 0;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

String CommandRouter::infoJson() const {
  String s = "{";
  s += "\"fw\":\"" + String(cfg::FIRMWARE_VERSION) + "\"";
  s += ",\"chip\":\"" + String(ESP.getChipModel()) + "\"";
  s += ",\"idf\":\"" + String(ESP.getSdkVersion()) + "\"";
  s += ",\"runtime\":\"" + String(ctx_.runtime ? ctx_.runtime->name() : "none") + "\"";
  s += ",\"programs\":" + String((unsigned)(ctx_.store ? ctx_.store->count() : 0));
  s += ",\"store_kb\":" + String((unsigned)(ctx_.store ? ctx_.store->totalBytes() / 1024 : 0));
  s += ",\"sensor\":" + String((ctx_.sensor && ctx_.sensor->present()) ? "true" : "false");
  s += ",\"tach\":" + String((ctx_.pump && ctx_.pump->tachEnabled()) ? "true" : "false");
  s += ",\"heap\":" + String((unsigned)ESP.getFreeHeap());
  s += "}";
  return s;
}

String CommandRouter::statusJson() const {
  String s = "{";
  if (ctx_.sensor) {
    s += "\"kpa\":" + formatFloat(ctx_.sensor->kpa(), 2);
    s += ",\"vacuum\":" + formatFloat(ctx_.sensor->vacuumKpa(), 2);
    s += ",\"vacuum_pct\":" + formatFloat(ctx_.sensor->vacuumPercent(), 1);
    s += ",\"ambient\":" + formatFloat(ctx_.sensor->ambientKpa(), 2);
    s += ",\"sensor\":" + String(ctx_.sensor->present() ? "true" : "false");
  }
  if (ctx_.pump) {
    s += ",\"duty\":" + formatFloat(ctx_.pump->dutyPercent(), 1);
    s += ",\"rpm\":" + formatFloat(ctx_.pump->rpm(), 0);
    s += ",\"fault\":" + String(ctx_.pump->tripped() ? "\"" + String(ctx_.pump->faultReason()) + "\"" : "null");
  }
  if (ctx_.control) {
    s += ",\"state\":\"" + String(ctx_.control->stateText()) + "\"";
    s += ",\"target\":" + String(ctx_.control->targetActive() ? formatFloat(ctx_.control->targetKpa(), 2) : String("null"));
  }
  if (ctx_.store) {
    String autostart = ctx_.store->autostart();
    s += ",\"autostart\":\"" + jsonEscape(autostart) + "\"";
  }
  if (ctx_.runtime) {
    s += ",\"running\":\"" + jsonEscape(String(ctx_.runtime->running() ? ctx_.runtime->currentProgram() : "")) + "\"";
  }
  if (ctx_.ble) {
    s += ",\"ble\":" + String(ctx_.ble->connected() ? "true" : "false");
  }
  s += ",\"uptime_ms\":" + String((unsigned)millis());
  s += "}";
  return s;
}

void CommandRouter::emitHelp(ResponseSink& out) {
  out.emit(ok("help"));
  out.emit("  ping | info | status | log [n] | help");
  out.emit("  list | cat <name> [offset] [len] | rm <name>");
  out.emit("  run <name> [args] | stop | eval <berry>");
  out.emit("  pump <0-100|off> | target <kpa|off> | ambient [kpa]");
  out.emit("  autostart [<name>|none] | telemetry <on|off|ms> | reset");
}

void CommandRouter::emitList(ResponseSink& out) {
  if (!ctx_.store) {
    out.emit(err("no store"));
    return;
  }
  String autostart = ctx_.store->autostart();
  String s = "[";
  for (size_t i = 0; i < ctx_.store->count(); i++) {
    const ProgramInfo* info = ctx_.store->at(i);
    if (!info) break;
    if (i) s += ",";
    s += "{\"name\":\"" + jsonEscape(info->name) + "\"";
    s += ",\"size\":" + String((unsigned)info->size);
    char crcbuf[16];
    snprintf(crcbuf, sizeof(crcbuf), "0x%08X", (unsigned)info->crc);
    s += ",\"crc\":\"" + String(crcbuf) + "\"";
    s += ",\"autostart\":" + String(info->name == autostart ? "true" : "false");
    s += "}";
  }
  s += "]";
  out.emit(ok(s));
}

void CommandRouter::emitCat(const String& name, uint32_t offset, uint32_t length,
                            ResponseSink& out) {
  if (!ctx_.store) {
    out.emit(err("no store"));
    return;
  }
  String contents;
  if (!ctx_.store->read(name, contents)) {
    out.emit(err("not found"));
    return;
  }
  uint32_t total = contents.length();
  if (offset > total) offset = total;
  uint32_t available = total - offset;
  if (length == 0 || length > available) length = available;

  char header[96];
  snprintf(header, sizeof(header), "file %s %u %u %u", name.c_str(), (unsigned)offset,
           (unsigned)length, (unsigned)total);
  out.emit(ok(String(header)));
  // The sink is responsible for splitting this across BLE notifications.
  out.emit(contents.substring(offset, offset + length));
  out.emit("eof");
}

void CommandRouter::emitLog(int lines, ResponseSink& out) {
  if (lines <= 0) lines = 20;
  const char* tail = logTail();
  // Walk backwards to find the start of the requested number of lines.
  int count = 0;
  size_t len = strlen(tail);
  size_t idx = len;
  while (idx > 0 && count <= lines) {
    idx--;
    if (tail[idx] == '\n' && idx != len - 1) count++;
  }
  out.emit(ok("log"));
  out.emit(String(tail + idx));
  out.emit("eof");
}

void CommandRouter::cmdRun(const String& name, const String& args, ResponseSink& out) {
  if (!ctx_.runtime || !ctx_.store) {
    out.emit(err("no runtime"));
    return;
  }
  if (!ctx_.runtime->available()) {
    out.emit(err("script runtime not built in"));
    return;
  }
  String source;
  if (!ctx_.store->read(name, source)) {
    out.emit(err("not found"));
    return;
  }
  if (ctx_.runtime->running()) ctx_.runtime->stop("replaced");
  if (ctx_.api) ctx_.api->setProgramArgs(args);
  String error;
  if (!ctx_.runtime->start(name.c_str(), source, error)) {
    out.emit(err(error));
    return;
  }
  out.emit(ok("running " + name));
}

void CommandRouter::handleLine(const String& rawLine, ResponseSink& out) {
  String line = rawLine;
  line.trim();
  if (line.length() == 0) return;
  if (line.startsWith("#")) return;

  String argv[kMaxArgs];
  size_t argc = 0;
  String rest;
  splitArgs(line, argv, kMaxArgs, argc, rest);
  String cmd = argv[0];
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    emitHelp(out);
    return;
  }
  if (cmd == "ping") {
    out.emit(ok("pong " + String(cfg::FIRMWARE_VERSION)));
    return;
  }
  if (cmd == "info") {
    out.emit(ok(infoJson()));
    return;
  }
  if (cmd == "status") {
    out.emit(ok(statusJson()));
    return;
  }
  if (cmd == "log") {
    emitLog(argc > 1 ? parseIntOr(argv[1], 20) : 20, out);
    return;
  }
  if (cmd == "list" || cmd == "ls") {
    emitList(out);
    return;
  }
  if (cmd == "cat") {
    if (argc < 2) {
      out.emit(err("usage: cat <name> [offset] [len]"));
      return;
    }
    emitCat(argv[1], argc > 2 ? parseU32Or(argv[2], 0) : 0,
            argc > 3 ? parseU32Or(argv[3], 0) : 0, out);
    return;
  }
  if (cmd == "rm" || cmd == "del") {
    if (argc < 2 || !ctx_.store || !ctx_.store->remove(argv[1])) {
      out.emit(err("not found"));
      return;
    }
    out.emit(ok("removed " + argv[1]));
    return;
  }
  if (cmd == "run") {
    if (argc < 2) {
      out.emit(err("usage: run <name> [args]"));
      return;
    }
    cmdRun(argv[1], rest, out);
    return;
  }
  if (cmd == "stop") {
    if (ctx_.runtime) ctx_.runtime->stop("stopped by command");
    if (ctx_.control) ctx_.control->stop();
    if (ctx_.pump) ctx_.pump->stop();
    out.emit(ok("stopped"));
    return;
  }
  if (cmd == "eval" || cmd == "repl") {
    if (!ctx_.runtime || !ctx_.runtime->available()) {
      out.emit(err("script runtime not built in"));
      return;
    }
    String result;
    if (!ctx_.runtime->evaluate(rest, result)) {
      out.emit(err(result));
      return;
    }
    out.emit(ok(result));
    return;
  }
  if (cmd == "pump" || cmd == "manual") {
    if (!ctx_.pump || argc < 2) {
      out.emit(err("usage: pump <0-100|off>"));
      return;
    }
    String value = argv[1];
    value.toLowerCase();
    if (value == "off" || value == "stop") {
      if (ctx_.control) ctx_.control->stop();
      out.emit(ok("pump off"));
    } else {
      float percent = clampf(parseFloatOr(argv[1], -1.0f), 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
      if (percent < 0.0f) {
        out.emit(err("bad duty"));
        return;
      }
      if (ctx_.control) ctx_.control->setManualDuty(percent);
      out.emit(ok("duty " + formatFloat(percent, 1)));
    }
    return;
  }
  if (cmd == "target" || cmd == "hold") {
    if (!ctx_.control || argc < 2) {
      out.emit(err("usage: target <kpa|off>"));
      return;
    }
    String value = argv[1];
    value.toLowerCase();
    if (value == "off") {
      ctx_.control->disableTarget();
      out.emit(ok("target off"));
    } else {
      float kpa = parseFloatOr(argv[1], -1.0f);
      if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) {
        out.emit(err("bad target"));
        return;
      }
      ctx_.control->setTarget(kpa);
      out.emit(ok("target " + formatFloat(kpa, 2)));
    }
    return;
  }
  if (cmd == "ambient") {
    if (!ctx_.sensor) {
      out.emit(err("no sensor"));
      return;
    }
    if (argc > 1) {
      ctx_.sensor->setAmbientKpa(parseFloatOr(argv[1], -1.0f));
    } else {
      ctx_.sensor->captureAmbient();
    }
    out.emit(ok("ambient " + formatFloat(ctx_.sensor->ambientKpa(), 2)));
    return;
  }
  if (cmd == "autostart") {
    if (!ctx_.store) {
      out.emit(err("no store"));
      return;
    }
    if (argc < 2) {
      String current = ctx_.store->autostart();
      out.emit(ok("autostart " + (current.length() ? current : String("none"))));
      return;
    }
    if (!ctx_.store->setAutostart(argv[1] == "none" ? String("") : argv[1])) {
      out.emit(err("not found"));
      return;
    }
    out.emit(ok("autostart " + argv[1]));
    return;
  }
  if (cmd == "telemetry" || cmd == "tel") {
    if (!ctx_.ble || argc < 2) {
      out.emit(err("usage: telemetry <on|off|ms>"));
      return;
    }
    String value = argv[1];
    value.toLowerCase();
    if (value == "on") {
      ctx_.ble->setTelemetry(true, 0);
    } else if (value == "off") {
      ctx_.ble->setTelemetry(false, 0);
    } else {
      uint32_t period = parseU32Or(argv[1], 0);
      if (period < 50) period = 50;
      ctx_.ble->setTelemetry(true, period);
    }
    out.emit(ok(String("telemetry ") + (ctx_.ble->telemetryEnabled() ? "on " : "off ") +
                String((unsigned)ctx_.ble->telemetryPeriodMs())));
    return;
  }
  if (cmd == "reset") {
    if (ctx_.pump) ctx_.pump->resetFault();
    if (ctx_.control) ctx_.control->clearFault();
    if (ctx_.sensor) ctx_.sensor->clearErrors();
    out.emit(ok("reset"));
    return;
  }
  if (cmd == "buttons") {
    if (ctx_.ui) ctx_.ui->nextPage(1);
    out.emit(ok("page " + String(ctx_.ui ? ctx_.ui->pageName() : "?")));
    return;
  }
  if (cmd == "reboot" || cmd == "restart") {
    out.emit(ok("rebooting"));
    delay(50);
    ESP.restart();
    return;
  }

  out.emit(err("unknown command"));
}

}  // namespace eggplan
