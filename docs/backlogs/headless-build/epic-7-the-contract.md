# Epic 7 — The contract

The specs catch up with what the firmware has done since Epic 4: the ownership
stack, the wire surface each build composes, and the safety rule every build
shares. One fact per item, each checkable against the code alone; reading the
specs against the implementation also turned up a good deal of text still
describing the one-owner record Epic 4 deleted.

## What changed

- **`control` is a stack on both links.** `control get`, `acquire` and `release`
  answer `serial`, `bluetooth`, `panel` or `none` — who is on top. There is no
  `busy`: a claim always succeeds and the link beneath is set aside. The BLE
  `control` characteristic carries the same four values as the `Link` enum.
- **A claim vents.** Taking the top vents the chamber, so a new controller
  starts from an open chamber; the connecting rules and the worked examples now
  say so, and a dropped link is removed rather than leaving a held record.
- **The panel owns its fields.** `fs` and `buzzer` are served only where a panel
  is fitted, and `batt_v`, `batt_pct`, `log`, `unit` and the BLE `batt` and
  `unit` characteristics exist only there too. A machine-alone `device get` reads
  `proto`, `fw`, `chip`, `heap` and `caps`.
- **Any change of the top vents.** The vacuum contract states the rule outright
  and the event table and drop bullet agree with it: a claim, a release or a
  drop all vent, so a vanished link cannot leave the machine held.
- **A machine with no panel holds only while owned.** Its stack is `none` alone,
  so an unowned machine is vented; a panel build differs only in being a
  controller of its own.
- **Back releases, Back+OK reboots.** Back takes the panel out of the stack,
  which vents, and acts only while the panel drives. Back+OK is the same safe
  reset the wire serves as `device reboot` and `command 0x04` — not a reset that
  clears a held record, because there is none to clear.
- **A build without keys has no gestures.** The gesture contract says so at the
  top: its stop is ownership alone.
- **The README names both builds.** What the full device and the headless build
  are for, and `--sketch` in the build section.

## Where the summary sits

Epics 1–4 keep their summaries beside the backlog at
`docs/backlogs/headless-build/`, so this one does too. The backlog skill names
`docs/backlogs/completed/<topic>/`; that directory does not exist and moving one
file alone would be worse than either convention.

## Verification

Documentation only, so the check is against the code rather than a build: the
reply grammar matches `ControlDomain`, the characteristic values match the
`Link` enum, the field lists match `MachineDeviceDomain` and the panel's
`DeviceDomain`, the gate is `ownedBy(Link::Panel)` in the panel's button beat,
and Back+OK reaches `safeReboot` without consulting ownership. The behavioural
pass is Epic 8.
