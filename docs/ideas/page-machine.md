# Should a page own its machine?

Three routine pages run a state machine and none of them declares one. `interval`, `ramped_interval`
and `ramped_static` each keep a `phase` string and move it from `advance()`, `ok()`, `pause()`,
`edit()` and `settings_closed()`. They share a vocabulary — `ready`, `waiting`, `paused`,
`approach`, `work`, `run`, `rest_approach`, `rest` — but each page re-derives which of those it is
in, and which of them mean "running", in its own methods. Roughly thirty `self.phase` mentions per
page, and about ten methods per page consult it.

What is being weighed is a `Machine` class a page constructs, whose records are just
`event => (callback, next state)`; a state is declared once and holds any number of them. Because a
Berry closure captures the page it was written in, the machine needs nothing from the page — no
`owner`, no `super()`, no inheritance — and because a record names an event and no condition, the
machine never has to ask anything: it only routes an event the page chose to send. The question is
whether a page should own one, and what owning rather than inheriting costs.

## The shape being weighed

In the page's `init()`:

```berry
self.machine = Machine()

var approach = self.machine.state("approach")
approach.on("arrived", / -> self.mark_arrival(), "run")
approach.on("lost", / -> self.abandon(), "ready")

var run = self.machine.state("run")
run.on("elapsed", / -> self.finish(), "ready")
```

Read as: in `approach`, an `arrived` event marks the arrival and goes to `run`; in the same state, a
`lost` event abandons the run and goes to `ready`. Every closure captures the page, so the machine's
whole dispatch is a lookup of the event in the current state's records, the action, and the current
state becoming the record's target. `Page.on()` already registers a gesture this way
(`/ -> self.ok()`), so an action is the same kind of value the codebase already passes around.

The machine holds no reference to the page and asks no questions. That is the property being
examined: if a closure is enough to reach the page, and the page alone decides when an event is
true, then the machine is a routing table and nothing more.

## Where the events come from

A record has no guard, so the condition that makes an event true is written where it is known —
beside the thing that observes it:

```berry
def tick(now)
  if self.editor != nil
    return
  end
  var s = vacuum.state()
  if s.state == "HOLD" && s.target == self.target
    self.machine.fire("arrived")
  end
  if self.run_left() == 0
    self.machine.fire("elapsed")
  end
  if vacuum.stopped()
    self.machine.fire("lost")
  end
end
```

`elapsed` is the example that matters: it is true when the page's own clock says so, and the page is
the only thing that can know that. Putting `self.run_left() == 0` in the record would make the
machine ask it on every poll, in every state, and would put the page's clock inside a declaration
that is supposed to be about transitions.

Because a state ignores an event it does not declare, the firing code does not have to know where
the page is: it states facts, and the machine decides whether the current state cares. `arrived`
fired in `ready` is dropped there and only acts in `approach`; `elapsed` fired in `approach` does
nothing. That is what keeps the block above short — no state is mentioned in it.

The caveat is repetition. A fact that stays true — `arrived` while `run` sits at its target — is
fired on every poll and dropped on every poll, so either the firing block asks `is()` before firing
the persistent ones, or the machine's drop log is per state and event rather than per drop. Neither
is a condition inside a record, which is the thing being kept out.

The other events come from where they already are. OK and Back fire from the gesture handlers, the
editor fires when it closes, and a routine that starts a run fires its own event at the point it
knows the start succeeded. An event that can mean two things is two events: a start that must be
refused is a page that does not fire `ok`, not a record that conditionally transitions.

This is the split the machine is buying: the page keeps the conditions, which it always had, and
gives up the transitions, which is what the ladder is made of.

## What capturing closures buys

* **No inheritance slot, and nothing on pages without a run.** The pages keep `extends Page` and
  spend nothing on the machine, so `default`, `browser` and `system` never construct one and never
  carry a state. `Page` does not change either, so the [routine spec](../specs/ui/routine.md) — which
  describes `Page` as what a routine extends — does not grow an artifact every routine carries.
* **No load-order constraint.** Under inheritance the machine's file would have to be reachable
  before `page.be` loads, which makes `phasemachine.be` a bug purely because of its name. A class a
  page constructs only has to load before the routines, which every `/system/` and `/modules/` file
  does.
* **A machine that is not a page.** The records are closures, not page methods, so the same class can
  be driven by a host script or by a page: what the machine does is entirely in the closures handed
  to it. That is what makes it possible to exercise the transitions of a page without a panel.
* **Nothing to schedule and nothing to order.** With no guard, one event in one state has one
  record, and dispatch is a lookup rather than a scan; the machine has no work to do between events.
  Two records for the same event in the same state is a declaration error the API can refuse when
  the machine is built.

## What it costs

* **The conditions are still in the page.** The machine declares transitions; it does not say what
  `arrived` means. Someone reading only the records sees the shape of the run but not the fact that
  it waits for `HOLD` at the target, so the event-firing block has to be read too.
* **Two places to look per page.** The records are in `init()` and the event sources are in `tick()`
  and the gesture handlers. That is fewer places than the ladder's ten methods, but it is not one.
* **An unrelated event and a mistake look the same.** Dropping undeclared events is what lets the
  page state facts rather than ask where it is; it is also how a page firing an event from the wrong
  state goes unnoticed. The drop log is the only thing that tells the two apart.
* **One closure and one record per edge, at load.** Twenty records is twenty small objects per page
  instance, built once in `init()`. The routines have never been measured, so this is the number the
  idea would want before it is taken. A closure also pins the page instance it captured, which is
  fine while pages are singletons registered once at load.
* **The page has to fire the events.** Inheritance's one genuine advantage was that `Page.poll()`
  would run the machine whether or not the page remembered; a machine with no guards has nothing to
  run, so what the page must remember instead is to translate the clock and the chamber into events
  in `tick()`. That is the same kind of line `self.advance(now)` is today, so the risk is not new,
  but it is not zero either.

## What the pages do instead

`advance()` is a ladder: a pre-check that the run was not taken away (`lost()`), then one
`if self.phase == ...` block per phase, each testing its exit condition and assigning the next phase.
The commands ask the phase again — `ok()` picks start or resume, `pause()` refuses unless the page is
running, the editor stops a run it is about to change — and the display asks it a third time:
`status_text()`, `hold_left()` and their counterparts in the ramped pages.

The cost is that a phase assignment in one method is invisible to the others, so "which phases are
running" is a list copied by hand into every method that needs it — and `tick()` already carries a
second copy of the abandoned-run edge that `lost()` covers. `ramped_interval` is `interval` with a
list of targets and a `paused` phase folded in: the same ladder, copied. The benefit is that each
block reads as its condition and its consequence together; a machine has to beat that, not merely
tidy it.

## Options

### Own it: the page holds a machine

The shape above. It is the smallest change that makes the transitions declarable, and it leaves
`Page`, the routine spec and the pages that have no run alone. The machine can also answer the
question the display methods keep asking — `self.machine.is("approach", "run")` rather than four
places each listing the phases that count as running. Whether the records are added one at a time or
handed over together is the design work; what matters is that the state owns its records, the
records own their closures, and the page owns the events.

### Inherit it: `Page` extends `Machine`

The same declaration API reached through the parent. What it adds over owning one is that `is()` is
available without constructing anything and the state is declared in the object every page already
is. The lifecycle argument that a guarded machine would have had — `Page.poll()` running it on
every pass — does not survive a machine with no guards: it has nothing to do between events.
What it costs is a state and an API on every page that will never use them, and a base class that
every routine — including ones written outside this tree — inherits. If the machine ever needed to
intercept the page's lifecycle, this would be the argument for it; it does not.

### Leave it alone

Nothing is broken. Three pages work, and the duplication between `interval` and `ramped_interval` is
a copy of a ladder rather than a copy of a bug. The project's guidance asks an abstraction to pay for
itself in code that exists, and today there are three callers and no fourth, so "no" is a legitimate
answer that the fourth page would have to overturn.

### Ruled out

* **A guard on the record.** `on("elapsed", guard, action, next)` would have the machine ask a
  question the page can answer once, on every poll, in every state — and it would let one event mean
  two things depending on a condition buried in a record. The page firing the event is where that
  condition already lives.
* **A native `machine` binding.** It would move a page's phases out of editable script and into the
  firmware, and the phases belong to the page: the events come from the page's own clock, its
  gestures and the chamber it reads.
* **A hierarchical machine** (parent states, entry/exit actions, deferred events). No page nests a
  phase inside another.
* **One class per phase.** The phases share the page's state — settings, clock, target, chrome — so
  the fields would split across seven classes, and the phase would stop being a value the display
  methods compare against.

## What would have to be true

* A record is `(event, action, next)` and nothing else. The action runs on the event; it does not
  decide whether to transition, because the decision is whether the event was fired.
* Every state's events are distinct. Two records for the same event in the same state is a
  declaration error, refused when the machine is built, and with it goes any dispatch-order rule.
* A next state that was never declared is caught when the machine is built, not when it fires. The
  API is the only thing that can do this, and Berry's strings would otherwise make a typo a
  transition that never happens. Whether a record names the state object or its name decides what
  the API can promise: an object cannot be mistyped but a record can only point backwards, and a
  name allows any order at the cost of a check before the run starts.
* An event fired in a state that does not declare it is dropped, and says so through `vacuum.log`.
  Silence would make a page wrong about its own state invisible.
* The edge that leaves every running phase at once — `lost()` — has a home: the record repeated per
  state, or declared once as from-any. This is the one place the API might need more than a state
  and its records.
* The declaration is short enough to read in one block, which for these pages means eight states and
  fewer than twenty records.

## What is still unknown

* Whether a fourth page copies the ladder again. Three hand-written pages are a pattern; a fourth
  that copies the ladder is an argument, and one that diverges is an argument against.
* Whether moving the conditions out of the machine actually shortens the page, or just moves the
  same `if`s into `tick()`. The ladder's `if`s also assign the next phase; the tick's `if`s only
  fire, so the page should get shorter by the assignments, not by the conditions.
* Whether `is()` actually removes the display-side phase questions or only renames them. The page
  still decides what `rest` should print, and those may be `if`s on purpose.
* Whether the routine spec should say anything at all. Under ownership it can stay silent: a machine
  is a module a page may use, not a contract a page must satisfy.

## What I think

Owning a machine of closures is the version worth building, and dropping the guard is what makes it
small. The machine holds a current state and, per state, events with an action and a target; the page
says when each event is true, at the place that knows. The evidence is the copy between `interval`
and `ramped_interval`, and the mechanism is a closure that reaches the page, kept in a table,
dispatched on an event.

The honest risk is that the conditions do not disappear, they move into `tick()` — so the page still
has `if`s, and the gain is the transitions rather than the tests. That is still the gain worth
having: the ladder's cost is a phase assignment being invisible from every other method, and a fired
event is exactly as visible as the state it lands in.

Held moderately: if the fourth page copies the ladder, build `Machine` as a class the page owns. If
that page's machine and `Page`'s lifecycle ever need to interleave, inheritance becomes an argument
again — but nothing today suggests they will.
