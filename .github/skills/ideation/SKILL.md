---
name: ideation
description: 'Ideation documents in docs/ideas/: open exploration of a possibility before it has earned a requirement — the options, the trade-offs, and the constraint that set each aside. An idea document is not a plan, and it is deleted once a plan or a spec carries what it established. Use when brainstorming a direction, writing up or reviewing an idea, or deciding whether what you have is an idea, a plan, a backlog or a parking-lot line.'
---

# Ideation

## When to use

- A direction needs weighing before anyone commits to it
- Writing, extending or deleting a document in `docs/ideas/`
- Deciding whether what you have is an idea, a plan, a backlog or a parking-lot line
- Reviewing an idea document

## What an ideation document is

The record of opening a question rather than answering it. It is written while the shape of the
thing is still in doubt, so it holds the alternatives that were seriously considered, what each
would cost, and which constraint ruled each one out. It ends with what the author thinks and what
is still unknown — not with a decision that binds anyone.

It lives in `docs/ideas/`, one file per question, kebab-case, titled with the question it opens
(`second-chamber.md`, not `vacuum-thoughts.md`). A page is a normal length; an idea that needs five
has usually turned into a plan without being called one.

## What it is not

| | An ideation document | A plan | A backlog |
| --- | --- | --- | --- |
| Holds | options and trade-offs | a requirement: what has to be true | an ordered tasklist for decided work |
| Commits to | nothing | the change it proposes | the steps it lists |
| Leaves by | deletion | deletion | deletion |

**A plan is a requirement, not a design.** Once the question is settled, write the requirement into
a plan — or straight into a spec if it is small enough. An idea document that starts collecting
acceptance criteria, steps or a file list has become a plan; stop and write the plan.

**A backlog is a tasklist, not a decision.** It breaks decided work into ordered steps, so an idea
does not turn into one directly: the requirement lands in a plan or a spec first, and the backlog
breaks that down. An idea document that starts listing tasks has skipped the step where the work was
decided.

**A parking-lot entry is a note, not a plan.** An idea small enough to state in a sentence does not
earn a document: put that sentence and what is blocking it in `docs/plans/parking-lot.md` and stop
there. An idea document is for a possibility with more than one live option.

**A spec says what the firmware does now.** The requirement goes in the spec itself; a spec never
depends on an idea document to be understood.

## Lifecycle

An idea document is deleted once a plan or a spec carries what it established, the same way a plan
is deleted once it is implemented. The thinking is not the artifact that survives; the decision is.

If an idea stalls with no decision and nobody picking it up, shrink it to a parking-lot line and
delete the document. An idea nobody reads and nobody acts on is noise.

## What goes in one

- The question in the first paragraph: what is being weighed, and why now.
- Each option in enough detail to argue about — what it does, what it costs, what it makes harder.
- What ruled an option out, named as the constraint rather than as a preference.
- What would have to be true for the preferred option to work, and what is still unknown.
- The recommendation, if there is one, and how strongly it is held.

Leave out the parts of a plan: no steps, no acceptance criteria, no file list, no estimate. Leave
out detail the idea has not earned as well — a diagram of a part nobody has chosen, a module map
for code that does not exist. The document records the thinking that happened, not a rehearsal of
the work.

## Nothing references an idea document

An idea document disappears the moment a plan or spec carries it, so a link, a filename or a
section number in a spec, a guide, a backlog, a code comment, a script header or the project README
becomes a dangling pointer. Specs and guides stand alone, and code comments say what is true rather
than what was once considered — a plan written from an idea states its own requirement and does not
cite the idea.

An idea may reference a spec or a guide, because those outlive it. The reverse never happens.
