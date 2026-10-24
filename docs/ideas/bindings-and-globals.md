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
other is four pass-throughs grouped under "the modules' surface": `pushPress`/`takePress` forward to
`gestures_`, and `storeGet`/`storeSet` read and write `library_`. None of those methods adds
anything; each exists so a binding can reach state that lives in `GestureQueue` or in the
`Library` the context handed over. `Routines` is the owner of the VM and the loading, and a window
onto the queue and the store at the same time.

The **mixed buckets**, as this was first written, was `UxStack` being three things at once: the
scene stack (`frames_`, `push`/`pop`), the gesture queue (`presses_`, `pushPress`/`takePress`), and
the stop notice — three states sharing a name because they were polled on the same beat. Two of the
three have since left the question. The scene stack is `/system/ux.be` now, a Berry class, so its
frames, its callbacks and its pending push or pop are not C++ at all. The stop notice was not in the
file any more either; that half of the paragraph was stale by the time the stack moved. What is left
is [GestureQueue.{h,cpp}](../../libraries/PanelLink/src/routines/GestureQueue.h): the press ring,
written by the panel task and read by the `buttons` binding, which is exactly the case the
"different writer, different reader" test was meant to catch.

`PanelContext` itself is not part of the problem. It is an interface with two implementations — the
device's `Panel` and the harness's session — so it is the one seam here that earns its interface the
way the firmware conventions describe: it names a foreign boundary, not a second implementation
nobody asked for. Folding it away would be wrong; it is the glue the closures ought to capture, not
the thing to delete.

## The options

### Split the buckets into classes of their own

`UxStack` held three kinds of state that did not change together: the scene stack, the gesture
queue, and the stop notice. Each becomes its own class, and `Routines` becomes only the runtime —
the VM, the loader, and the beat that drives the stack. The scene stack has since gone further than
this option asked and left C++ altogether (`/system/ux.be`), so what remains to split is the rest:

```cpp
class Routines {
  // begin/vm/showDefault/poll/enterPage/active, the loader, and:
  GestureQueue gestures_;
  RoutineStore store_;
};
```

`GestureQueue` is the ring buffer and its overflow, and it already exists. `RoutineStore` takes the
`plainWord` and `documentPath` fact and the `Library` the context handed over, so the `store` module
reaches it directly and `Routines` no longer answers for a routine's values. The `SceneStack` half of
this option is answered by `/system/ux.be`'s `Ux`: the frames, the pending push or pop and the
activate/poll/deliver logic are Berry now, and the interpreter's collector keeps the scenes alive
where the `__scene<i>` globals did.

This is the same reasoning the conventions already use, in the other direction: the chamber's state
and the run's state were one state, so `VacuumSystem` is one class; the scene stack and the button
queue are not one state, so they stop sharing a name. Each new class has one implementation and no
interface — they are state with an owner, not a boundary.

What it costs: a class or two where there was one, and a decision about who builds and holds them.
The counterweight is the conventions' own caution against classes nobody else holds. The gesture
queue is small; it earns its name because a reader and a writer that are not the same thing reach
it — the panel task writes the queue and the bindings read it. The scene stack, which was the third
of those, has left C++ entirely and needs no class here at all.

### A `BindingSurface` as the single point of entry

The classes a binding reaches, plus `PanelContext`, are what a module needs, and a module reaches
several at once. A `BindingSurface` wraps them in one aggregate — a plain struct holding
`PanelContext&` and references to the others, no interface — and the closures capture a
`BindingSurface*` instead of two or three separate pointers:

```cpp
struct BindingSurface {
  PanelContext& context;
  GestureQueue& gestures;
  RoutineStore& store;
};
```

`install()` takes one argument, and every callback reads the one surface and reaches the part it
needs — `surface.context.display()`, `surface.stack.push(...)`, `surface.store.get(...)`. This is
the answer to the question the previous shape left open — one bundled object or several pointers —
and it is the thing that makes the split possible without making every binding take a fistful of
arguments.

### Capture the surface in native closures, and delete the globals

Each module's callbacks all reach the same owners, so the module installer captures one pointer per
module and every function in that module closes over it:

```cpp
// today
void installScreenModule(bvm* vm) { installModule(vm, "screen", kScreenLibrary); }
// after
void installScreenModule(bvm* vm, BindingSurface& surface) {
  installModule(vm, "screen", kScreenLibrary, &surface);
}
```

`installModule` pushes each function as a closure with one upvalue holding the pointer, and a
callback reads its owner back at entry instead of naming `gContext`. `gContext`, `gRoutines` and
`gLogger` disappear; `install()` shrinks to the one surface, and the harness's `gSession` goes the
same way.

Cost: one unpack line at the top of each callback, hidden behind a helper. The `bnfuncinfo` table
no longer says everything — the installer has to know it is capturing a pointer — but the table
shape itself can stay, since one captured pointer per module serves every function in it.

What it makes harder: nothing in the code that exists. It removes the "one VM and one app" sentence
the globals encoded, but only in the direction that helps — a second VM would capture its own
surface, which globals cannot express at all.

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

## What each option was ruled in or out by

The closures and the `BindingSurface` rule the rest in together: the closures delete the globals,
and the surface gives them one thing to capture, which is what lets the buckets split without
scattering pointers through every binding. The stated dislike of the globals rules out "keep them,
fewer"; the VM-global option is ruled out by the namespace it would hand to routine code and by the
indirection it adds for no gain over a closure.

The split is ruled in by the same test that kept `VacuumSystem` whole — is it one state or three? —
and the buckets answered three. It is disciplined by the conventions' caution against classes
nobody holds: a bucket earns its class only when its reader and its writer are not the same thing,
which is true for the queue (panel task writes, bindings read).

What has to be true for the preferred option: the surface must point at objects that outlive the
VM — the same lifetime the globals already assume, since `Panel` and `Routines` outlive the
interpreter they built. The `bnfuncinfo` table and its `installModule` are the only place the
installer's shape has to change, and the harness's `installHarnessModule` follows the same shape.

What is still unknown: whether the remaining classes are built and owned by `Routines` or by the
composition root that hands them to both `Routines` and the surface; whether the harness migration
rides in the same change or follows it; and whether the panel task's `stopSeq` counter, which the
`StopNotice` compares against, is passed in as the field it already is or folded into the class.

## Where I land

Do the rest of the whole thing, in this order: give `store` its own `RoutineStore`, and leave
`Routines` as the runtime that owns and drives what is left. The scene stack is already gone from
C++ (`/system/ux.be`) and the gesture queue already has its class. Build a `BindingSurface` over
`PanelContext` and the remaining classes, capture it in native closures, and delete the three
globals.

The closures are not an exotic mechanism — the VM's own libraries are built on them — and they turn
the globals from a fact of the API into a choice that was never necessary. The split is the same
"is it one state?" test `VacuumSystem` was decided by, answered the other way. The surface is what
makes the split liveable. Held with moderate confidence on the whole of it; the still-open parts
are who owns the four classes and whether the harness migration rides in the same change.
