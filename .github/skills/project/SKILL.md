---
name: project
description: 'Gardener project documentation and Git practice: specs versus plans versus idea documents, a plan or an idea is deleted when carried so nothing may reference one, parking-lot notes, and how to commit (one commit, the whole tree, the history style). Use when editing README, docs or code comments, writing or retiring a plan, an idea or a spec, or when asked to commit.'
---

# Project

## When to use

- Editing `README.md`, `docs/specs/`, `docs/guides/`, `docs/plans/` or `docs/ideas/`
- Writing a plan, an idea, a spec or a parking-lot entry
- Naming a plan or an idea anywhere, including a code comment or a script header
- Asked to commit

## Specs and plans

**A plan is a requirement, not a design.** `docs/plans/` records proposals and design work;
`docs/specs/` is what the firmware does now. A plan leaves when it is implemented or dropped:
delete it, and carry what it established into the spec.

**A parking-lot entry is a note, not a plan.** `docs/plans/parking-lot.md` holds unrealized
"what if" ideas. One or two sentences say what the idea is and what is blocking it; the design
waits for whoever picks it up. Detail added today is detail the idea has not earned.

**An idea document is not a plan.** `docs/ideas/` records a direction being weighed before it has
earned a requirement: the options, what each costs and what set it aside. It commits to nothing and
may end in "no". It is deleted once a plan or a spec carries what it established, so it is not a
home for steps, acceptance criteria or a file list. `.github/skills/ideation/` has the detail.

**Nothing references a plan.** A plan is deleted the moment it is implemented or dropped, so any
reference to one — a link, a section number, a filename in prose — becomes a dangling pointer as
soon as the plan's work lands. That holds everywhere, not just in `docs/specs/` and `docs/guides/`:
the project README, a code comment, a script header, a test fixture. Cite the plan when you are
writing or arguing for it; once the decision is real, write the requirement where it will live and
let the plan go. An idea document is deleted on the same terms, so nothing may reference one either.

`docs/specs/` and `docs/guides/` describe what the firmware and its tools do now, so they stand
alone: write the requirement into the spec or guide itself. A plan may reference a spec, because a
proposal depends on the contract it changes; the reverse is never right. `docs/README.md` is the one
index of plans and ideas, and it is the only place a plan or an idea document is named.

## Committing

**Committing means commit.** When you are told to commit, stage what is in the tree and write the
message: one commit, the change as it stands, in the style the history already uses. Don't re-run
what was verified when the change was made, don't split one request into a tidier series of commits,
and never revert files to rebuild an intermediate state so that a commit looks smaller. Verification
belongs to the change, not to the commit.
