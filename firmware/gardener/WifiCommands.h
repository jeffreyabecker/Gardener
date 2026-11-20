#pragma once

#include <Arduino.h>

#include "WifiControl.h"
#include "serial/CommandPlugin.h"

namespace gardener::runtime {

// The network's lines on the serial wire, under the wifi prefix: the status, the scan that lists
// what is out there, the stored choice a field at a time, the join that applies it, and the forget
// that clears it. Every decision behind them is WifiControl's; this plugin only parses and formats.
//
// It is a header because the host suite compiles the sketch's platform-free headers and not its
// sources, and the quoting rules earn their cases: a network name with a space or a quote in it has
// to survive the reply and the next command.
class WifiCommands : public CommandPlugin {
 public:
  explicit WifiCommands(WifiControl& wifi) : wifi_(wifi) {}

  const char* prefix() const override { return "wifi"; }
  void serve(Print& io, const String& args) override;

 private:
  // How many networks one scan puts on a line.
  static constexpr int ScanListMax = 16;

  // Writes a value bare while it reads back as one token, and quoted when it does not, with the two
  // characters that would end or escape the quote spelled out.
  static bool needsQuotes(const String& value);
  static String quoted(const String& value);
  static String unquoted(const String& text);
  static void badArgument(Print& io, const String& token);
  static void writeStatus(Print& io, const char* prefix, const WifiStatus& status);

  WifiControl& wifi_;
};

inline bool WifiCommands::needsQuotes(const String& value) {
  if (value.isEmpty()) return true;
  for (unsigned int i = 0; i < value.length(); i++) {
    const char character = value[i];
    if (character == ' ' || character == '"' || character == '\\') return true;
  }
  return false;
}

inline String WifiCommands::quoted(const String& value) {
  if (!needsQuotes(value)) return value;

  String out = "\"";
  for (unsigned int i = 0; i < value.length(); i++) {
    const char character = value[i];
    if (character == '"' || character == '\\') out += '\\';
    out += character;
  }
  out += '"';
  return out;
}

// A value the way the wire takes one: a surrounding pair of quotes is stripped and its escapes read,
// so `"My Net"` and the rest of a line that is `My Net` are the same name. Text after a closed pair
// is not part of it, and an unclosed quote is just a character in the name.
inline String WifiCommands::unquoted(const String& text) {
  String value = text;
  value.trim();
  if (value.length() < 2 || value[0] != '"') return value;

  String out;
  for (unsigned int i = 1; i < value.length(); i++) {
    const char character = value[i];
    if (character == '\\' && i + 1 < value.length()) {
      out += value[++i];
      continue;
    }
    if (character == '"') return out;
    out += character;
  }
  return value;
}

// The rest of the line is refused the way every other word under the prefix refuses an argument it
// did not expect.
inline void WifiCommands::badArgument(Print& io, const String& token) {
  const int space = token.indexOf(' ');
  String name = space < 0 ? token : token.substring(0, (unsigned int)space);
  io.print("err bad_argument ");
  io.print(name);
  io.print('\n');
}

inline void WifiCommands::writeStatus(Print& io, const char* prefix, const WifiStatus& status) {
  String line = prefix;
  line += "ssid=";
  line += quoted(status.ssid);
  line += " mode=";
  line += status.mode;
  line += " ip=";
  line += status.address;
  line += " keep_ap=";
  line += status.keepPortal ? "1" : "0";
  line += '\n';
  io.print(line);
}

inline void WifiCommands::serve(Print& io, const String& args) {
  const int space = args.indexOf(' ');
  String word = space < 0 ? args : args.substring(0, (unsigned int)space);
  String rest = space < 0 ? String() : args.substring((unsigned int)space + 1);
  rest.trim();
  word.toLowerCase();

  if (word.isEmpty()) {
    writeStatus(io, "ok ", wifi_.status());
    return;
  }

  if (word == "scan") {
    if (!rest.isEmpty()) {
      badArgument(io, rest);
      return;
    }
    WifiNetwork networks[ScanListMax];
    const int count = wifi_.scan(networks, ScanListMax);
    // One line per network, because the receipt is a line the CDC can carry in one write: a scan of
    // a street is several hundred bytes, and one write of that loses a chunk of its middle.
    String head = "ok count=";
    head += String(count);
    head += '\n';
    io.print(head);
    for (int i = 0; i < count; i++) {
      String line = "ssid";
      line += String(i);
      line += '=';
      line += quoted(networks[i].ssid);
      line += " rssi";
      line += String(i);
      line += '=';
      line += String((long)networks[i].rssi);
      line += '\n';
      io.print(line);
      // The ring drains slower than this loop writes, so each line waits for the one before it: a
      // scan of a street otherwise fills the ring and loses a chunk of its own reply.
      io.flush();
    }
    return;
  }

  if (word == "ssid") {
    if (rest.isEmpty()) {
      badArgument(io, "ssid");
      return;
    }
    const String name = unquoted(rest);
    wifi_.setSsid(name);
    io.print("ok ssid=");
    io.print(quoted(name));
    io.print('\n');
    return;
  }

  if (word == "pass") {
    // The password is stored and never repeated: a scan of the wire's own history is not a place to
    // read it back from.
    wifi_.setPass(unquoted(rest));
    io.print("ok\n");
    return;
  }

  if (word == "keep") {
    String flag = rest;
    flag.toLowerCase();
    if (flag == "on") {
      wifi_.setKeepPortal(true);
      io.print("ok keep_ap=1\n");
      return;
    }
    if (flag == "off") {
      wifi_.setKeepPortal(false);
      io.print("ok keep_ap=0\n");
      return;
    }
    badArgument(io, rest);
    return;
  }

  if (word == "join") {
    if (!rest.isEmpty()) {
      badArgument(io, rest);
      return;
    }
    if (wifi_.status().ssid.isEmpty()) {
      io.print("err no_network\n");
      return;
    }
    wifi_.join();
    writeStatus(io, "ok ", wifi_.status());
    return;
  }

  if (word == "forget") {
    if (!rest.isEmpty()) {
      badArgument(io, rest);
      return;
    }
    wifi_.forget();
    writeStatus(io, "ok ", wifi_.status());
    return;
  }

  badArgument(io, word);
}

}  // namespace gardener::runtime
