# Link control

Status: proposed. Nothing here is implemented. The firmware today treats a serial connection as
ownership, while the BLE design treats a connection as a viewer; this replaces both with one
explicit lifecycle for every transport.

## 1. The lifecycle

Every link — USB serial, BLE, and anything added later — moves through the same four steps.

| Step | What it means | What changes |
| --- | --- | --- |
| **connect** | the transport is up | the link may read; the machine and the panel do not change |
| **assume control** | this link becomes the one controller | the panel switches to `ConnectedUI`; state-changing commands are accepted |
| **command** | the controller drives the machine | the existing domains and opcodes, but only while controlling |
| **release control** | the link gives ownership back | stop and vent, then the panel returns to `DisconnectedUI` |

Connecting is not controlling. A link that only connects can watch; it cannot change the machine
and it does not take the panel. A link that assumes control owns the device until it releases it.
An involuntary drop does not release it: ownership stays held until the local reset gesture.

## 2. Safety rules

* **One controller at a time, across all links.** A second `acquire` while another link owns the
  device is refused as busy. There is no preemption.
* **Release is the safe path.** It is the same behaviour as `vacuum stop`: pump off, valve open,
  vent. Giving up control without releasing the chamber is not an option.
* **An involuntary drop does nothing.** The machine keeps its state, the pump keeps running, and
  the ownership record stays held. The device does not stop, vent, release or reboot on its own.
* **Recovery is local.** Back held for 3 s stops and vents; Back+OK for 5 s resets the device and
  clears ownership (§6). Until that reset, a reconnected link is a read-only observer.
* **A deliberate release is the graceful exit.** `control release` stops and vents, clears
  ownership, and does not reboot.
* **Link loss while only connected does nothing.** There is no session to recover.
* **The local controls are never gated by ownership.** The Back hold still stops and vents, and
  the reset gesture below works whatever a controller is doing.
* **A controller that stays connected but stops responding keeps control.** There is no idle
  lease in this plan; the local reset gesture is the way out (see §6).

Read-only verbs — `device get`, `vacuum get`, `buzzer get`, `fs list`, `fs read` — stay available
while connected. Everything that changes state — `device set`, `device reboot`, and every writing
`vacuum`, `buzzer` or `fs` verb — is refused with `err not_controlling` until the link assumes
control.

## 3. The serial wire

A new `control` domain carries ownership. The protocol is already unsettled, so this is a breaking
change and `proto` stays `1` for now.

| Verb | Reply | Notes |
| --- | --- | --- |
| `control get` | `ok control=none` / `ok control=owned` | the caller's view of ownership |
| `control acquire` | `ok control=owned` / `err busy` | idempotent for the current owner |
| `control release` | `ok control=none` | idempotent; releases and vents when owned |

The `not_controlling` code joins the error table for every gated verb. A connected link may use
`control get`, `control acquire`, `control release` and the read verbs; everything else is gated.

`device reboot` uses the safe reboot path in §6 rather than a bare `ESP.restart()`, so a controller
cannot skip the release on its way out.

`control release` is the graceful exit. A link that disappears while owning cannot send it, so the
machine and the ownership record stay as they are; a reconnected link can read, but cannot acquire,
until the local reset gesture.

A plain UART cannot report presence, so it cannot detect a dropped link. It acquires exactly like
USB CDC and must release explicitly; a controller that disappears leaves ownership held, and the
reset gesture is the way out.

## 4. BLE

BLE gets the same lifecycle:

* A connection alone is read-only. The panel stays where it is.
* A `control` characteristic is the toggle: read, write, notify, encrypted, `uint8` with `0` for
  not owned by this link and `1` for owned by this link. Writing `1` acquires; writing `0` releases.
  It is the same state the serial `control acquire` and `control release` verbs move.
* A disconnect while owning does nothing: no stop, no vent, no reboot, and ownership stays held.
  An explicit write of `0` to `control` is the graceful release. A disconnect while only connected
  changes nothing. A reconnected central reads `0` and its acquire is refused as busy until the
  local reset gesture.
* The existing `command` characteristic and its result codes stay for machine actions; `control`
  is session state, not an opcode.

The GATT table and the screen rules in [ble-control.md](./ble-control.md) follow this document.

## 5. The panel

* `ConnectedUI` is the screen for **ownership**, not attachment.
* `DisconnectedUI` is the screen whenever no link owns the device, even if one or more links are
  connected. The overlay's serial and Bluetooth indicators still show presence, because they
  answer a different question.
* Acquiring control does not change the machine. A controller that takes over a running chamber
  reads `vacuum get` before commanding it.
* Releasing control restores the disconnected page and its routine. A routine that was running is
  paused while the page is not showing, exactly as it is today.
* Losing control does not change the page: a dropped controller leaves `ConnectedUI` showing a
  machine that is still running, which is the truth. The local reset gesture clears ownership and
  returns the disconnected page.
* A connection or disconnection that does not change ownership does not repaint the panel.

## 6. The universal reboot gesture

**Hold Back and OK together for 5 s.** This is the local reset gesture, and it works from every
page, in every link state, with or without a controller.

* At the mark it performs the release: stop the pump, open the valve, and wait for `VENT` to finish
  or 2 s, whichever comes first. Then it calls `ESP.restart()`.
* It cannot be suppressed by a page, a routine, or a controller.
* The existing Back 3 s hold stops and vents, and on the disconnected pages the OK 3 s hold stops,
  vents and shows `default`, so either may fire first. That is harmless: the chamber is already on
  the release path when the 5 s mark arrives.
* The same safe reboot is what `device reboot` and the BLE reboot opcode run, so there is one
  definition of "safe reboot". It is also the reset that clears a stale ownership after a dropped
  link.
* The restart clears ownership, so the device comes back with no controller, like a fresh boot.
* If the firmware loop itself is wedged, no gesture can run. The hardware reset and the normally
  open valve remain the last resort.

## 7. What this changes in the code

* One ownership record in `AppContext`, built by the composition root, is the source of truth.
  There is no per-transport "connected means controlling" flag to keep in step.
* The serial domains ask that record before running a gated verb; `App::update()` chooses the page
  from it instead of from `serialAttached()`.
* `BleLink` uses the same record; a disconnect leaves it held rather than clearing it.
* The Back+OK hold lives with the other reserved holds in `App`, not in a page or a routine.
* `device reboot` and the BLE reboot action share the safe reboot routine.

## 8. Order of work

1. Ownership record, the `control` domain, gating, and the panel following ownership.
2. The universal reboot gesture and the shared safe reboot path.
3. The BLE control characteristic and disconnect-leaves-ownership-held.
4. Update [serial-protocol.md](../specs/serial-protocol.md),
   [gestures.md](../specs/gestures.md) and [ble-control.md](./ble-control.md) to the implemented
   contract, with the new error and test cases.

## 9. Not in this plan

* No preemption and no second controller.
* No idle lease or timeout; a drop leaves ownership held, and the local reset gesture is the only
  way out.
* No change to the vacuum control model, the local routine API, or the display unit rules.
