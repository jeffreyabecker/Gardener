---
name: project
description: 'Gardener Git and documentation practice: how to commit (one commit, the whole tree, the history style), and which document a requirement, a proposal, an idea or a tasklist belongs in. Use when asked to commit, when editing the project README, or when deciding where a note about work belongs.'
---

# Project

## When to use

- Asked to commit
- Editing the project `README.md` or `docs/README.md`
- Deciding where a piece of work belongs before writing anything down

## Committing

**Committing means commit.** When you are told to commit, stage what is in the tree and write the
message: one commit, the change as it stands, in the style the history already uses. Don't re-run
what was verified when the change was made, don't split one request into a tidier series of commits,
and never revert files to rebuild an intermediate state so that a commit looks smaller. Verification
belongs to the change, not to the commit.

## Where work goes

Four documents, four jobs. Each one leaves when its job is done, so none of them is a record — git
is.

| Document | Location | Holds | Leaves by |
| --- | --- | --- | --- |
| Spec | `docs/specs/` | what the firmware and its tools do now | being rewritten, or dropped with the behaviour |
| Plan | `docs/plans/` | a proposal: what has to be true | deletion, its requirement carried into a spec |
| Idea | `docs/ideas/` | options being weighed before anything is required | deletion, carried by a plan or a spec |
| Backlog | `docs/backlogs/` | a fine-grained tasklist for decided work | deletion, when the last item lands |

A parking-lot line in `docs/plans/parking-lot.md` is the fifth thing and the smallest: a sentence
naming a deferred idea and what is blocking it, for work too small to earn any of the four.

The skill for each has the detail: `.github/skills/specs/`, `.github/skills/plans/`,
`.github/skills/ideation/`, `.github/skills/backlog/`.

`docs/README.md` is the one index of all of them. Everything else — specs, guides, code comments,
script headers, the project README — stands alone and links to a spec or a guide, never to a plan, an
idea or a backlog, because those disappear.
