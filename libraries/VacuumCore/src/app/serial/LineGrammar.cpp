#include "app/serial/LineGrammar.h"

#include <string.h>

namespace gardener {

void lower(char* text) {
  for (; *text != '\0'; text++) {
    if (*text >= 'A' && *text <= 'Z') *text = (char)(*text - 'A' + 'a');
  }
}

bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

bool sameFold(const char* a, const char* b) {
  for (;; a++, b++) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
    if (ca == '\0') return true;
  }
}

bool parseFlag(const char* text, bool& out) {
  if (sameFold(text, "on")) {
    out = true;
    return true;
  }
  if (sameFold(text, "off")) {
    out = false;
    return true;
  }
  return false;
}

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

char* splitValue(char* token) {
  char* equals = strchr(token, '=');
  if (!equals) return nullptr;
  *equals = '\0';
  return equals + 1;
}

}  // namespace gardener
