#include "services/SnapshotText.h"

#include <stdio.h>
#include <string.h>

#include "core/Config.h"
#include "core/Format.h"

namespace eggplan {
namespace snapshottext {
namespace {

// Every value is a space free token, so '-' can mean "not applicable" without
// quoting anything.
void appendToken(char* out, size_t size, const char* key, const char* value) {
  size_t used = strlen(out);
  if (used >= size - 1) return;
  snprintf(out + used, size - used, "%s=%s ", key, value && value[0] ? value : "-");
}

void appendToken(char* out, size_t size, const char* key, float value, uint8_t decimals) {
  appendToken(out, size, key, formatFloat(value, decimals).c_str());
}

void appendToken(char* out, size_t size, const char* key, int value) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", value);
  appendToken(out, size, key, buffer);
}

void trimTrailingSpace(char* out) {
  size_t used = strlen(out);
  while (used > 0 && out[used - 1] == ' ') out[--used] = '\0';
}

}  // namespace

const char* stateToken(ControlState state) { return controlStateName(state); }

const char* faultToken(const SystemSnapshot& snapshot) {
  return snapshot.pumpTripped ? snapshot.faultCode : "none";
}

size_t statusBody(const SystemSnapshot& snapshot, char* out, size_t size) {
  if (size == 0) return 0;
  out[0] = '\0';

  appendToken(out, size, "kpa", snapshot.kpa, 2);
  appendToken(out, size, "vac", snapshot.vacuumKpa, 2);
  appendToken(out, size, "pct", snapshot.vacuumPercent, 1);
  appendToken(out, size, "duty", snapshot.duty, 1);
  appendToken(out, size, "state", stateToken(snapshot.state));
  if (snapshot.targetActive) {
    appendToken(out, size, "target", snapshot.targetKpa, 1);
  } else {
    appendToken(out, size, "target", "-");
  }
  appendToken(out, size, "floor", snapshot.floorKpa, 1);
  appendToken(out, size, "fault", faultToken(snapshot));
  appendToken(out, size, "up", (int)snapshot.uptimeMs);
  appendToken(out, size, "ble", snapshot.bleConnected ? 1 : 0);

  trimTrailingSpace(out);
  return strlen(out);
}

size_t infoBody(const SystemSnapshot& snapshot, char* out, size_t size) {
  if (size == 0) return 0;
  out[0] = '\0';

  appendToken(out, size, "fw", cfg::FIRMWARE_VERSION);
  appendToken(out, size, "chip", snapshot.device ? snapshot.device->chip : "-");
  appendToken(out, size, "runtime", snapshot.runtime);
  appendToken(out, size, "progs", (int)snapshot.programCount);
  appendToken(out, size, "store_kb", (int)snapshot.storeTotalKb);
  appendToken(out, size, "sensor", snapshot.sensorOk ? 1 : 0);
  appendToken(out, size, "tach", snapshot.tachEnabled ? 1 : 0);
  appendToken(out, size, "heap", snapshot.device ? (int)snapshot.device->freeHeap : 0);

  trimTrailingSpace(out);
  return strlen(out);
}

String programRows(const SystemSnapshot& snapshot) {
  String rows;
  for (size_t i = 0; i < snapshot.programCount; i++) {
    const ProgramInfo& info = snapshot.programs[i];
    char checksum[16];
    snprintf(checksum, sizeof(checksum), "0x%08X", (unsigned)info.checksum);
    rows += "p ";
    rows += info.name;
    rows += " ";
    rows += String((unsigned)info.size);
    rows += " ";
    rows += checksum;
    rows += info.autostart ? " auto" : " -";
    if (i + 1 < snapshot.programCount) rows += "\n";
  }
  return rows;
}

}  // namespace snapshottext
}  // namespace eggplan
