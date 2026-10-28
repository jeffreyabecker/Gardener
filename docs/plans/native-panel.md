# The native panel

The panel's scene stack, value editors and settings list are C++. The pages are not: `/system` still
loads six Berry files, five of which name classes the tree no longer has — the component model, the
scene lifecycle and the settings kinds — so the pages they build cannot load, and the C++ record a
routine registers, with the store its set is read from and written to, is reached by nothing in the
firmware. This plan states what has to be true when every page the panel shows is drawn by the scene
stack. The machine, the wire and the drawing library are already native and are left alone.

## The device's own settings are edited in the native list

Now: the home page is a Berry scene, and it maps no gesture to the settings list any more — its two
double presses went with the pages they opened. The three keys the device's document holds under
`[device]` — the unit, the target and the jingle — are read one at a time by whoever needs one.

Instead: Up held to a double on the home page pushes the native settings list, one row per key, in
the order the panel shows them: the unit a choice over the units the panel prints, the target a
pressure row over the machine's bounds, and the jingle a choice over the entries
`/cfg/jingles.ini` carries, whose Forward sounds the entry it is showing. A value
`/cfg/settings.ini` carries is the value its row opens on, a key it does not carry starts at that
kind's default, and OK writes the settled set back through the storage layer, leaving a key this
revision does not name as it found it. Cancel leaves the document as it was read. The keys keep
their names and their section: `unit`, `target` and `jingle` under `[device]`.

Checked: a host case over a fixture document answers the three rows and opens a key the fixture
omits at its default; on the device, a double-Up, an edit and OK leave the new value in
`/cfg/settings.ini`, and Cancel leaves the file as it was.

## The home page is the root the stack draws

Now: `home.be` and `home_machine.be` are the home page, and `/system/order` names them.

Instead: the root the booted panel draws is a scene the stack holds, and the mark is what draws for
an image that names no root. Neither file is in the image or in the order, and nothing a page does
changes: OK starts or resumes a run, Cancel stops and vents, and the page shows the target the
device stores, the measured pressure and the run's elapsed time.

Checked: the order names neither file, the panel boots to the home page on an image they left
behind, and an image that names no root draws the mark.

## The browser is a page the stack draws, and it launches

Now: `browser.be` is the browser. It names `routines` and `SettingsEditScene`, and neither survives.

Instead: the browser's rows are the routines the registry holds, in its order, one row per key with
an alternate following the record it belongs to, each carrying the title and the glyph the record
gives it. Up and Down move with the selection inverted. Forward opens the selected routine's
settings list, built from the rows its record declares and opened on the set the registry reads for
that key. OK launches the routine on the set that list settled.

Checked: a host case drives the rows, opens a routine's list, edits one setting and reads the value
the launch started on; on the device, the browser lists what the registry holds and a launch opens
on the settled set.

## A routine's page draws its own record

Now: the title and the settings a routine's page shows are written in the `.be` files that build the
page, and the record the routine registered is drawn nowhere.

Instead: the title and the settings a routine's page shows are read from the registry's entry for
the routine, so the page and the declaration cannot disagree about an order or a value, and a
setting the record does not declare is not on the page.

Checked: a page built from an entry prints the title and the settings the record holds.

## The panel's contracts are written down

Now: `docs/specs/` holds nothing. The documents written before the panel port are archived, out of
date, and the archived component spec names classes the tree no longer has; what a page is given and
what a call draws are written only in the headers.

Instead: a spec of the drawing verbs — the box a call takes, the composed verbs and the distribution
functions — that names no component class, and the routine, scene and binding contracts stated as
the tree has them, so a page can be written from the documents rather than from the headers.

Checked: neither file names a class or a file the tree no longer has.

## Consequences

- The image: `browser.be`, `home.be` and `home_machine.be` leave `/system/order`, so the routines
  that name them lose the last pages the image draws for itself.
- A page's drawing stops being documented by `ui/Drawing.h`, and the archived component spec is not
  the answer: the new spec is what a page is written against.
- Nothing on the wire changes, and no document on the device changes shape: the three keys keep
  their names, their kinds and their place in `[device]`, and a routine's set stays in its own
  document.

## Deferred

Two lines in [docs/parking-lot.md](../parking-lot.md) carry what this plan leaves out: a spec for
the image's check, which waits for that check to run again, and a one-file compile, which waits on
whether the firmware's own host check answers the same question.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the panel and packs the image.
2. `python tools/gfx/check.py` passes, with the cases the three page requirements name.
3. On the device, with `--task upload` and `--task data`: the boot log has no missing-global line,
   a double-Up on home opens the settings list, the browser launches a routine on the set its list
   settled, and `/cfg/settings.ini` holds what the list wrote.
