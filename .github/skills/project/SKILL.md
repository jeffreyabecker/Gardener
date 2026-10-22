---
name: project
description: 'Gardener Git and documentation practice: how to commit (one commit, the whole tree, the subject-and-body message style), and which document a requirement, a proposal, an idea or a tasklist belongs in. Use when asked to commit, when editing the project README, or when deciding where a note about work belongs.'
---

# Project

## When to use

- Asked to commit
- Editing the project `README.md` or `docs/README.md`
- Deciding where a piece of work belongs before writing anything down

## Committing

**Committing means commit.** When you are told to commit, it is `git status --short`, `git add -A`
and one `git commit`: the whole tree as it stands, in the message style below. That is the task, and
it should take the time three commands take.

**Look at the tree before you describe it.** `git status --short` comes first because it answers what
the commit is: what is staged, what is not, and whether the work you meant to describe is still there.
The tree moves between calls — work lands from another terminal, a commit from another session — so
the index you saw a minute ago is no evidence of the one in front of you. Open a diff only when the
message needs it to name the change, and stop at what names it. When a diff comes back empty where you
expected files, `status` is the question to ask; repeating the same query, quoted or narrowed, only
says nothing twice.

**A commit runs nothing.** No build, no test, no lint, no file-by-file read of the diff to satisfy
yourself the tree is good, and no check of whether someone else's unstaged work compiles. Whatever is
in the tree was verified when it was made; verifying it again belongs to the change, not to the
commit. If the tree holds work you did not write, that is still the tree — stage it and describe it
in the message.

Don't split one request into a tidier series of commits, and never revert files to rebuild an
intermediate state so that a commit looks smaller. Don't amend a commit that already landed.

**The message has a shape.** The subject is one imperative sentence, capitalised, with no scope,
ticket or type prefix and no trailing full stop, and it stays under about 65 characters. Then a
blank line and a body of plain paragraphs wrapped at 100 columns: what was there before, what
replaces it and why, naming the files, types and behaviours it touches. No bullets, no headings, no
backticks, and no summary, test-plan or checklist sections. End with the history's trailer,
`Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.

## Where work goes

Four documents, four jobs. Each one leaves when its job is done, so none of them is a record — git
is.

| Document | Location | Holds | Leaves by |
| --- | --- | --- | --- |
| Spec | `docs/specs/` | what the firmware and its tools do now | being rewritten, or dropped with the behaviour |
| Plan | `docs/plans/` | a proposal: what has to be true | deletion, its requirement carried into a spec |
| Idea | `docs/ideas/` | options being weighed before anything is required | deletion, carried by a plan or a spec |
| Backlog | `docs/backlogs/` | the tasklist breakdown of a plan, in order | deletion, with its plan, when the last item lands |

A parking-lot line in `docs/plans/parking-lot.md` is the fifth thing and the smallest: a sentence
naming a deferred idea and what is blocking it, for work too small to earn any of the four.

The skill for each has the detail: `.github/skills/specs/`, `.github/skills/plans/`,
`.github/skills/ideation/`, `.github/skills/backlog/`.

`docs/README.md` indexes specs, guides and backlogs; plans and ideas have no entry, so they are
found by name in their folder. Everything else — specs, guides, code comments,
script headers, the project README — stands alone and links to a spec or a guide. Plans, idea
documents and backlogs disappear when their work lands, so nothing cites them; the one exception is
a backlog naming the plan it breaks down, since the two are deleted together.
