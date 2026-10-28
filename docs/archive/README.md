# Archive

`specs-and-plans.zip` holds every spec and plan this project wrote before the panel port, as they
stood when the port outran them. They describe classes the tree no longer has — the component
model, the scene lifecycle, the settings kinds — and several of their links still point at the
folders they used to live in.

Nothing here is read when implementing, and no work is checked against it: what is true is the code
and the [native panel backlog](../backlogs/native-panel.md). A document comes back by being
rewritten against the tree and landing in `docs/specs/` or `docs/plans/`, not by being unzipped, so
a file lifted out of it is a starting point rather than a contract.

To look something up:

```powershell
Expand-Archive docs/archive/specs-and-plans.zip -DestinationPath $env:TEMP/specs-and-plans
```

The machine's own specs — the wire and the machine's protections — are in there and are the first
worth bringing back, since the port did not change them.
