#include "app/LineGrammar.h"

#include <string.h>

namespace gardener {

bool startsWith(const char* line, const char* word) {
  for (; *word != '\0'; word++, line++) {
    char c = *line;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c != *word) return false;
  }
  return *line == '\0' || *line == ' ' || *line == '\t';
}

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

bool parseCoord(const char* text, int16_t& out) {
  float value = 0.0f;
  if (!parseFloat(text, value)) return false;
  if (value < -32768.0f || value > 32767.0f) return false;
  out = (int16_t)value;
  return true;
}

}  // namespace gardener
