#pragma once

#include <Arduino.h>

#include "WifiControl.h"
#include "core/Formatting.h"
#include "serial/CommandPlugin.h"

namespace gardener::runtime {

// The network's lines on the serial wire, under the wifi prefix: the status, the scan that lists
// what is out there, the stored choice a field at a time, the join that applies it, and the forget
// that clears it. Every decision behind them is WifiControl's; this plugin only parses and formats.
//
// It is a header because the host suite compiles the sketch's platform-free headers and not its
// sources, and the parsing rules earn their cases: a network name with a space or a quote in it has
// to survive the reply and the next command.
class WifiCommands : public CommandPlugin {
 public:
  explicit WifiCommands(WifiControl& wifi) : wifi_(wifi) {}

  const char* prefix() const override { return "wifi"; }
  void serve(Print& io, const String& args) override;

 private:
  // How many networks one scan puts on a line.
  static constexpr int ScanListMax = 16;

  static String unquoted(const String& text);
  static void badArgument(Print& io, const String& token);
  static void writeStatus(Print& io, const char* prefix, const WifiStatus& status);

  WifiControl& wifi_;
};

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
  println(io, "err bad_argument ", name);
}

inline void WifiCommands::writeStatus(Print& io, const char* prefix, const WifiStatus& status) {
  println(io, prefix, "ssid=", quote(status.ssid), " mode=", status.mode, " ip=", status.address,
          " keep_ap=", status.keepPortal ? 1 : 0);
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
    println(io, "ok count=", count);
    for (int i = 0; i < count; i++) {
      println(io, "ssid", i, '=', quote(networks[i].ssid), " rssi", i, '=', networks[i].rssi);
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
    println(io, "ok ssid=", quote(name));
    return;
  }

  if (word == "pass") {
    // The password is stored and never repeated: a scan of the wire's own history is not a place to
    // read it back from.
    wifi_.setPass(unquoted(rest));
    println(io, "ok");
    return;
  }

  if (word == "keep") {
    String flag = rest;
    flag.toLowerCase();
    if (flag == "on") {
      wifi_.setKeepPortal(true);
      println(io, "ok keep_ap=1");
      return;
    }
    if (flag == "off") {
      wifi_.setKeepPortal(false);
      println(io, "ok keep_ap=0");
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
      println(io, "err no_network");
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
