# Reentrancy and resumption

Status: investigation, not a spec. This is [the plan](berry-on-device-control.md) §5.3 read against
the VM's own source: [spec 01](specs/01-execution-model.md) asks whether a runaway program can
stretch the tick, and this asks what happens to the VM when the service pass runs *inside* it, and
what it would take for a program to suspend and resume at all.

It settles nothing new on purpose. It records what the vendored VM was found to guarantee — because
[spec 01](specs/01-execution-model.md) §6, [spec 03](specs/03-berry-runtime-and-api.md) §3 and
[spec 05](specs/05-disconnected-operation.md) §5 all rest on it — and it names the axis the specs
leave unpinned. Where a program shape is named below it is named with its price, not chosen: there
are no acceptance criteria here because nothing is being accepted, and §7 is the invariants a later
decision has to keep rather than criteria for one.

## 1. What the VM guarantees

Read against upstream Berry (`berry-lang/berry`: `src/be_vm.c`, `src/be_vm.h`, `src/be_exec.c`,
`src/be_debug.c`, `default/berry_conf.h`). Every claim here is a property of the port as vendored,
not of the language.

### 1.1 The hook

- The hook exists, and it is the **observability** hook: `be_set_obs_hook(vm, hook)`, called back
  with `BE_OBS_VM_HEARTBEAT`. [Spec 03](specs/03-berry-runtime-and-api.md) §2's reading — an
  execution-limit hook with a per-call instruction budget — is close but is not the mechanism;
  there is no per-call budget, only a sampling mask over a global counter.
- It fires **between Berry instructions and nowhere else**. The dispatch loop is
  `loop: DEBUG_HOOK(); COUNTER_HOOK(); VM_HEARTBEAT(); switch (IGET_OP(ins = *vm->ip++))`, so the
  hook runs at an instruction boundary and never while a native function is executing. A native
  call that takes time — `vacuum.delay` spinning, `screen.flush()` over I²C, a sensor read — is
  invisible to it. That is why [spec 03](specs/03-berry-runtime-and-api.md) §4's `delay()` has to call
  the service hook itself.
- The interval is a build constant and a power of two: `BE_VM_OBSERVABILITY_SAMPLING`, default
  `20`, tested as `(counter_ins & ((1 << (SAMPLING - 1)) - 1)) == 0` — about one instruction in
  524288, not the "million" the port's own comments claim. It also requires
  `BE_USE_PERF_COUNTERS=1`, which is what puts a per-instruction counter increment in the loop.
  So `cfg::VM_HOOK_INSTRUCTIONS`, the constant [spec 01](specs/01-execution-model.md) §5 plans to size on
  the bench, cannot be the constant that section describes: the value is an exponent in Berry's own
  `berry_conf.h`, the reachable intervals are powers of two, and it is an instruction count and
  never a deadline.
- The hook is not only the heartbeat. The same callback receives `BE_OBS_GC_START`,
  `BE_OBS_GC_END`, `BE_OBS_MALLOC_FAIL`, `BE_OBS_PCALL_ERROR` and `BE_OBS_STACK_RESIZE_START` —
  the last fired from inside `be_stack_expansion()`, that is, from inside a nested call that is
  allocating. A handler that runs the service pass has to filter the event and has to be prepared
  to be called during allocation.
- Unlike Berry's own debug hook — `be_callhook()` clears `vm->hookmask` while the hook runs so a
  nested call cannot re-enter it — the observability hook has no recursion guard.

### 1.2 Calling into Berry from the hook

Berry supports it: its own debugger does exactly this (`be_callhook` → `be_call` → `be_dofunc`,
after `be_stack_require(vm, BE_STACK_FREE_MIN + 2)` reserves the hook's own slots). That is what
makes [spec 03](specs/03-berry-runtime-and-api.md) §5's "drain the queue into the matching callbacks"
look reasonable. Two details decide whether it is safe *here*:

- **Stack reallocation is invisible to C locals.** Every call reserves `nstack +
  BE_STACK_FREE_MIN`; when that does not fit, `be_stack_expansion()` → `stack_resize()`
  reallocates `vm->stack` and fixes up every `bcallframe`, every open upvalue, `vm->reg` and
  `vm->top`. It cannot fix up a pointer held in a C local, and the execute loop holds one: `reg`,
  the base register every `RA()`/`RKB()`/`RKC()` is computed from. `DEBUG_HOOK()` therefore
  re-syncs `reg = vm->reg;` after running the hook. `VM_HEARTBEAT()` does not. A nested Berry call
  from the heartbeat that grows the stack leaves the outer program's `reg` pointing into freed
  memory.
- **The nesting is not what the specs think it is.** They treat "the service pass runs inside a
  Berry call" as a property of the design. It is a property of this port, at this sampling setting,
  for programs whose stack does not grow at the hook. None of the three is guaranteed by anything
  in the API the specs use.

### 1.3 There is no suspension

`src/` carries no coroutine object type and neither `yield` nor `generator` appears anywhere in it,
so there is no language-level way to leave a call and come back to it. The only way out of a call
that does not return is an exception: `be_raise()` longjmps to the nearest `be_pcall` boundary
(`be_protectedcall`, which saves VM state around the frame and restores it on the way out).
[Spec 01](specs/01-execution-model.md) §5 is right that a hook "calls in and returns"; the reason is
stronger than the plan states — there is nothing else it could do.

## 2. Where the two questions arise

**Reentrancy.** Four places, all the same shape — the VM is entered while the VM is on the stack:

| Where | What re-enters | Named in |
| --- | --- | --- |
| Button events | the program's own `buttons.on` callbacks | [spec 03](specs/03-berry-runtime-and-api.md) §5, [spec 04](specs/04-screen-and-buttons.md) §2 |
| `programs.run` / `home` / `stop` | a new program, from inside the running one | [spec 03](specs/03-berry-runtime-and-api.md) §6.5 |
| Serial commands | `store run`, an upload, `vm mode` | [spec 05](specs/05-disconnected-operation.md) §5 |
| The service pass itself | the plant tick, the link poll, the button scan | [spec 01](specs/01-execution-model.md) §5 |

The bottom three are handled by deferral — a request is parked and applied at the yield point — but
the first is not deferral, it is an upcall by design, and it is the one that performs the nested
`be_call`. Nothing in `ProgramApi`'s shape ([spec 03](specs/03-berry-runtime-and-api.md) §4) keeps the
next verb from being one: deferral is a convention held by reading, not a mechanism held by the
type.

**Resumption.** Nothing suspends. A program that waits polls (`while true … vacuum.delay(100)`), so
a Berry program is already a state machine whose state lives in globals that outlive each statement
— [default.be](../firmware/progs/default.be) is written that way. "Resumption" in this design
means re-entering the program, never continuing it.

## 3. The axis the specs leave unpinned

[Spec 03](specs/03-berry-runtime-and-api.md) §3 describes `start()` / `run()` / `stop()` — one call that
lives as long as the program does. `default.be` assumes callbacks dispatched into it from outside.
Those are two different program shapes, and the board's second core makes a third available:

| | Long-lived call | Re-entered state | Task-resident call |
| --- | --- | --- | --- |
| A program is | one call that yields from inside | entry points the dispatcher calls | one call that blocks and is suspended by the scheduler |
| Reentrancy arises | at every upcall, from the VM's own frame | never, if only the dispatcher enters | at every upcall made from inside a blocking yield |
| Resumption | none ([spec 01](specs/01-execution-model.md) §5) | not needed: the next entry re-enters | real: the C stack and the Berry stack survive |
| A stop is | a flag the hook reads, then an unwind | a flag the dispatcher reads between entries | a flag the wait reads, then an unwind |
| The tick is held by | the hook interval | the program returning each beat | task priority |
| Costs | §1.2, and the interval is a power of two | programs that never block; state in globals | §5 |

The third column is the plan's own fallback ([spec 01](specs/01-execution-model.md) §6), and it is named
here to be priced, not proposed: it is what a measurement would ask for, not an alternative to
taking one. The second column is the only one of the three that needs no new context at all — it is
a change to what a program *is*, and it is where §2's upcall stops being reentrant by construction.
§4 is that change written down as a requirement, and what it would delete.

## 4. What enforcement could remove

§2's four rows and §1.2's hazard have one cause: a Berry call made *from inside a VM frame*. Every
row of that table is a call the runtime makes while the VM is already on the stack. Make the runtime
the only thing that enters the VM, and enter it once per beat, and the table has nothing left to
describe. The question is not "event-driven or callbacks"; it is "who enters".

### 4.1 One entry, one beat

**The rule.** The runtime calls Berry only from its own frame: it compiles a program, instantiates
it, and from then on every `tick`, `event` and `stop` is a separate `be_pcall` that runs to
completion or to an exception. Between entries the VM's call stack is empty.

That is what makes it safe rather than disciplined. A `be_pcall` from C is an *entry*, not an
upcall: `be_protectedcall` saves VM state around the frame and restores it on the way out, so an
exception — including a deliberate one — unwinds to the runtime and never into a half-finished
program. The one habit the application side has to keep is §1.2's, read from the other end: it may
hold *indices* and absolute positions in the VM's stack, never a cached `bvalue*`, because the stack
relocates under any call. The instance is held as a global (`be_setglobal`), not as an index, so the
collector keeps it alive across beats.

This also corrects §2. The danger was never that the runtime calls into Berry — Berry's own debugger
does that. It is calling in *from inside a frame*.

### 4.2 What a required interface makes enforceable

Berry's own API is enough for all of it, and none of it needs a fork:

| What the runtime does | Berry's API | Note |
| --- | --- | --- |
| Gets the program's class | the file's top level ends `return TheClass`: `be_loadbuffer` compiles the source into a function and `be_pcall(vm, 0)` returns its value | `be_repl.c`'s `try_return` is this same mechanism — it wraps a line as `return (…)` and reads the result |
| Instantiates it | calling the class *is* instantiation: `be_dofunc` dispatches `BE_CLASS` to `do_class` → `be_class_newobj` plus the constructor | a class with no `init` still instantiates, through `be_default_init_native_function` |
| Checks the interface | `be_getmethod(instance, name)` | presence, not signature |
| Runs a beat | `be_getmember(inst, name)`, push `self`, push the arguments, `be_pcall` | one entry, and it is bounded |

Presence is the right check because it is all Berry's calling convention gives you: a missing
parameter arrives as `nil` and an extra argument is ignored, so `def tick(self)` and
`def tick(self, now)` are both callable with the same call. Nothing can be checked about types, and
nothing needs to be. The check runs at load — compile, run the top level under the beat budget,
probe each required name, report the first missing one by name — which is the difference between
`store put` refusing a program at the desk and the device finding out with nobody watching.

### 4.3 The beat's return value is the schedule

```berry
class Readout
  def tick(now)
    draw()
    return now + 100          # what vacuum.delay(100) means now
  end
  def event(kind, arg) ... end
end
return Readout
```

- It is a mechanical translation of the plan's shape. `while true … vacuum.delay(100)` becomes a
  `tick` that returns a deadline; nothing is suspended and nothing has to be, which is all §1.3
  allows.
- An idle program costs nothing: no beat until its deadline or an event arrives, which today's poll
  loop cannot express — a 10 Hz poll is ten entries a second forever.
- It answers the event-driven question without enforcing it. An event-only program is a program that
  returns a far deadline. Same interface, same dispatcher, and "must they be event-driven" stops
  being a design question.

### 4.4 Callbacks, and what the hook is left with

- In this model `buttons.on` is an entry, not an upcall, so the callback form is safe too. Which
  form to use is then ergonomics, and the pull form — `tick` reads `vacuum.state()` and drains the
  queue [spec 04](specs/04-screen-and-buttons.md) §3 already defines — is the cheaper of the two
  because it needs no dispatch plumbing at all. It wins on its merits rather than because it is the
  only safe one.
- The hook is left with one job: end a beat that overran. That makes the wall-clock deadline
  [spec 01](specs/01-execution-model.md) §5 says cannot exist affordable, because what the exponent
  buys is calls, not checks: the sampling mask is tested on every instruction whatever the exponent
  is, so the exponent only sets how often the callback runs — around
  `BE_VM_OBSERVABILITY_SAMPLING` 12 that is one call per 2048 instructions, a fraction of a
  millisecond of Berry execution on a 240 MHz part. The exponent is a bench number (§6); the shape
  of the cost is not.
- `be_raise` from the hook is the supported way out — the VM already raises from inside
  `be_stack_expansion` — and `be_protectedcall` restores state, so the dispatcher simply learns the
  beat was cut. Because the hook never enters Berry, §1.2's stale `reg` is gone rather than patched
  around.
- The same callback receives `BE_OBS_MALLOC_FAIL`, so a program that eats the heap has a defined
  failure path instead of a crash.

### 4.5 What it would cost

- **Beats replace interleaving.** The plant tick waits for a beat instead of being serviced inside
  it, so the deadline has to sit well under `CONTROL_PERIOD_MS`. A beat can still overrun by one
  native call that cannot be interrupted — an I²C flush — and a draw storm inside one beat is still
  possible. That last one belongs to the display path (rate-limit the flush, last frame wins), not
  to the program model.
- **The authoring model changes.** Programs become plugins with a `self` instead of scripts with
  globals, straight-line waits are gone, and `default` and `browser` are rewritten.
- **Load becomes a phase.** Compile, run the top level, probe, instantiate — more work than "run the
  script", bounded by the same budget, and it is what moves a malformed program's failure to the
  desk.
- **The budget machinery stays.** This removes the reentrancy class and the interleaving argument,
  not the need to bound work.

### 4.6 What comes with it

- The probe is data: `store list` and the browser can show which entry points a program implements,
  which is more than the manifest comment can say.
- Pause becomes first-class. The instance survives a mode switch, so `vm mode=peripheral` could
  suspend rather than destroy ([spec 05](specs/05-disconnected-operation.md) §5 stops and vents).
  Resuming into a plant state somebody else changed is a semantics question this model does not
  answer.
- A program can drive another program's `tick` from Berry. That is Berry calling Berry, which was
  never the unsafe direction, and it is what a browser preview needs.

None of this is taken here: it is §3's second column written down as a requirement with its price,
and §6 is where either column gets settled, by a measurement rather than by this document.

## 5. What a second context costs, and what it does not buy

[Spec 01](specs/01-execution-model.md) §6 already prices the fallback: two readers on one `Wire` (the
sensor poll and the display flush), the snapshot as a cross-task boundary, logs behind a `portMUX`,
a mode switch as a message rather than a call. Three more:

- **A task does not remove the hook.** A `while true` program inside a task still has to be
  stopped, and a task cannot be unwound from outside its own frame: either the hook still reads the
  flag and raises, or the task is deleted — which strands the interpreter, its GC heap and any open
  upvalues. Deleting the task is not a stop path; it is a leak with a reset attached.
- **A second context is isolation, not reentrancy.** A callback dispatched from inside a blocking
  `delay()` is still a nested call inside the VM's frame, because a task can only be entered at its
  own top. The second context buys the tick and the separation; what happens at the upcall is still
  a decision about the program.
- **RAM.** A VM task stack is drawn from the same 512 KB the Berry heap lives in, and
  `BE_STACK_TOTAL_MAX` (20000 values) is the Berry stack's own ceiling, not the task's.

## 6. What would settle it

[Spec 01](specs/01-execution-model.md) §6 names the measurement — a hostile program against the tick —
and this note adds one test to it, because that gate tests the wrong half:

- **The tick test is necessary and not sufficient.** `plant get` and the PID tick surviving a
  `while true` program test the service pass being *called from* the hook. They do not test the
  hook calling *back*: no button is pressed in that test, so no callback is dispatched and no
  nested call happens. `plant get` and `store run other` in [spec 01](specs/01-execution-model.md) §6 are
  a read and a deferral; neither is an upcall.
- **So measure the upcall too.** With the hostile program running, press the button whose callback
  the program registered, and watch for the program's next `state_set` / `draw` to land anywhere
  other than the panel. Run the same test once with `BE_USE_DEBUG_HOOK` on and the same function as
  the hook: the port already documents the fix in `reg = vm->reg;`, so the two results are the
  answer to whether §1.2 bites in practice.
- **And measure the interval as an exponent.** With the knob being
  `BE_VM_OBSERVABILITY_SAMPLING`, the tick can only be held at power-of-two instruction counts, and
  every step down doubles how often the hook runs. The mask itself is tested on every instruction
  either way (§4.4), so what the exponent buys is calls, not checks.

If the upcall measurement fails, the answer is the "re-entered state" or "task-resident call"
column of §3, not a lock in §2; if it passes, the long-lived call stands as it is written and §4's
price is not paid.

## 7. What any shape has to preserve

Not acceptance criteria for something that exists yet: the invariants a later decision has to keep,
whichever it is.

- One writer to the pump and the valve, in whichever context the program runs.
- The tick holds `cfg::CONTROL_PERIOD_MS` under a program that never yields
  ([spec 01](specs/01-execution-model.md) §7).
- A stop lands from any state, including from inside a callback the program registered — the abort
  cannot depend on the program returning ([spec 04](specs/04-screen-and-buttons.md) §3,
  [spec 05](specs/05-disconnected-operation.md) §4).
- The service pass is never run from a point where the VM's cached registers can be invalidated
  (§1.2), or the port is patched so they cannot be — `DEBUG_HOOK()`'s `reg = vm->reg;` is the shape
  of that patch.
- No Berry call is entered re-entrantly except from a point the runtime itself chooses.
