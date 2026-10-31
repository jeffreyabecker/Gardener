#include "beats/MachineTick.h"

#include "core/Logger.h"
#include "core/SystemState.h"

namespace gardener {

MachineTick::MachineTick(VacuumSystem& vacuum, Logger& logger, CommandQueue& queue,
                         SystemState& state)
    : vacuum_(vacuum), logger_(logger), queue_(queue), state_(state),
      lastState_(vacuum.runState()) {}

void MachineTick::tick(uint32_t nowMs) {
  // Commands first, against the machine as the last tick left it.
  drainCommands();

  // The devices and the PID on the fixed beat, then the run log.
  vacuum_.sensor().update(nowMs);
  vacuum_.pump().update(nowMs);
  vacuum_.service(nowMs);
  logRunChanges(nowMs);

  publish();
}

// The machine's slice, read once at the end of the beat: the run the panel draws and the answer a
// page's start gets are the ones this beat produced. A page that asks between two beats is answered
// from here, so the machine can still refuse the command it posts.
void MachineTick::publish() {
  state_.runState = vacuum_.runState();
  state_.running = vacuum_.running();
  state_.currentPressureKpa = vacuum_.currentPressureKpa();
  state_.targetPressureKpa = vacuum_.targetPressureKpa();
  state_.canStart = vacuum_.startRefusal() == nullptr;
}

bool MachineTick::takeReboot() {
  const bool pending = rebootPending_;
  rebootPending_ = false;
  return pending;
}

void MachineTick::apply(const MachineCommand& command) {
  if (!applyMachineCommand(vacuum_, command)) rebootPending_ = true;
}

void MachineTick::logCollapsed(const MachineCommand& command, const char* reason) {
  String message = "queue: dropped ";
  message += machineCommandName(command.op);
  if (command.op == MachineCommand::Op::SetTarget) {
    message += ' ';
    message += String(command.value, 1u);
  }
  message += ' ';
  message += reason;
  logger_.warn(message.c_str());
}

void MachineTick::drainCommands() {
  MachineCommand batch[CommandQueueDepth];
  uint8_t count = 0;
  while (count < CommandQueueDepth && queue_.take(batch[count])) {
    count++;
  }
  if (count == 0) return;

  // The last stop owns everything before it.
  uint8_t first = 0;
  for (uint8_t i = count; i > 0; i--) {
    if (batch[i - 1].op == MachineCommand::Op::Stop) {
      first = i - 1;
      break;
    }
  }
  for (uint8_t i = 0; i < first; i++) logCollapsed(batch[i], "before a stop");

  // Of what is left, only the last target is worth applying: the depths before it are setpoints
  // the machine never has to stand still at.
  uint8_t lastTarget = count;
  for (uint8_t i = count; i > first; i--) {
    if (batch[i - 1].op == MachineCommand::Op::SetTarget) {
      lastTarget = i - 1;
      break;
    }
  }
  for (uint8_t i = first; i < count; i++) {
    if (i != lastTarget && batch[i].op == MachineCommand::Op::SetTarget) {
      logCollapsed(batch[i], "for a later target");
      continue;
    }
    apply(batch[i]);
  }
}

void MachineTick::logRunChanges(uint32_t nowMs) {
  if (vacuum_.takeDeadlineExpired()) {
    String deadline = "run: hold deadline reached after ";
    deadline += String(vacuum_.holdSeconds());
    deadline += " s";
    logger_.warn(deadline.c_str());
  }

  const RunState current = vacuum_.runState();
  if (current == lastState_) return;

  String message = runStateName(lastState_);
  message += " -> ";
  message += runStateName(current);
  if (current == RunState::Fault) {
    message += " (fault ";
    message += vacuum_.faultCode();
    message += ')';
  }
  if (lastState_ == RunState::Venting) {
    message += " after ";
    message += String(nowMs - ventStartMs_);
    message += " ms";
  }
  if (current == RunState::Venting) ventStartMs_ = nowMs;
  logger_.info(message.c_str());
  lastState_ = current;
}

}  // namespace gardener
