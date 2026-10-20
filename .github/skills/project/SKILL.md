---
name: project
description: 'Gardener project documentation and Git practice: specs versus plans, a plan leaves when it is implemented, parking-lot notes, specs and guides never link to plans, and how to commit (one commit, the whole tree, the history style). Use when editing README or docs, writing or retiring a plan or spec, or when asked to commit.'
---

# Project

## When to use

- Editing `README.md`, `docs/specs/`, `docs/guides/` or `docs/plans/`
- Writing a plan, a spec or a parking-lot entry
- Asked to commit

## Specs and plans

**A plan is a requirement, not a design.** `docs/plans/` records proposals and design work;
`docs/specs/` is what the firmware does now. A plan leaves when it is implemented or dropped:
delete it, and carry what it established into the spec.

**A parking-lot entry is a note, not a plan.** `docs/plans/parking-lot.md` holds unrealized
"what if" ideas. One or two sentences say what the idea is and what is blocking it; the design
waits for whoever picks it up. Detail added today is detail the idea has not earned.

**Specs and guides stand alone.** `docs/specs/` and `docs/guides/` describe what the firmware and
its tools do now. They must not link to `docs/plans/`: a plan is a transient design record that
leaves when it is implemented or dropped, and a contract that depends on one rots with it. Write
the requirement into the spec or guide itself.

A plan may reference a spec, because a proposal depends on the contract it changes. The reverse is
never right. `docs/README.md` is the one index of plans; specs, guides and the project README do
not link to one.

## Committing

**Committing means commit.** When you are told to commit, stage what is in the tree and write the
message: one commit, the change as it stands, in the style the history already uses. Don't re-run
what was verified when the change was made, don't split one request into a tidier series of commits,
and never revert files to rebuild an intermediate state so that a commit looks smaller. Verification
belongs to the change, not to the commit.
