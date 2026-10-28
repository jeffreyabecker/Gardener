---
name: plans
description: 'Plans in docs/plans/: a proposal that states an actionable requirement — what is wrong now, what has to be true instead and how that is checked — rather than a design, and is deleted once implemented or dropped, together with any backlog that breaks it down. Also covers the parking lot in docs/parking-lot.md. Use when writing, reviewing or retiring a plan, adding a parking-lot entry, or naming a plan anywhere.'
---

# Plans

## When to use

- Writing, changing or reviewing a document in `docs/plans/`
- A question is settled and the requirement needs a home
- Adding or removing a parking-lot entry
- Naming a plan anywhere, including a code comment or a script header

## A plan is a requirement, not a design

`docs/plans/` records proposals: work that is wanted and argued for, described by what has to be
true when it is done. The behaviour, the constraint, the trade being accepted — that is the plan.
Which file, which class, which mechanism does it is the implementer's call, and this project answers
a plan with the smallest mechanism the current code allows.

The folder is empty for now: the plans written before the panel port are in
`docs/archive/specs-and-plans.zip`, out of date and read by nothing. A plan written today lands in
`docs/plans/` as usual.

A plan that dictates a task, a lock, a queue or a watchdog has answered a question that belongs to
the code. A plan that lists the steps in order has become a backlog. State the requirement and let
the design happen where the code is.

A backlog may break the plan into tasks and name it. That is the only document a plan is referenced
from, because the two leave together.

## A requirement has to be actionable

"Not a design" is not licence to be vague. A plan is read by whoever implements it, with nothing but
the file in front of them, so it has to be specific enough to start from and to finish against. A
requirement carries four things:

- **What is wrong now, and where it lives.** Name the verb, the characteristic, the state or the
  function as it exists today: "`setTarget` accepts any depth, including past `floor`", "a
  non-owner's `stop` is refused before the opcode is read". Locating the problem in the current code
  is not a design; it is how the reader finds the behaviour to change. Name the code that is wrong,
  never the code you would write in its place.
- **What is true instead.** The new state, transition, reply, refusal or limit in the terms a spec
  would use: from what to what, whether the old answer survives, and what a caller sees. Say which
  entry points it has to hold on — the wire, the panel, a routine, the loader — because a rule
  enforced on one path is a rule the others break, and "the target is bounded" leaves the
  implementer to guess that the panel sets one too.
- **The number, where a threshold is involved.** Ranges, timeouts and limits with their unit and
  where the value lives. "A reasonable bound" is not a requirement; "5–100 kPa, clamped when loaded
  and refused when written" is.
- **How it is checked.** The observation that shows the requirement holds: a wire exchange, a log
  line, a state read back, a size measured. A requirement whose only check is that the code looks
  right is not finished.

If you cannot write the first and the last of those, the requirement is not understood yet — it is
an idea or a question, and it does not belong in a plan. If you can write all four but not the
mechanism, that is exactly right: the mechanism is the implementer's decision.

| Vague | Actionable |
| --- | --- |
| Make `stop` reliable. | `stop` is accepted from any link before the ownership gate, cannot be dropped by a full queue, and does not restart a vent timer. |
| Bound the tuning values. | `flow`, `volume`, `deadband` and `pump_max` refuse a write outside their ranges on both links, and the refusal names the field and the bound. |
| Telemetry should be ordered. | The device's uptime is a characteristic notified at the fast rate, so a client can place a result in time and tell a reconnect from a restart. |

## One requirement, one section

A heading holds one requirement, so each one can become one backlog item, land on its own and be
checked on its own. Two outcomes that could ship separately under one heading are two requirements,
and a bullet that needs "and" to describe its outcome is a split that has not happened.

A plan whose body is a list of noun phrases — "pairing hardening", "telemetry ordering", "tuning
bounds" — has headings without requirements. Each of them needs the four things above before the
plan is worth a backlog.

## What the plan still leaves out

- **Mechanism.** File, class, function, task, lock, queue, flag: the implementer's call.
- **Order and size.** Sequence, phases, estimates and ownership belong to the backlog, written when
  the work is taken up.
- **History.** What the code used to do and which bug prompted the rule, unless the reason
  constrains the fix.
- **Detail the requirement has not earned.** A diagram of a part nobody has chosen, a field list for
  a message nobody has needed.

## Shape

No two plans are the same length, but an actionable one holds the same parts:

1. **What this is**, in a paragraph: the behaviour that is wrong, what the plan changes, and what it
   deliberately leaves alone.
2. **The requirements**, one section each, stating the problem as it stands, what is true instead,
   the numbers where there are any, and the check.
3. **Consequences**, where the requirement touches a contract: the spec that now says something
   different, the wire fields added, refused or removed, the guide that warns about something new.
   Named by their subject rather than as a file list to edit.
4. **Deferred**, pointing at the parking-lot lines for what this plan deliberately leaves out.
5. **How it is known to hold**, the checks gathered into one pass, so a build can be held against
   the plan without reading the code.

A plan can be smaller than that — a paragraph and one requirement, if the change is one behaviour —
and should be. It cannot be larger than its own specificity: length that adds no requirement, number
or check is padding.

## Leaving

A plan leaves when it is implemented or dropped: delete it, and carry what it established into the
spec. Do not check it off, do not mark it done, and do not keep it as a record — git has it, and a
plan that stays after its work lands is a second, staler copy of the requirement.

A plan that a backlog breaks down is not done until that backlog is. When the backlog's last item
lands both are deleted, and deleting the plan at any point deletes any backlog that breaks it down.

## Only a backlog references a plan

A plan is deleted the moment it is implemented or dropped, so any reference to one — a link, a
section number, a filename in prose — becomes a dangling pointer as soon as the plan's work lands.
That holds everywhere, not just in `docs/specs/` and `docs/guides/`: the project README, a code
comment, a script header, a test fixture. The one exception is the backlog that breaks the plan
down, because the two are deleted together.

Cite the plan when you are writing or arguing for it. Once the decision is real, write the
requirement where it will live and let the plan go.

## The parking lot

`docs/parking-lot.md` holds unrealized "what if" ideas: work that has been considered and
deliberately deferred. An entry is a note, not a plan — one or two sentences saying what the idea is
and what is blocking it. The design waits for whoever picks it up, because detail added today is
detail the idea has not earned.

An entry leaves when it is done and the result is in the tree, or when it is dropped and the reason
is in git.
