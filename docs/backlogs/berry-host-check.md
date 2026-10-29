# A working Berry host check

The breakdown of [berry-host-check](../plans/berry-host-check.md) from where the repair stands:
`tools/berry/check.py` compiles the three libraries it is made of, links, loads the image and reports
`9 of 9 files loaded`, and no registered page activates yet, so a plain run reports `0 of 3 routines
driven` and fails. The plan's compile, source-discovery, count and module-binding sections are done;
what is left is the activation bug and the suites that say the check holds the image.

## An image whose pages activate

- [x] Make the clean settings map a page's init is handed subscriptable, so `routines.activate`
      answers a page for every registered key and a plain run reports `3 of 3 routines driven` and
      exits 0.

## The suites name what the image has

- [x] Declare the blank page the machine suites push above the root once, in `tests/suite.be`, so
      `Scene()` is a page the framework declares and not a name five cases share.
- [x] Read the machine suites' clock through `device.now_ms()` and set their pressure through
      `vacuum.set_target_pressure()`, so no suite names a call the image does not have.
- [x] Assert the three page suites' drawing through `draw_page()` and the nodes it answers, in place
      of the `chrome` fields no page keeps.
- [x] Give the two chrome builders one clock unit and correct the home case that hands 65 000 to a
      builder taking seconds, so a 65-second run draws the string `duration_text` answers for 65
      seconds whichever page shows it.
- [x] Start the home machine's runs from a target the case places, the way the interval's machine
      suite does, so a home run does not lean on a stored target the seed does not carry.
- [x] Assert that the home page's Up-double pushes the browser the image opens from home, in place of
      the settings list the case still expects.

## The suites the image has to answer for

- [x] Hold a browser suite: the rows the registry answers, the selected row inverted and no other,
      the first row the window shows once the selection passes the bottom edge, OK launching the
      record's page, Forward opening the settings list and launching on its answer, and Back leaving.
- [x] Hold an interval page suite: the settings set it declares, the chrome it inherits from the ramp
      and the flat ladder the ramp refuses and it does not.
- [x] Hold a `screen` suite: a node of the kind it names with its fields filled, the ink `measure`
      answers, `duration_text`'s string and `--` for a clock that is not running, and `icon` refusing
      a string that is not a bitmap.
- [x] Hold a `settings` suite: a `RoutineSetting` per declaration with the bounds it was declared
      with, a declaration consumed once so a second record taking the same row fails, and a raise
      that leaves the first record's rows untouched.
- [x] Hold a `routines` suite: keys, rows, a nil title for an unknown key, nil `settings_cfgs` for a
      record that declares nothing, an activation that fills a missing setting with its default, and
      a save that writes each key once.
- [ ] Hold a `ux` suite: the three calls' answers, `depth` from 1 at boot to the stack's cap, `top`
      as the page instance the image built, a handle pushed twice refused, the callback handed the
      popped page's value, and no callback from `pop_to_root`.
- [ ] Hold the frame-ink cases on `harness.ink`: an inverted leaf over a full cell, a child that
      names no size filling its cell, and a text node whose ink is the width `measure` answers.

## The tool is documented as it is

- [ ] Restore the harness notes in `.github/skills/berry/SKILL.md`, drop "the Berry check is in a
      known broken state" from the skill and the project instructions, and correct the last line to
      the counts a run prints.
- [ ] Run the commands the plan names — `--compile`, a plain run, `--tests`, `--art` and `--scene` —
      and have each do what it says, `--component` refused with the flag gone.
