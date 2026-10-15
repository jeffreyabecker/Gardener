#include "hal/Device.h"

#include <Arduino.h>
#include <string.h>

namespace eggplan {
namespace {

// Wire values are space separated tokens, so a space in a device string would break
// the framing. Sanitise at the source rather than escaping downstream.
void copyTokenSafe(char* destination, size_t size, const char* source) {
  if (size == 0) return;
  size_t i = 0;
  if (source) {
    for (; i + 1 < size && source[i] != '\0'; i++) {
      char c = source[i];
      destination[i] = (c == ' ' || c == '\t' || c == '\n' || c == '\r') ? '_' : c;
    }
  }
  destination[i] = '\0';
}

}  // namespace

DeviceInfo readDeviceInfo() {
  DeviceInfo info;
  copyTokenSafe(info.chip, sizeof(info.chip), ESP.getChipModel());
  copyTokenSafe(info.sdk, sizeof(info.sdk), ESP.getSdkVersion());
  info.freeHeap = ESP.getFreeHeap();
  return info;
}

}  // namespace eggplan
