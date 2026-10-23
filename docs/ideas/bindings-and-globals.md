# Who should own the native bindings?

`Routines` has become a facade for state that lives elsewhere, and the native modules reach it
through three globals. The question being opened is whether the bindings should reach their owners
by a captured pointer instead of a global, and where the scene stack, the gesture queue and the
routine store should actually live. This is weighed now because the surface has accumulated to the
point where every new module call adds another pass-through on `Routines` and another read of a
global, and each addition has to be made in the same three places.

What the bindings are is clear: the native half of the surface [bindings.md](../specs/ui/bindings.md)
documents, seven modules that read and command the panel parts and the machine. What has gotten
messy is how they are wired to their owners.

## What is there now

Three seams carry the bindings' state today.

The **globals**. [Module.h](../../libraries/PanelLink/src/routines/bindings/Module.h) declares
`gContext`, `gRoutines` and `gLogger`, and [Bindings.cpp](../../libraries/PanelLink/src/routines/Bindings.cpp)
is their one writer. These three are the globals in question, and they are not the only file-scope
names in the library: `stopSeq` and `goHomeSeq` in `PanelTasks.h` are cross-task scalars with one
writer each, and the font tables in `panel/Fonts.h` are const data. Those two groups are a
different mechanism — shared state between two tasks, and read-only tables — so they are set aside;
the bindings globals are the only ones that exist because of an API the VM does not actually
impose.

The reason the bindings globals exist is stated in the header: the native callbacks are plain C
functions with no user data, and there is one VM and one app. Three things are wrong with that. The VM ships
native closures with upvalues — `be_pushntvclosure`, `be_pushcomptr`, `be_setupval`, and the same
list and range libraries use them — so the premise is narrower than it claims: a callback *can*
carry data, just not through `be_pushntvfunction`. And `gLogger` is a second name for
`gContext->logger()`, so one of the three is already redundant. The host harness has the same seam
in [HarnessModule.cpp](../../tools/berry/host/HarnessModule.cpp) (`gSession`), so whatever cures the
device also cures the harness.

The **facade**. [Routines.h](../../libraries/PanelLink/src/routines/Routines.h) has two halves. One
is the runtime: `begin`, `vm`, `poll`, `enterPage`, `showDefault`, `active`, and the loader. The
other is eleven pass-throughs grouped under "the modules' surface": `push`/`pop`/`pop_to_root`/
`top`/`depth` forward to `stack_`, `pushPress`/`takePress` forward to `stack_`, `takeStopped`
forwards to `stack_`, and `storeGet`/`storeSet` read and write `library_`. None of those methods
adds anything; each exists so a binding can reach state that already lives in `UxStack` or in the
`Library` the context handed over. `Routines` is the owner of the VM and the loading, and a window
onto the stack and the store at the same time.

The **mixed buckets**. [UxStack.h](../../libraries/PanelLink/src/routines/UxStack.h) is three
things: the scene stack (`frames_`, `push`/`pop`), the gesture queue (`presses_`, `pushPress`/
`takePress`), and the stop notice (`lastStopSeq_`, `takeStopped`). The stack owns all three because
they happen to be polled on the same beat, but a scene stack, a button queue and a one-shot flag
are different kinds of state with different writers — the queue is filled by the panel task, the
flag by the hand-off, the stack by the scene itself.

`PanelContext` itself is not part of the problem. It is an interface with two implementations — the
device's `Panel` and the harness's session — so it is the one seam here that earns its interface the
way the firmware conventions describe: it names a foreign boundary, not a second implementation
nobody asked for. Folding it away would be wrong; it is the glue the closures ought to capture, not
the thing to delete.

## The options

### Capture the owners in native closures, and delete the globals

Each module's callbacks all reach the same owners, so the module installer captures one pointer per
module and every function in that module closes over it:

```cpp
// today
void installScreenModule(bvm* vm) { installModule(vm, "screen", kScreenLibrary); }
// after
void installScreenModule(bvm* vm, PanelContext& context) {
  installModule(vm, "screen", kScreenLibrary, &context);
}
```

`installModule` pushes each function as a closure with one upvalue holding the pointer, and a
callback reads its owner back at entry instead of naming `gContext`. `gContext`, `gRoutines` and
`gLogger` disappear; `install()` shrinks from three arguments to the owners the modules actually
need, and the harness's `gSession` goes the same way.

Cost: one unpack line at the top of each callback, hidden behind a helper. The `bnfuncinfo` table
no longer says everything — the installer has to know it is capturing a pointer — but the table
shape itself can stay, since one captured pointer per module serves every function in it.

What it makes harder: nothing in the code that exists. It removes the "one VM and one app" sentence
the globals encoded, but only in the direction that helps — a second VM would capture its own
context, which globals cannot express at all.

### Keep the globals but reduce them to one

Delete `gLogger` and derive it from `gContext->logger()`, and either fold `gRoutines` into the
context or leave the two globals and call it good. This is the smallest change and removes the
redundant name, but it keeps the pattern the question is about. It would be the answer only if the
dislike were of the count rather than of the mechanism, which it is not.

### Move the owners into the VM as Berry globals

Stash the context as a comptr under a global name and have the callbacks read it with
`be_getglobal`. This moves the global from C into the interpreter rather than removing it. It
pollutes the namespace a routine sees, adds a fetch to every call, and makes the owners nameable
from routine code — strictly worse than the closures, which keep the pointer where a routine can
never reach it.

### Slim `Routines` once the closures are in

The closures make the facade unnecessary: a binding reaches the `UxStack` and the `Library` through
the captured pointer, so `Routines` sheds the eleven pass-throughs and keeps the runtime and the
loader. What this leaves open is `store`. The `plainWord` and `routinePath` helpers are the
"routine documents live at `/routines/<name>.cfg`" fact, which is `Routines`' own; but the actual
read and write are `Library`'s, and the `store` module could capture `Library&` directly and take
the path fact with it. Whether `store` stays on `Routines` or moves to the `store` binding is a
second-order question this option does not settle.

### Split `UxStack` into its three buckets

The scene stack, the gesture queue and the stop notice could each become their own owner, reached
directly by their own module. Against it: the gesture queue is four fields and the stop notice is
one, and the firmware conventions argue against classes nobody else holds — "one class where one
class works" cuts against inventing a `GestureQueue` with one caller. The buckets sit together in
`UxStack` because one beat drives them; the real question is only whether the bindings hold them
through `Routines` or directly.

## What each option was ruled in or out by

The closures are the constraint that rules the others in. The stated dislike of the globals rules
out "keep them, fewer"; the VM-global option is ruled out by the namespace it would hand to routine
code and by the indirection it adds for no gain over a closure. The `UxStack` split is not ruled
out but not earned: the three buckets have one reader apiece, and a split names parts that nothing
else composes.

What has to be true for the preferred option: the captured pointer must point at objects that
outlive the VM. That is the same lifetime the globals already assume — `Panel` and `Routines`
outlive the interpreter they built — so it is a stated assumption, not a new hazard. The
`bnfuncinfo` table and its `installModule` are the only place the installer's shape has to change,
and the harness's `installHarnessModule` follows the same shape.

What is still unknown: whether to bundle the captured owners into one small context object or
capture two pointers (`PanelContext*` and `Routines*`) per module; whether `store` keeps its path
fact on `Routines` or moves it to the binding; and whether the harness migration rides in the same
change or follows it.

## Where I land

Capture the owners in native closures and delete the three globals, then let `Routines` shed the
pass-through surface that only existed to feed them. The closures are not an exotic mechanism — the
VM's own libraries are built on them — and they turn the globals from a fact of the API into a
choice that was never necessary. The `UxStack` split I would leave: the buckets share a beat, and
splitting them names parts with no second owner. Held with moderate confidence: the closures and
the `Routines` slimming I would do; the one-context-object-versus-two-pointers and the fate of
`store` are open enough to decide when the work is picked up.
