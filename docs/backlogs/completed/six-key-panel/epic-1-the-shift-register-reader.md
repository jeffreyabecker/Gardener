# Epic 1 — The shift-register reader

The panel's six keys are named and read through one 74HC165. This is part 1 of
[the six-key panel plan](../../../plans/six-key-panel.md); the gesture map and the gesture string
follow in epics 2 and 3.

## Task 1 — Name the six keys

`Button` is declared in panel order — up, down, forward, back, okay, cancel — with `ButtonCount`
replacing the literal four the classifier spelled in its arrays and loops. `buttonName()` answers
all six tokens, and the panel task's press order lists all six.

Verified: `python scripts/arduino.py --task build` and `python tools/berry/check.py --tests`, with
the suites unchanged at 20 passed. The two new keys never press at this point, because the reader
still samples four pins.

## Task 2 — Read the keys through the 74HC165

`hal/impl/Buttons` is a shift register reader: latch on GPIO 10, clock on 11 and data on 1, with
the two spare register inputs tied high. The interrupt capture, the ISR, the packed mask it wrote
and the per-pin `digitalRead` all went; `readKeys()` latches the eight inputs and clocks six out on
every settle. The debounce, the double window, the hold measurement and the key-held-at-boot
exclusion are the same code fed from a shift instead of a pin.

`PanelPins` carries the three lines where it carried four key pins, the sketch's `Board` block
names them, and the parts guide and the alpha board's button row describe six switches and one
register instead of the bought five-key module. GPIO 2 is spare again.

Verified: `python scripts/arduino.py --task build` — 873,045 bytes of program storage, 30,004 bytes
of globals. The harness does not compile the reader, so it says nothing about this task.

**Not yet done:** the bench check. The plan's completion for this task is that Okay starts, Back
stops and vents, and Up+Down opens the settings on the wired rig, with no key reading another's bit.
That needs the register wired, which has not happened here.
