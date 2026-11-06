# Should a link be able to press the panel's keys?

The serial line reaches the machine, the device's own document, the piezo and the file store. The
panel is reached only by its six keys. A controller can pull the chamber to a depth, read it back,
change the unit and restart the device, but it cannot open the settings list, push the browser,
close a page or say a word a routine defines for itself — and those are most of what the panel is
for. The queue the keys fill is already a seam: an interface the board fills, drained by the page
once a beat and by `device.take_gesture()` for a routine. The question is whether a link should be a
writer of that queue, and what a gesture means when it arrives over a wire rather than from a press.

## The options

**Share the queue.** The link reads one word off the line and posts it, whole, into the same queue
the keys fill. The word then reaches a page or a routine exactly as a press does, and nothing in the
panel can tell which it was. It is the cheapest shape by a wide margin — the queue is an interface
with a board-fillable implementation, the intern table already holds the lock a second task needs,
and the host tools already press the panel this way — and it makes the wire and the keys one
vocabulary rather than two. The cost is that everything the panel does *before* the queue is on the
link's side of it: the classifier's safety holds never see the word, and a gesture is an event
rather than a request, so a controller cannot be told what became of it.

**Feed the classifier.** The link presents synthetic button states to the panel's input path so the
classifier, the holds and the overrides apply in one place. It is the only option where a link's
press and a key press are processed by the same code, and it is the only one where a hold has its
meaning for free. But the reader under the classifier is a shift register's levels and timings, so a
synthetic press has to invent durations for keys — a press becomes a little state machine whose
period has to agree with the panel's beat — and it puts a second producer into the one path that is
deliberately the hardware's. Every safety hold then depends on two implementations agreeing, which
is the thing a safety hold is least able to afford.

**A page-command domain.** A second wire surface that names what a page should do — push the
browser, open settings, go home — rather than what key was pressed. A controller gets a vocabulary
that reads like an API instead of like a remote control. It costs a second contract for the same
pages: every gesture a page binds today needs an equivalent operation, the pages have to be written
against both, and the two drift the moment one is edited. The panel's own vocabulary is the gesture,
so this is a new surface in place of the existing one rather than an addition to it.

**Machine commands only.** Leave the panel local. A controller that wants a run says `vacuum start`
and reads `vacuum get`; the pages stay a human surface. Nothing is added and nothing can drift. What
it costs is everything above the machine: the settings list, the browser, a routine's own words and
the panel's two safety holds remain unreachable from the wire, and a device with no hands on it has
no way to change what it will do next.

## The constraints a gesture over the wire sits inside

- **A gesture is the page's word, not the wire's.** The link carries what it is given: a word a
  routine defines for itself is a word a page may take, and the link gives it no meaning and takes
  none away. `cmd:` is one of those forms — the routine's machine decides what follows it, and
  nothing on the link's side does.
- **A chord is buttons, in button order.** Every part of a `+`-joined word names one of the six
  buttons, and the parts stand in the order the buttons sit on the panel: up, down, forward, back,
  ok, cancel — the spelling [the walkthrough's `send`](../guides/walkthroughs.md) already takes.
  This is the layout's order rather than the order presses happened in, so it is stricter than the
  classifier, which can put a hold it has already issued first beside a later press and answer
  `ok:3+forward:single`. A word in any other order is one a link may not send.
- **`cmd:` and `system:` are whole words.** Neither can be part of a chord: a word beginning with
  either is the whole gesture and stands alone.
- **The safety holds belong to the link.** The panel's holds exist only as overrides inside its
  classifier, which a posted word never passes, so the two reserved words have to be recognised by
  the link itself and answered with the machine's commands — a stop and vent for the force-quit, a
  restart for the reset. Nothing downstream of the queue acts on them, and a link that forwards them
  unchanged has done nothing at all.
- **A gesture is not answered.** The wire has no acknowledgement, no refusal and no echo for one:
  a press is an event, and a controller learns what happened by reading the machine's state, which
  the wire already answers. A word that arrives while the panel cannot take it is lost without a
  sound.
- **The glass does not mark it.** A key press leaves the page's own answer and nothing else, and a
  link's press is the same; the marked buttons exist in the host tools' pictures, not on the device.

## What has to be true for sharing the queue

- **The link and the panel hold the same queue.** The composition already builds one, the queue is
  an interface so the board can fill it, and the board's implementation exists precisely for a
  second writing task. What is left is that the link is handed it.
- **The intern table is the device's own.** A word posted from the interface task interns it there,
  and the insert allocates, so the table that locks across the insert is not decoration; a build
  that forgets to install it is a race between two tasks pushing onto one vector.
- **The link's holds are the machine's half.** The panel's force-quit does two things — stops and
  vents, and asks for the root page — and only the first is a command. A link's force-quit can stop
  the machine but cannot send the page home, so the glass keeps the page it was on. That is a
  difference between a hand on the panel and a word over the wire, and it should be expected rather
  than discovered.
- **A word with no page showing has a meaning.** The queue waits while the stack is empty and the
  word is taken when a page comes up, and the panel's own force-quit clears whatever is waiting.
  Both are the queue's behaviour today; a link makes them reachable by someone who cannot see the
  glass.
- **The drain rate is the panel's, not the buffer's.** The queue is deep enough that a stopped drain
  is what fills it, and it refuses rather than drops what is already there. With nothing answering,
  that refusal is silent — so the panel's one gesture a beat is the pace a controller has to live
  inside, not a buffer it can fill.

## What is still unknown

- Whether the wire's gesture domain is one way to press and nothing else, or whether a link will
  want to ask which words a page takes. The pages define their words in Berry, and nothing publishes
  them to a link today.
- Whether a routine that reads `device.take_gesture()` and the page above it are the two readers a
  controller should be able to choose between, or whether a wire word should be the page's alone. A
  key has the same ambiguity and nobody minds, because a key press has a person behind it.
- Whether the Bluetooth link follows the same route. Its table is a second contract written for the
  machine's fields, and adding a gesture to it is either one more characteristic or a reason for
  that link to stop being a table at all. It is out of the build, so nothing here is urgent.

## Recommendation

Share the queue, and let the wire carry one gesture word per line with no reply. It is the only
option that makes the wire and the keys one vocabulary, it asks for nothing the panel does not
already do, and the two things it moves to the link — the safety holds and the pacing — are small
and finite: two words, and a drain rate that is already written down. The page-command domain is the
option to keep in reserve if a controller ever turns out to need to know what a page takes before it
presses; the classifier is the one to avoid, because it puts a second implementation of the safety
holds beside the hardware's.
