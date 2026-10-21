---
name: specs
description: 'Specifications in docs/specs/: the contract for what the firmware and its tools do now — states, transitions, wire formats, limits — written to stand on its own. Use when writing or changing a spec, deciding whether a requirement belongs in a spec or somewhere else, or checking that a spec does not point at a plan, an idea or a backlog.'
---

# Specs

## When to use

- Writing, changing or reviewing anything in `docs/specs/`
- A requirement has settled and needs a home
- Checking that a spec still describes what the firmware does

## A spec is what is true now

`docs/specs/` is the contract the firmware and its tools satisfy today. Someone implementing against
it, or testing against it, needs nothing else: the states, the transitions, the wire format, the
limits and the errors are all in the file.

A spec is not a proposal and not a design. It says what is true, not what was considered, what will
change, or which file to edit.

## What goes in one

- Observable behaviour: states, transitions, what each reply contains, what an error means.
- Numbers with their units and the constant that holds them, so the contract and the code can be
  compared.
- The rule, and the reason where the reason is a constraint rather than a preference.
- What a caller must not do, when the firmware relies on it — a page leaving a reserved gesture
  alone, one owner of the link.

## What stays out

- **History.** What it used to do, what the previous version did, which bug prompted the rule.
- **A plan.** Steps, phases, sequencing and dates belong to a plan; a settled requirement is written
  here whole.
- **A backlog.** Which task changes which line is the implementer's business.
- **An idea.** Options that were weighed and dropped are not part of a contract.
- **Code walkthrough.** A spec names behaviour; it does not tour the file that implements it.

## Stands alone

A spec links to another spec or to a guide, and to nothing else. It never links to a plan, an idea
document or a backlog, because each of those is deleted when its work lands and the link would rot.
Write the requirement into the spec itself.

The dependency only runs one way: a plan or a backlog may reference a spec, because the proposal
depends on the contract it changes. Never the reverse.

`docs/guides/` is the other side of it: user-facing build and parts documentation, telling someone
how to assemble and run the thing. A spec says what has to be true; a guide says what to do.

## Shape

One file per area, kebab-case, titled with the thing it is the contract for: `serial-protocol.md`,
`vacuum-control.md`, `gestures.md`. The routine surface has its own folder, `docs/specs/ui/`,
because it is a contract between the firmware and the routines the panel runs.

Open with a paragraph saying what the file is the contract for and where its edges are — what
belongs to it and what belongs to the file next door. Then say it plainly: tables for states and
formats, numbered rules where order matters, prose where a reader needs the reason.

`docs/README.md` indexes every spec with a one-line summary. Add the line with the file and change it
when the file's subject changes; the index is how a reader finds the contract without the code.
