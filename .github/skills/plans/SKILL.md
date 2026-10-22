---
name: plans
description: 'Plans in docs/plans/: a proposal that states a requirement — what has to be true — rather than a design, and is deleted once implemented or dropped, together with any backlog that breaks it down. Also covers the parking lot in docs/plans/parking-lot.md. Use when writing, reviewing or retiring a plan, adding a parking-lot entry, or naming a plan anywhere.'
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

A plan that dictates a task, a lock, a queue or a watchdog has answered a question that belongs to
the code. A plan that lists the steps has become a backlog. State the requirement and let the design
happen where the code is.

A backlog may break the plan into tasks and name it. That is the only document a plan is referenced
from, because the two leave together.

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

`docs/plans/parking-lot.md` holds unrealized "what if" ideas: work that has been considered and
deliberately deferred. An entry is a note, not a plan — one or two sentences saying what the idea is
and what is blocking it. The design waits for whoever picks it up, because detail added today is
detail the idea has not earned.

An entry leaves when it is done and the result is in the tree, or when it is dropped and the reason
is in git.
