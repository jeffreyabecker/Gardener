# Gardener

An ESP32-S3 vacuum pump controller. It pulls a chamber to a target depth below ambient, holds it,
then vents it — driven over a serial line and a web page.

The serial line and the WiFi page are the ways in. A controller sets a depth and the machine pulls
to it; a vent releases the chamber; a reset clears a fault. What the device owns is what has to
happen when nobody is watching: holding the target, and venting the chamber through one path on
stop, abort or fault. Pressures are kPa below ambient, on both wires and in the firmware.

The device is the machine — pump, sensor and vent valve. One sketch,
[firmware/gardener](./firmware/gardener), composes the libraries for it: what it names is what the
device has, over the serial link and the web link it builds.

## Wire

Four verbs for the machine on a line ending in LF, and one word for the network:

| command | answer |
| --- | --- |
| `get` | `ok pressure_kpa=<x> target=<y> state=<word> duty=<z> fault=<code\|none>` |
| `set <kPa\|->` | `ok target=<kPa> state=PULL`, or `ok target=0.0 state=VENT` for the ambient target |
| `vent` | `ok state=VENT` |
| `reset` | `ok state=IDLE` — clears a latched fault |
| `wifi` | `ok ssid=<name> mode=<sta\|ap\|ap+sta> ip=<address> keep_ap=<0\|1>` |
| `wifi scan` | `ok count=<n>`, then one line per network: `ssid0=<name> rssi0=<dBm>` |
| `wifi ssid <name>` | `ok ssid=<name>` — the rest of the line is the name, and it is stored |
| `wifi pass <password>` | `ok` — the rest of the line, stored and never repeated |
| `wifi keep <on\|off>` | `ok keep_ap=<0\|1>` |
| `wifi join` | the status line, after the stored network is applied |
| `wifi forget` | the status line, after the stored network is cleared |

A set is a target and a run in one command, and the run is granted the machine's own ceiling
(two hours). Zero is the ambient target, which releases the chamber, so `set 0` and `set -` vent.
A target is refused with `err out_of_range` past the fitted floor, and with `err not_ready` when
the machine has no reading or a latched fault. The boot and run lines the firmware logs share the
same stream, framed as `[I]` and `[W]`, so a controller reads the `ok` and `err` lines.

The network is set a field at a time, and `wifi join` is what applies what is stored; a join with
nothing stored is `err no_network`. Each field is written to the board's filesystem as it is given,
so the choice outlives a power cut. A value is written bare while it reads back as one token and
quoted when it cannot be — `ssid3="Magic Shop"` — with `\"` and `\\` inside, and an argument may be
quoted on the way in, so `wifi ssid My Net` and `wifi ssid "My Net"` name the same network. A scan
answers nothing while the radio is joining, and its list is one line per network because the port
cannot carry a street's worth of names in one write.

Every five seconds, while a controller has the port open, the link also writes a heartbeat
unprompted — `hb pressure_kpa=<x> target=<y> state=<word> duty=<z> fault=<code|none>`, the
`get` fields without the `ok` — so a controller can tell a quiet machine from a dead one
without polling. Its own word keeps it clear of the replies a controller is waiting for.

## Web

The sketch joins a WiFi network, or runs its own setup access point when it has none to join, and
serves a page tree at `/` and the API the page drives over the same four verbs:

| call | result |
| --- | --- |
| `GET /api/status` | `ok=1&pressure_kpa=<x>&target=<y>&state=<word>&duty=<z>&fault=<code\|none>` |
| `POST /api/set` with `target=<kPa\|->` | `ok=1&target=<kPa>&state=PULL`, or `ok=1&target=0.0&state=VENT` |
| `POST /api/vent` | `ok=1&state=VENT` |
| `POST /api/reset` | `ok=1&state=IDLE` |
| `GET /api/wifi` | `ok=1&ssid=<name>&mode=<sta\|ap\|ap+sta>&ip=<address>&keep_ap=<0\|1>` |
| `GET /api/wifi/scan` | `ok=1&count=<n>&ssid0=<name>&rssi0=<dBm>…` |
| `POST /api/wifi` with `ssid`, `pass`, `keep_ap` | `ok=1` |
| `POST /api/wifi/forget` | `ok=1` |

The same network is set from the wire — `wifi ssid`, `wifi pass`, `wifi keep`, `wifi join` and
`wifi forget` in the section above — over the one radio and the one stored choice.

Requests and results are both `application/x-www-form-urlencoded`; there is no JSON anywhere. A
refusal is `ok=0&err=<code>` with the serial wire's codes (`out_of_range`, `not_ready`) and a
`reason`, `value`, `min` or `max` beside it. The HTTP status is 200 for a call the machine took,
400 for a refused argument and 409 for `not_ready`.

On first boot there is no stored network, so the board starts an open access point named
`Gardener-<suffix>` and a captive portal; joining it opens the page. Scan, pick a network, enter its
password and save. The board joins it, and unless **keep the setup network on** is ticked the access
point closes once the join succeeds. **Forget** clears the stored network and brings the portal
back. The serial log prints the portal name, the join attempts and the address the board ends up
on. The page and the API are unauthenticated on whatever network the board joins, the same trust
the serial line carries.

The board answers mDNS on whichever interface is up, under the setup network's name in lower case:
`http://gardener-<suffix>.local/` is the page, and the API is advertised as `_gardener._tcp` on the
same port with an `api=/api` TXT record, beside `_http._tcp` for the page. A client that joined the
access point can use the name as well as the address, so the page is reachable before any address
is known.

The page is a file tree on the board, not a string in the firmware: everything under
[firmware/gardener/data/www](./firmware/gardener/data/www) becomes the LittleFS image that
`--task data` builds, and a `GET` is the matching path inside it — `/` and `/dir/` serve that
directory's `index.html`, `/page.css` serves the stylesheet beside it, and a URI with `..` in it is
refused. A file added to the folder is a URL; splitting the page into files is what `index.html`,
`page.css` and `page.js` there already do. A `GET` that matches nothing is the portal's landing
while the setup access point is up and a 404 otherwise, and a missing tree answers
`404 ok=0&err=no_page` — the serial link still works, which is how a board without an image is
looked at.

The network the board stores lives in the same partition, at `/wifi.conf` outside `www`, as one
form-encoded line the page's own save writes. It is not reachable over HTTP, and it is cleared by a
new image: re-flashing the page tree brings the setup access point back.

## Build

Requires `arduino-cli` with the ESP32 core, and Python 3 for the wrapper.

```powershell
arduino-cli core install esp32:esp32
python scripts/arduino.py --task build
python scripts/arduino.py --task upload --port COM5
python scripts/arduino.py --task data --port COM5
python scripts/arduino.py --task monitor --port COM5
```

`--task upload` writes the page image as well as the sketch, and `--task data` writes the image on
its own. The image is built with the core's `mklittlefs` and written with its `esptool` at the
offset the board's own partition table gives the `spiffs` partition, so a different scheme needs no
edit here. The write is skipped while `data/` is unchanged, since the partition also holds the
stored network; a page edit rewrites it and the board comes back as its access point.

The board options are pinned in
[firmware/gardener/sketch.yaml](./firmware/gardener/sketch.yaml) — including `CDCOnBoot=cdc`,
without which `Serial` is UART0 and a USB cable sees nothing. The build uses
`--libraries libraries`, so [libraries/Gardener](./libraries/Gardener) — the machine, the control
law, the serial link and the three drivers that fill its parts — is the firmware rather than a
sketchbook copy. The board it runs on sits beside the sketch in
[firmware/gardener](./firmware/gardener): the composition root, the parts builder, the web link and
the tasks that drive them. The web link uses the ESP-IDF HTTP server, `DNSServer`, `LittleFS` and
`ESPmDNS` from the core, so there is no extra library to install.

A build keeps its object files in `firmware/gardener/build/compile-<hash>`, a path per board and
define set, so the next one compiles only what changed since, header dependents included.

The rig's numbers are written to a generated `firmware/gardener/rig_options.h` rather than passed
as compiler flags, so retuning a rig recompiles the files that read them and nothing else. The
header `core/knob_defaults.h` beside `core/Knob.h` is generated the same way, by `tools/knob.py`
from the `GARDENER_KNOB_VALID` calls, and both build paths write it before they compile.

## Unit tests

The tree's own C++ runs on the host, with no board and no data image:

```powershell
python tools/unit/check.py                        # build and run every case
python tools/unit/check.py --list-test-cases      # what the suite holds
python tools/unit/check.py --test-case="a name"   # one case, while a change is being made
```

The suite compiles [libraries/Gardener](./libraries/Gardener) beside its own cases under
[doctest](./tools/doctest), with [tools/host](./tools/host) supplying the Arduino API it is built
against and a clock a case advances. It needs a C++ compiler (`g++` or `clang++`) and Python, and
nothing else.

## Formatting

The C++ is formatted with `clang-format` from [.clang-format](./.clang-format) at the root, which
holds the style [.github/skills/cpp](./.github/skills/cpp/SKILL.md) describes: two spaces, a brace on
the statement's own line, a hundred columns, and comments left as they were written.

```powershell
clang-format -i (git ls-files '*.h' '*.cpp' '*.ino')   # rewrite what is not formatted
clang-format --dry-run --Werror libraries/Gardener/src/machine/Machine.h
```

What keeps another style carries a `.clang-format` of its own switching formatting off — doctest,
and the host's Arduino core and stand-ins — so the sweep leaves them alone. An editor reads the
same files, so a save lands in the shape the sweep would give it.

## Documentation

- [docs/README.md](./docs/README.md) — the index: specs, guides and backlogs
- [docs/guides/poc-parts.md](./docs/guides/poc-parts.md) — the parts, the wiring and a build
- [docs/archive/specs-and-plans.zip](./docs/archive/specs-and-plans.zip) — every spec and plan as it
  stood before the panel port, kept for reference and read by nothing

## Safety

Vacuum work is not benign: imploding chambers, collapsing tubing and pumped liquids or vapours can
hurt people and destroy hardware. Keep a rated catch pot and filter in line, and remember that the
firmware's limits are software limits only.

The one hardware guarantee is the valve. It is normally open, so losing power releases the chamber
whether or not the firmware noticed; a normally closed part trades that fail-safe for holding the
vacuum through a power loss.
