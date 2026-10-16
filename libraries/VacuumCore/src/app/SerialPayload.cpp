#include "app/SerialLink.h"

#include <Arduino.h>

#include "core/Config.h"
#include "core/Snapshot.h"

namespace gardener {
namespace {

// Every value is a space free token, so '-' can mean "not applicable" without any quoting
// or escaping. This is also where the "no spaces in a value" rule is enforced, because a
// library string is the one value the firmware does not control.
void addToken(String& buf, const char* key, const char* value) {
  if (buf.length() > 0) buf += ' ';
  buf += key;
  buf += '=';
  if (!value || !value[0]) {
    buf += '-';
    return;
  }
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    char c = *cursor;
    buf += (c == ' ' || c == '\t' || c == '\n' || c == '\r') ? '_' : c;
  }
}

void addFloatToken(String& buf, const char* key, float value, uint8_t decimals) {
  if (buf.length() > 0) buf += ' ';
  buf += key;
  buf += '=';
  // The cast picks String(float, decimals) out of the overload set: a uint8_t also matches
  // the integer constructors, whose base parameter is an unsigned char.
  buf += String(value, (unsigned int)decimals);
}

void addU32Token(String& buf, const char* key, uint32_t value) {
  if (buf.length() > 0) buf += ' ';
  buf += key;
  buf += '=';
  buf += String(value);
}

}  // namespace

// --- payload text ----------------------------------------------------------

String SerialLink::statusBody() const {
  String buf;

  addFloatToken(buf, "kpa", snapshot_.plant.kpa, 2);
  addFloatToken(buf, "vac", snapshot_.plant.vacuumKpa, 2);
  addFloatToken(buf, "pct", snapshot_.plant.vacuumPercent, 1);
  if (snapshot_.plant.targetKpa > 0.0f) {
    addFloatToken(buf, "target", snapshot_.plant.targetKpa, 1);
  } else {
    addToken(buf, "target", "-");
  }
  addFloatToken(buf, "floor", cfg::CONTROL_FLOOR_KPA, 1);
  addFloatToken(buf, "duty", snapshot_.plant.duty, 1);
  addToken(buf, "state", runStateName(snapshot_.plant.runState));
  if (snapshot_.batteryPresent) {
    addU32Token(buf, "batt", snapshot_.batteryPercent);
  } else {
    addToken(buf, "batt", "-");
  }
  addToken(buf, "sensor", snapshot_.plant.sensorOk ? "1" : "0");
  addToken(buf, "fault", snapshot_.plant.faultCode[0] ? snapshot_.plant.faultCode : "none");
  addU32Token(buf, "up", snapshot_.uptimeMs);

  return buf;
}

String SerialLink::infoBody() const {
  String buf;

  addToken(buf, "fw", cfg::FIRMWARE_VERSION);
  // Read where they are formatted: two facts from the chip, not a module's state.
  addToken(buf, "chip", ESP.getChipModel());
  addU32Token(buf, "heap", (uint32_t)ESP.getFreeHeap());
  if (snapshot_.batteryPresent) {
    addFloatToken(buf, "batt_v", snapshot_.batteryVolts, 2);
    addU32Token(buf, "batt_pct", snapshot_.batteryPercent);
  } else {
    addToken(buf, "batt_v", "-");
    addToken(buf, "batt_pct", "-");
  }
  addToken(buf, "log", logEnabled_ ? "on" : "off");

  return buf;
}

}  // namespace gardener
