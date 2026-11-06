# Link presses

The steps that make a link a writer of the panel's gesture queue: one serial domain whose whole body
is a gesture word, posted where the keys post, and the panel's two safety words answered by the link
because a posted word never passes the classifier that names them. A gesture is never answered — the
word goes in and nothing comes back — and a word the grammar will not take is dropped without a
sound. Each item is one branch, one commit and one diff, and leaves the tree building with the suite
green; the sketch is compiled on the item that changes it, which the suite cannot reach. Branches
are `backlog/link-presses/main`, `backlog/link-presses/epic-<n>/main` and
`backlog/link-presses/epic-<n>/task-<m>`, merged back with `--no-ff` as each item lands.

## Epic 1 — the word

- [ ] **Serve a gesture domain over the serial line.** `serial/SerialGesture.cpp` beside the four
      grammars, reached from `SerialLink`'s token list and its dispatcher as `gesture`; the body is
      read to the end of the line and posted whole to a `GestureQueue&` that `SerialParts` now
      carries. The sketch passes `root.routines().presses()`, and the four benches that build a
      `SerialParts` — `link.cpp`, `device_commands.cpp`, `vacuum_commands.cpp` and
      `fs_commands.cpp` — pass a `GestureList`. A body over the line maximum is `err bad_argument`,
      as it is for every grammar, and a word that is taken prints nothing at all. Cases drive the
      link and take the words back off the queue in the order they were sent, with the stream empty;
      the sketch line is checked with the sketch build.

- [ ] **Accept only a word a link may send.** The grammar keeps a `+`-joined chord to button parts
      in the order the buttons sit on the panel — up, down, forward, back, ok, cancel — each spelled
      `single`, `double` or whole seconds. `cmd:` is one whole word with a name after it, carried
      unchanged and never part of a chord; `system:` is one of the panel's two safety words, whole,
      and never part of a chord. A word it will not take is dropped: nothing posted and nothing
      printed. Cases cover an in-order chord, an out-of-order one, a repeated button, an unknown
      button, an unknown form, a `cmd:` word alone, either word inside a chord, and an unknown
      `system:` word.

## Epic 2 — the safety words

- [ ] **Answer the safety words with the machine's commands.** A word that is `system:reboot` posts
      `MachineCommand::Op::Reboot` to the command queue and not to the gesture queue; `system:quit`
      posts `MachineCommand::Op::Stop` — the stop and vent the panel's own hold makes — and leaves
      the page on the glass, with a comment recording that sending the screen home is still to do.
      Cases give the link each word and find the command in the command queue and nothing in the
      gesture queue.

When the last item lands, delete this file.
