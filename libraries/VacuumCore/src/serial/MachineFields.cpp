#include "serial/MachineFields.h"

#include "Machine.h"
#include "MachineCommand.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

// A second token on the line: `set` is all or nothing, so it is refused before anything is
// checked or applied.
bool refuseExtra(Stream& io, const String& extra) {
  if (extra.isEmpty()) return false;
  io.print("err bad_argument ");
  io.print(firstToken(extra));
  io.print('\n');
  return true;
}

// The bound a setting holds, refused in the same shape wherever it is written. The value is the
// token as typed, so a client sees what it sent rather than what the parser made of it.
bool inRange(Stream& io, const char* field, const String& token, float value, float min, float max,
             unsigned int decimals) {
  if (value >= min && value <= max) return true;
  io.print("err out_of_range field=");
  io.print(field);
  io.print(" value=");
  io.print(token);
  if (value < min) {
    io.print(" min=");
    io.print(min, decimals);
  } else {
    io.print(" max=");
    io.print(max, decimals);
  }
  io.print('\n');
  return false;
}

// One float field: the extra-token check, the bound, and the command that applies it on the
// control task. The reply echoes what was asked for; the read-back stays the authority.
bool setFloatField(Stream& io, CommandOutbox& outbox, MachineCommand::Op op, const char* name,
                   const String& token, const String& extra, float min, float max,
                   unsigned int decimals) {
  if (refuseExtra(io, extra)) return true;
  const float value = token.toFloat();
  if (!inRange(io, name, token, value, min, max, decimals)) return true;
  outbox.post(MachineCommand{op, value});

  io.print("ok ");
  io.print(name);
  io.print('=');
  io.print(value, decimals);
  io.print('\n');
  return true;
}

}  // namespace

bool setMachineField(Stream& io, Machine& app, CommandOutbox& outbox, const String& field,
                     const String& value, const String& extra) {
  (void)app;

  if (field.equalsIgnoreCase("flow")) {
    return setFloatField(io, outbox, MachineCommand::Op::SetFlow, "flow", value, extra,
                         PUMP_FLOW_MIN_LPM, PUMP_FLOW_MAX_LPM, 2u);
  }

  if (field.equalsIgnoreCase("volume")) {
    return setFloatField(io, outbox, MachineCommand::Op::SetVolume, "volume", value, extra,
                         CHAMBER_VOLUME_MIN_L, CHAMBER_VOLUME_MAX_L, 3u);
  }

  if (field.equalsIgnoreCase("floor")) {
    return setFloatField(io, outbox, MachineCommand::Op::SetFloor, "floor", value, extra,
                         CONTROL_MIN_TARGET_KPA, CONTROL_FLOOR_MAX_KPA, 1u);
  }

  if (field.equalsIgnoreCase("pump_max")) {
    if (refuseExtra(io, extra)) return true;
    const float percent = value.toFloat();
    // The floor is exclusive: a duty cap of zero is not a cap, it is a stopped pump.
    if (!(percent > 0.0f)) {
      io.print("err out_of_range field=pump_max value=");
      io.print(value);
      io.print(" min=0.0\n");
      return true;
    }
    if (!inRange(io, "pump_max", value, percent, 0.0f, PUMP_DUTY_MAX_PERCENT, 1u)) return true;
    outbox.post(MachineCommand{MachineCommand::Op::SetPumpMax, percent});

    io.print("ok pump_max=");
    io.print(percent, 1u);
    io.print('\n');
    return true;
  }

  if (field.equalsIgnoreCase("deadband")) {
    return setFloatField(io, outbox, MachineCommand::Op::SetDeadband, "deadband", value, extra,
                         CONTROL_DEADBAND_MIN_KPA, CONTROL_DEADBAND_MAX_KPA, 2u);
  }

  if (field.equalsIgnoreCase("valve")) {
    if (refuseExtra(io, extra)) return true;
    bool normallyOpen = false;
    if (value.equalsIgnoreCase("open")) {
      normallyOpen = true;
    } else if (value.equalsIgnoreCase("closed")) {
      normallyOpen = false;
    } else {
      io.print("err bad_argument ");
      io.print(value);
      io.print('\n');
      return true;
    }
    // The command carries the direction, and the fitted part only changes on the next boot, so the
    // reply echoes what was asked for rather than what is fitted now.
    outbox.post(MachineCommand{MachineCommand::Op::SetValve, normallyOpen ? 1.0f : 0.0f});

    io.print("ok valve=");
    io.print(normallyOpen ? "open" : "closed");
    io.print('\n');
    return true;
  }

  return false;
}

}  // namespace gardener
