#include "services/SnapshotText.h"

#include <Arduino.h>

#include "core/Config.h"
#include "core/Text.h"

namespace eggplan {
namespace snapshottext {
namespace {

// Every value is a space free token, so '-' can mean "not applicable" without any
// quoting or escaping, and the payload is built by hand rather than by printf. The token
// writer is also where the "no spaces in a value" invariant is enforced, because a
// library string is the one value the firmware does not control.
void addToken(text::Buf& buf, const char* key, const char* value) {
  if (buf.length() > 0) buf << ' ';
  buf << key << '=';
  if (!value || !value[0]) {
    buf << '-';
    return;
  }
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    char c = *cursor;
    buf << (c == ' ' || c == '\t' || c == '\n' || c == '\r' ? '_' : c);
  }
}

void addFloatToken(text::Buf& buf, const char* key, float value, uint8_t decimals) {
  if (buf.length() > 0) buf << ' ';
  buf << key << '=';
  buf.add(value, decimals);
}

void addU32Token(text::Buf& buf, const char* key, uint32_t value) {
  if (buf.length() > 0) buf << ' ';
  buf << key << '=' << value;
}

}  // namespace

const char* stateToken(RunState state) { return runStateName(state); }

const char* faultToken(const SystemSnapshot& snapshot) {
  return snapshot.pumpTripped ? snapshot.faultCode : "none";
}

size_t statusBody(const SystemSnapshot& snapshot, char* out, size_t size) {
  if (size == 0) return 0;
  text::Buf buf(out, size);

  addFloatToken(buf, "kpa", snapshot.kpa, 2);
  addFloatToken(buf, "vac", snapshot.vacuumKpa, 2);
  addFloatToken(buf, "pct", snapshot.vacuumPercent, 1);
  if (snapshot.targetActive) {
    addFloatToken(buf, "target", snapshot.targetKpa, 1);
  } else {
    addToken(buf, "target", "-");
  }
  addFloatToken(buf, "floor", snapshot.floorKpa, 1);
  addFloatToken(buf, "duty", snapshot.duty, 1);
  addToken(buf, "state", stateToken(snapshot.state));
  if (snapshot.totalSeconds > 0) {
    addU32Token(buf, "remain", snapshot.remainSeconds);
    addU32Token(buf, "total", snapshot.totalSeconds);
  } else {
    addToken(buf, "remain", "-");
    addToken(buf, "total", "-");
  }
  if (snapshot.batteryPresent) {
    addU32Token(buf, "batt", snapshot.batteryPercent);
  } else {
    addToken(buf, "batt", "-");
  }
  addToken(buf, "sensor", snapshot.sensorOk ? "1" : "0");
  addToken(buf, "fault", faultToken(snapshot));
  addU32Token(buf, "up", snapshot.uptimeMs);

  return buf.length();
}

size_t infoBody(const SystemSnapshot& snapshot, const LinkSettings& settings, char* out,
                size_t size) {
  if (size == 0) return 0;
  text::Buf buf(out, size);

  addToken(buf, "proto", cfg::PROTOCOL_VERSION);
  addToken(buf, "fw", cfg::FIRMWARE_VERSION);
  // Read where they are formatted: two facts from the chip, not a module's state.
  addToken(buf, "chip", ESP.getChipModel());
  addU32Token(buf, "heap", (uint32_t)ESP.getFreeHeap());
  if (snapshot.batteryPresent) {
    addFloatToken(buf, "batt_v", snapshot.batteryVolts, 2);
    addU32Token(buf, "batt_pct", snapshot.batteryPercent);
  } else {
    addToken(buf, "batt_v", "-");
    addToken(buf, "batt_pct", "-");
  }
  addToken(buf, "log", settings.logEnabled ? "on" : "off");

  return buf.length();
}

}  // namespace snapshottext
}  // namespace eggplan
