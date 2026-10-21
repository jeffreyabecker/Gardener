---
name: backlog
description: 'Backlogs in docs/backlogs/: the fine-grained tasklist breakdown of the plan it implements — ordered, checkable items that each leave the tree working, with a branch per backlog, epic and task, a commit per item, and a summary written after each epic. Use when breaking decided work into steps, working through a tasklist, or deciding whether something is a task, a plan or a parking-lot line.'
---

# Backlog

## When to use

- Breaking work into steps before starting it
- Working through, adding to or finishing a document in `docs/backlogs/`
- Deciding whether a piece of work is a task, a plan or a parking-lot line

## A backlog is a tasklist

`docs/backlogs/<topic>.md`, one file per effort. It is the breakdown of work that has already been
decided: `- [ ]` items, each one a single step that can be done and checked on its own, ordered so
the effort can be worked from the top down.

A backlog is usually the detailed breakdown of a plan, and it may name the plan it implements. That
is the one reference a plan accepts, because the two leave together.

A backlog breaks work down; it does not choose the work or design it. If an item needs a decision
that has not been made, that decision belongs in a plan — or in an idea document if it is still
being weighed — and the item states the outcome. A backlog full of "decide how to…" lines is a plan
that has not been written yet.

## Items

- One imperative per line: what changes, and what is true when it is done. "Split the gesture
  classifier out of `Panel.cpp`" or "Reject a pressure target above ambient with `err: range`".
- Fine enough to finish and check without waiting on another item. If two items always move
  together, they are one item; if one item spans several unrelated files, it is several.
- Ordered so that every item leaves the tree working — it builds, and what the previous item
  verified still holds.
- Checked off with `- [x]` as it lands. Unticked items are what is left, so the list is the status.
- No estimates, no owners, no priorities. One file, one effort, one order.

## Branches

Each level of the backlog gets its own branch, so the history says which task, epic and effort a
commit belongs to and any piece can be set aside or dropped on its own:

- `backlog/<topic>/main` — one per backlog, cut from the branch the effort targets (`main` unless
  the backlog belongs to a larger effort). The `main` leaf keeps the branch from colliding with
  its epic and task branches, which git rejects because refs are stored as files and a branch
  cannot also be a directory.
- `backlog/<topic>/epic-<n>` — one per epic, cut from the backlog branch, worked in the backlog's
  order.
- `backlog/<topic>/epic-<n>/task-<m>` — one per item, cut from the epic branch.

Work on the task branch, commit the item, and merge it back into the epic branch once it is ticked
off, then delete it. Merge the epic branch into the backlog branch once its summary is committed,
and the backlog branch into its target once the backlog and its plan are deleted. Merge with
`--no-ff` so every task and epic keeps its own point in the history.

## Working a backlog

Work one item at a time: cut the task branch, do the item, verify it, tick it off, commit it, merge
it back, then start the next. One commit per item, so a step that turns out wrong can be walked back
without unpicking the ones around it. Do not implement a whole epic in a single go — working ahead
of the list hides which step broke the tree and turns a review into one large diff.

After the last item of an epic lands, write a summary at
`docs/backlogs/completed/<topic>/epic-<n>-<title>.md`: what changed and what was verified, task by
task. Commit it on its own, merge the epic branch, and leave the summary in place — the summaries
stay after the backlog is deleted.

## Leaving

When the last item lands, write the final epic's summary, then delete the backlog and the plan it
breaks down. Commit the deletion on the backlog branch and merge it into the target; the branches
have done their job once they are merged, so delete them rather than leaving them behind. The
checked list is not the record — git is, and the commit messages say what the items did; the epic
summaries are the per-epic account.

Deleting a plan deletes any backlog that breaks it down: a dropped plan does not leave its tasklist
behind. If an effort is dropped part-way, the unticked items go with the file unless an item is
still worth doing; then it becomes a parking-lot line, where it is a sentence rather than a task.

## Nothing references a backlog

A backlog is deleted when its work is done, so a link, filename or section number in a spec, a
guide, a code comment or the project README becomes a dangling pointer. Write the requirement where
it will live instead.

A backlog may reference the plan it breaks down and the specs it implements, since those exist for
as long as it does. The reference runs one way: the backlog names the plan, never the reverse, and
the pair is deleted together. No spec or guide links to a backlog either.

`docs/README.md` is the one index, and it names a backlog while the file exists. Add the entry when
the document is created, and remove it when the document is deleted.
