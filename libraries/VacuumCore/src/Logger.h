#pragma once

#include <Arduino.h>
#include <ctype.h>

namespace gardener {

// What every consumer holds: the three diagnostic levels and the set of them the links choose. There
// is one sink — SerialLogger, framed records on the serial line — and it writes every level from
// boot, so no consumer picks a sink and none checks a level of its own.
class Logger {
 public:
  virtual ~Logger() = default;

  // The levels this sink writes, in canonical spelling: some of I, W and E in that order, or `-` for
  // none.
  virtual String levels() const = 0;
  virtual void setLevels(const String& levels) = 0;

  virtual void info(const char* text) = 0;
  virtual void warn(const char* text) = 0;
  virtual void error(const char* text) = 0;
};

// The wire's spelling of a level set: some of I, W and E in any case and order, or `-` for none.
// `levels` is the canonical form; false when the text names anything else, which leaves it alone.
inline bool parseLevels(const String& text, String& levels) {
  if (text == "-") {
    levels = "-";
    return true;
  }

  bool info = false;
  bool warn = false;
  bool error = false;
  for (unsigned int i = 0; i < text.length(); ++i) {
    switch (toupper((unsigned char)text[i])) {
      case 'I': info = true; break;
      case 'W': warn = true; break;
      case 'E': error = true; break;
      default: return false;
    }
  }
  if (!info && !warn && !error) return false;

  levels = "";
  if (info) levels += 'I';
  if (warn) levels += 'W';
  if (error) levels += 'E';
  return true;
}

}  // namespace gardener
