#include "app/SerialLink.h"

#include <Arduino.h>

#include "app/Items.h"
#include "app/SerialSurface.h"
#include "core/Config.h"
#include "core/Snapshot.h"

namespace gardener {
namespace {

// A marker is stored decoded and printed escaped, so that `link get` and the echo of a write both
// answer with a token a host could send back. decodeTerminator() in SerialLink.cpp is the other
// half of this, and the two have to stay inside each other.
void encodeMarker(const String& marker, String& out) {
  out = "";
  for (size_t i = 0; i < marker.length(); i++) {
    switch (marker.charAt(i)) {
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\\': out += "\\\\"; break;
      default: out += marker.charAt(i); break;
    }
  }
}

template <typename T, size_t N>
constexpr uint8_t countOf(const T (&)[N]) {
  return (uint8_t)N;
}

// --- the field readers -----------------------------------------------------
//
// One per field, and the only place a field's value is formatted: `get`, a single-field read, a
// write's echo and the help page all reach a field through the tables below, so none of them can
// disagree about what `kpa` is or about which fields a resource has.

void readFirmware(const WireView&, Items& out) { out.add("fw", cfg::FIRMWARE_VERSION); }

void readProtocol(const WireView&, Items& out) { out.addU32("proto", cfg::PROTOCOL_VERSION); }

void readChip(const WireView&, Items& out) { out.add("chip", ESP.getChipModel()); }

void readHeap(const WireView&, Items& out) { out.addU32("heap", (uint32_t)ESP.getFreeHeap()); }

void readBatteryVolts(const WireView& view, Items& out) {
  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  if (!view.snapshot.batteryPresent) {
    out.add("batt_v", nullptr);
    return;
  }
  out.addFloat("batt_v", view.snapshot.batteryVolts, 2);
}

void readBatteryPercent(const WireView& view, Items& out) {
  if (!view.snapshot.batteryPresent) {
    out.add("batt_pct", nullptr);
    return;
  }
  out.addU32("batt_pct", view.snapshot.batteryPercent);
}

void readLog(const WireView& view, Items& out) { out.addFlag("log", view.logEnabled); }

void readAmbient(const WireView& view, Items& out) {
  out.addFloat("ambient", view.plant.ambientKpa, 2);
}

void readPressure(const WireView& view, Items& out) {
  out.addFloat("kpa", view.plant.kpa, 2);
}

void readVacuum(const WireView& view, Items& out) {
  out.addFloat("vac", view.plant.vacuumKpa, 2);
}

void readVacuumPercent(const WireView& view, Items& out) {
  out.addFloat("pct", view.plant.vacuumPercent, 1);
}

void readTarget(const WireView& view, Items& out) {
  // 0 is how the plant holds "no target", and the protocol writes that `-`: a target of nothing is
  // not a target, and there is no 0 kPa one for it to be confused with.
  if (view.plant.targetKpa <= 0.0f) {
    out.add("target", nullptr);
    return;
  }
  out.addFloat("target", view.plant.targetKpa, 1);
}

void readFloor(const WireView&, Items& out) { out.addFloat("floor", cfg::CONTROL_FLOOR_KPA, 1); }

void readDuty(const WireView& view, Items& out) {
  out.addFloat("duty", view.plant.duty, 1);
}

void readRunState(const WireView& view, Items& out) {
  out.add("state", runStateName(view.plant.runState));
}

void readBattery(const WireView& view, Items& out) {
  if (!view.snapshot.batteryPresent) {
    out.add("batt", nullptr);
    return;
  }
  out.addU32("batt", view.snapshot.batteryPercent);
}

void readSensor(const WireView& view, Items& out) {
  out.add("sensor", view.plant.sensorOk ? "1" : "0");
}

void readFault(const WireView& view, Items& out) {
  out.add("fault", view.plant.faultCode[0] ? view.plant.faultCode : "none");
}

void readUptime(const WireView& view, Items& out) { out.addU32("up", view.snapshot.uptimeMs); }

void readRotation(const WireView& view, Items& out) {
  out.addU32("rotation", view.display.rotation());
}

void readInvert(const WireView& view, Items& out) {
  out.addFlag("invert", view.display.inverted());
}

void readWidth(const WireView& view, Items& out) {
  out.addU32("w", (uint32_t)view.display.width());
}

void readHeight(const WireView& view, Items& out) {
  out.addU32("h", (uint32_t)view.display.height());
}

void readTerm(const WireView& view, Items& out) {
  if (!view.customTerminator) {
    out.add("term", "off");
    return;
  }
  String marker;
  encodeMarker(view.terminator, marker);
  out.add("term", marker.c_str());
}

void readAttached(const WireView& view, Items& out) {
  // `-` when the transport cannot tell: a plain UART cannot report a host, and a controller has to
  // be able to tell that apart from nobody being there.
  if (!view.hostKnown) {
    out.add("attached", nullptr);
    return;
  }
  out.add("attached", view.hostAttached ? "1" : "0");
}

}  // namespace

// --- the surface, as data --------------------------------------------------

const ResourceSpec* SerialLink::resources(uint8_t& count) {
  // The whole wire surface in one place: what each resource is called, which fields it has, how
  // each one is read, whether a host may write it, and which actions it answers. It is built
  // inside a member function because it names the action and writer members below; everything else
  // about the wire is derived from it.
  static const FieldSpec kDeviceFields[] = {
      {"proto", false, "int", nullptr, nullptr, nullptr, nullptr, readProtocol},
      {"fw", false, "text", nullptr, nullptr, nullptr, nullptr, readFirmware},
      {"chip", false, "text", nullptr, nullptr, nullptr, nullptr, readChip},
      {"heap", false, "int", "bytes", nullptr, nullptr, nullptr, readHeap},
      {"batt_v", false, "float", "V", nullptr, nullptr, nullptr, readBatteryVolts},
      {"batt_pct", false, "int", "%", nullptr, nullptr, nullptr, readBatteryPercent},
      {"log", true, "bool", nullptr, "on,off", nullptr, nullptr, readLog},
  };
  static const FieldSpec kPlantFields[] = {
      {"ambient", false, "float", "kPa", nullptr, nullptr, nullptr, readAmbient},
      {"kpa", false, "float", "kPa", nullptr, nullptr, nullptr, readPressure},
      {"vac", false, "float", "kPa", nullptr, nullptr, nullptr, readVacuum},
      {"pct", false, "float", "%", nullptr, nullptr, nullptr, readVacuumPercent},
      // Only the shallow bound is a rule. A target deeper than the pump can reach is still a
      // target: the duty pins at the cap and the run simply never arrives, which is why `floor` is
      // reported rather than enforced.
      {"target", true, "float", "kPa", nullptr, &cfg::CONTROL_MIN_TARGET_KPA, nullptr, readTarget},
      {"floor", false, "float", "kPa", nullptr, nullptr, nullptr, readFloor},
      {"duty", false, "float", "%", nullptr, nullptr, nullptr, readDuty},
      {"state", false, "choice", nullptr, "IDLE,PULL,HOLD,VENT,FAULT", nullptr, nullptr,
       readRunState},
      {"batt", false, "int", "%", nullptr, nullptr, nullptr, readBattery},
      {"sensor", false, "bool", nullptr, nullptr, nullptr, nullptr, readSensor},
      {"fault", false, "text", nullptr, nullptr, nullptr, nullptr, readFault},
      {"up", false, "int", "ms", nullptr, nullptr, nullptr, readUptime},
  };
  static const FieldSpec kPanelFields[] = {
      {"rotation", true, "int", nullptr, nullptr, &ROTATION_MIN, &ROTATION_MAX, readRotation},
      {"invert", true, "bool", nullptr, "on,off", nullptr, nullptr, readInvert},
      {"w", false, "int", "px", nullptr, nullptr, nullptr, readWidth},
      {"h", false, "int", "px", nullptr, nullptr, nullptr, readHeight},
  };
  static const FieldSpec kLinkFields[] = {
      {"term", true, "text", nullptr, nullptr, nullptr, nullptr, readTerm},
      {"attached", false, "bool", nullptr, nullptr, nullptr, nullptr, readAttached},
  };

  static const ActionSpec kDeviceActions[] = {
      {"reboot", false, &SerialLink::deviceReboot},
  };
  static const ActionSpec kPlantActions[] = {
      {"start", false, &SerialLink::plantStart},
      {"stop", false, &SerialLink::plantStop},
      {"reset", false, &SerialLink::plantReset},
      {"capture", false, &SerialLink::plantCapture},
  };
  static const ActionSpec kPanelActions[] = {
      // The one action whose arguments are not a fixed list: the rest of the line is an op and its
      // coordinates, and `help panel draw` is what lists the ops.
      {"draw", true, &SerialLink::panelDraw},
      {"flush", false, &SerialLink::panelFlush},
  };

  // In the order `help` lists them.
  static const ResourceSpec kResources[] = {
      {"plant", kPlantFields, countOf(kPlantFields), kPlantActions, countOf(kPlantActions),
       &SerialLink::writePlant},
      {"panel", kPanelFields, countOf(kPanelFields), kPanelActions, countOf(kPanelActions),
       &SerialLink::writePanel},
      {"link", kLinkFields, countOf(kLinkFields), nullptr, 0, &SerialLink::writeLink},
      {"device", kDeviceFields, countOf(kDeviceFields), kDeviceActions, countOf(kDeviceActions),
       &SerialLink::writeDevice},
  };

  count = countOf(kResources);
  return kResources;
}

// A field of a resource, by the name a host typed. Null means the device does not know the word,
// which the caller reports as an unknown command rather than a bad argument.
const FieldSpec* SerialLink::findField(const ResourceSpec& resource, const char* name) {
  for (uint8_t i = 0; i < resource.fieldCount; i++) {
    if (same(resource.fields[i].name, name)) return &resource.fields[i];
  }
  return nullptr;
}

// Every field, in table order, which is also the order the help page prints them in.
void SerialLink::renderFields(const ResourceSpec& resource, Items& out) const {
  WireView view = wireView();
  for (uint8_t i = 0; i < resource.fieldCount; i++) resource.fields[i].read(view, out);
}

}  // namespace gardener