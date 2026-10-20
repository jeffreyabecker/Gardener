---
name: project
description: 'Gardener project documentation and Git practice: specs versus plans, a plan is deleted when implemented so nothing may reference one, parking-lot notes, and how to commit (one commit, the whole tree, the history style). Use when editing README, docs or code comments, writing or retiring a plan or spec, or when asked to commit.'
---

# Project

## When to use

- Editing `README.md`, `docs/specs/`, `docs/guides/` or `docs/plans/`
- Writing a plan, a spec or a parking-lot entry
- Naming a plan anywhere, including a code comment or a script header
- Asked to commit

## Specs and plans

**A plan is a requirement, not a design.** `docs/plans/` records proposals and design work;
`docs/specs/` is what the firmware does now. A plan leaves when it is implemented or dropped:
delete it, and carry what it established into the spec.

**A parking-lot entry is a note, not a plan.** `docs/plans/parking-lot.md` holds unrealized
"what if" ideas. One or two sentences say what the idea is and what is blocking it; the design
waits for whoever picks it up. Detail added today is detail the idea has not earned.

**Nothing references a plan.** A plan is deleted the moment it is implemented or dropped, so any
reference to one — a link, a section number, a filename in prose — becomes a dangling pointer as
soon as the plan's work lands. That holds everywhere, not just in `docs/specs/` and `docs/guides/`:
the project README, a code comment, a script header, a test fixture. Cite the plan when you are
writing or arguing for it; once the decision is real, write the requirement where it will live and
let the plan go.

`docs/specs/` and `docs/guides/` describe what the firmware and its tools do now, so they stand
alone: write the requirement into the spec or guide itself. A plan may reference a spec, because a
proposal depends on the contract it changes; the reverse is never right. `docs/README.md` is the one
index of plans, and it is the only place a plan is named once the change is in.

## Committing

**Committing means commit.** When you are told to commit, stage what is in the tree and write the
message: one commit, the change as it stands, in the style the history already uses. Don't re-run
what was verified when the change was made, don't split one request into a tidier series of commits,
and never revert files to rebuild an intermediate state so that a commit looks smaller. Verification
belongs to the change, not to the commit.
