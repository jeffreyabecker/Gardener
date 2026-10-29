---
name: project
description: 'Gardener documentation practice: what the spec, the idea document, the plan, the backlog and the parking-lot line are each for — where each lives and when it goes. A spec describes the present, an idea a possible future, a plan the design that gets there, and a backlog the steps of that plan. Use when editing the README or anything under docs/, or when deciding where a note about work belongs.'
---

# Project

## When to use

- Writing, changing or deleting anything under `docs/`, or editing `README.md`
- Deciding where a piece of work belongs before writing anything down

## The documents

Each one leaves when its job is done, so none of them is a record — git is.

| Document | What it is for | Where | Leaves by |
| --- | --- | --- | --- |
| Spec | the present: what the tree does now | `docs/specs/` | rewritten, or dropped with the behaviour |
| Idea | a possible future: what it could do instead, and the options | `docs/ideas/` | deletion, carried by a spec or a plan |
| Plan | the design: how the tree gets from one to the other | `docs/plans/` | deletion, implemented or dropped with its backlog |
| Backlog | the steps of that plan, one commit each | `docs/backlogs/` | deletion, with its plan, when the last item lands |
| Parking-lot line | a deferred idea, and what is blocking it | `docs/parking-lot.md` | done and in the tree, or dropped with the reason in git |

A spec and an idea are parallel: the spec describes the present, the idea a possible future, and
neither of them proposes work. The plan is the design that takes the tree from the spec's present to
a wanted future — the idea's, where there is one — and the backlog is the steps of that plan.

`docs/README.md` indexes specs, guides and backlogs with a one-line summary, added with the file and
changed when the file's subject changes; plans and ideas are found by name in their folder. The
specs and plans written before the panel port are archived in `docs/archive/specs-and-plans.zip` —
out of date, read by nothing; a document that comes back is rewritten against the tree rather than
unzipped.

`docs/guides/` is the user-facing side: how to assemble and run the thing. A spec says what is true;
a guide says what to do.

## Spec: what is true now

`docs/specs/` is the contract the firmware and its tools satisfy today. Someone implementing or
testing against it needs nothing else: states, transitions, wire formats, limits and errors are all
in the file.

What goes in: observable behaviour; numbers with their units; the rule, and the reason where the
reason is a constraint; what a caller must not do where the firmware relies on it. What stays out:
history, the alternatives an idea weighs, the steps a backlog sets, and a tour of the code — a spec
is a description, not a proposal and not a design.

One file per area, kebab-case, titled with the thing it is the contract for — `serial-protocol.md`,
`vacuum-control.md`; the routine surface has its own folder, `docs/specs/ui/`. It opens with what it
is the contract for and where its edges are, then says it plainly: tables for states and formats,
numbered rules where order matters, prose where a reader needs the reason.

## Idea: a possible future

`docs/ideas/`, one file per question, kebab-case, titled with the question — `second-chamber.md`,
not `vacuum-thoughts.md`. It describes a future the tree does not have: written while the shape is
still in doubt, and it recommends rather than decides.

What goes in: the question in the first paragraph; each option with what it does, what it costs and
what it makes harder; what ruled an option out, named as a constraint rather than a preference; what
would have to be true for the preferred one, and what is still unknown.

What stays out: steps, acceptance criteria, a file list, an estimate, a recommendation stated as a
decision, and detail the idea has not earned. A possibility that fits in a sentence, with the one
constraint blocking it, does not earn a page.

## Plan: how it gets built

A plan is the design: how the tree gets from what the spec says to the behaviour that is wanted, at
the level where the decisions are still reversible — the shape of the change, the approach chosen
and the ones set aside, what it touches, and what could go wrong.

- **What is wrong now, and where.** The behaviour as it exists today — "`setTarget` accepts any depth,
  including past `floor`". Designing against the code that is there is not optional.
- **The shape of the change.** Where the new state, type, task or module goes, and how the paths that
  use it move. Enough that a reviewer can say "that is the right way in" or "no, do it the other way"
  before any code exists.
- **The alternatives, and why not.** The approaches seriously considered, and the constraint that
  ruled each one out.
- **The entry points it has to hold on.** The wire, the panel, a routine, the loader — a change
  enforced on one path is one the others break.
- **How it is known to have worked.** The observations that will show it: a wire exchange, a log line,
  a state read back, a size measured.

What it leaves out is the fine-grained sequence — which step first, which commit, which tick. That
is the backlog's, and writing it in both is how the two drift apart.

## Backlog: the steps

`docs/backlogs/<topic>.md`, one file per effort. A change that runs for many sessions, human or
agent, needs a list that says what is done, what is next, and where a step went wrong; the list is
the status, not a report written afterwards.

An item is the unit of work: a branch, a commit and a diff.

- One imperative per line: what changes, and what is true when it is done. "Reject a pressure target
  above ambient with `err: range`".
- Checked off with `- [x]` as it lands. Unticked items are what is left.
- Ordered so every item leaves the tree working — it builds, and what the previous item verified
  still holds.
- Independent, small and testable: nothing else in flight for it to land, one sitting and one commit,
  and a check that can be made from outside the code. Two items that only ever move together are one
  item.
- No estimates, no owners, no priorities. One file, one effort, one order.

A line that names several files or several unrelated behaviours, that cannot be ticked off in one
sitting, or whose commit message needs "and", is a heading that has not been split.

**A branch per level**, so the history says which task, epic and effort a commit belongs to and any
piece can be set aside on its own: `backlog/<topic>/main`, cut from the branch the effort targets;
`backlog/<topic>/epic-<n>/main` per epic, cut from the backlog branch; and
`backlog/<topic>/epic-<n>/task-<m>` per item, cut from the epic branch. The `main` leaf keeps a
branch from colliding with its children, which git rejects because refs are stored as files.

Work on the task branch, commit the item, merge it back into the epic branch once it is ticked, then
delete it; merge the epic into the backlog, and the backlog into its target once the backlog and its
plan are deleted. Merge with `--no-ff` so each level keeps its own point in the history.

**Work one item at a time**: cut the branch, do the item, verify it, tick it, commit it, merge it
back. One commit per item, so a step that turns out wrong can be walked back without unpicking the
ones around it. Do not run ahead of the list — it hides which step broke the tree. An item that
turns out to be epic-sized is split before the commit, not after.

When the last item lands, delete the backlog and the plan it breaks down, and delete the branches.
If the effort is dropped part-way, the unticked items go with the file unless one is still worth
doing, and then it is a parking-lot line.

## Nothing that leaves is cited

Specs, guides, code comments, script headers, tests, the README and the docs index stand alone and
cite a spec or a guide. Everything else leaves when its job is done, so a link to one becomes a
dangling pointer: a plan is cited only by the backlog that breaks it down, and an idea document and
a backlog by nothing. The dependency runs one way — an idea or a plan may cite a spec, because the
spec stays.