# Gardener serial and WiFi guide

The board has two front ends that need no library: a line-based command interface on its USB serial
port, and an HTTP API on whatever network its radio is on. Both reach the same machine through the
same control, so a target set from a terminal is the target the page and the API report, and a vent
asked for over HTTP is the vent the serial line answers `state=VENT` for.

Pressures are kPa below ambient everywhere: `30` is a chamber pulled 30 kPa under ambient, and
`0` is ambient. The floor, the run ceiling and the vent floor are build constants; the numbers here
are the fitted build's.

## The serial port

The board's USB-C socket carries the interface as a CDC port at 115200 baud, 8 data bits, no
parity, one stop bit. The upload and the console share that one port, so a terminal has to be
closed while the sketch is being flashed.

The serial line carries two things: the interface's replies, and the firmware's own log. A log
record is framed `[I]` or `[W]` and is never a reply. A reply's first line starts with `ok` or
`err`, and a scan's network lines follow its first; the only word the interface writes unprompted
is the heartbeat's `hb`. The first lines after boot name what the build serves, for example:

```
[I] serial: interface ready sys wifi
```

A command is one line. The line ends at LF; a CR before it is trimmed along with any other leading
or trailing whitespace. A blank line and a line whose first character is `#` are ignored, so a
comment and its command can be pasted together. A line is at most 128 bytes; a longer one is
answered once with `err bad_argument` and the rest of it is dropped.

The first word of the line is the prefix and is matched without regard to case, and so is the verb
under it; an SSID and a password keep their case. The default build registers two prefixes: `sys`
for the machine and `wifi` for the network. Replies end with LF, one per command except a scan,
which answers with a count line and then one line per network.

| what was sent | what comes back |
| --- | --- |
| a word no build registered | `err unknown_prefix <word>` |
| a word under a prefix that the plugin does not serve | `err bad_argument <word>` |
| an argument the word does not take | `err bad_argument <argument>` |

## Machine commands

| command | reply |
| --- | --- |
| `sys get` | `ok uptime_ms=<ms> pressure_kpa=<x> target=<y> state=<word> hold_time_ms=<ms> duty=<z> fault=<code\|none>` |
| `sys set <kPa\|->` | `ok target=<kPa> state=PULL`, or `ok target=0.0 state=VENT` for the ambient target |
| `sys vent` | `ok state=VENT` |
| `sys reset` | `ok state=IDLE` — clears a latched fault |

A bare `sys` is not a status request; it is `err bad_argument`. `sys get` is the one way to ask.

The status fields are one instant's copy of the machine:

| field | meaning |
| --- | --- |
| `uptime_ms` | milliseconds since boot when the copy was taken |
| `pressure_kpa` | depth below ambient, two decimals; `0.00` while there is no valid reading |
| `target` | the target the machine holds, one decimal; `0.0` is the ambient target |
| `state` | `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT` |
| `hold_time_ms` | milliseconds since the state entered `HOLD`; `0` while it is not holding, and a new hold starts its own count |
| `duty` | the pump as commanded: `100.0` while it is driven, `0.0` while it is not |
| `fault` | the latched code, or `none` |

`sys set` is a target and a run in one command. The run gets the machine's own ceiling, two hours
on the fitted build; a controller that keeps the port open but stops talking cannot hold the pump
past it. A set is refused, and queues nothing, in these ways:

| reply | cause |
| --- | --- |
| `err out_of_range field=target value=<v> min=0.0` | a target below ambient, including one written as `nan` |
| `err out_of_range field=target value=<v> max=60.00` | a target deeper than the fitted floor |
| `err not_ready latch` | a fault is latched; `sys reset` clears it |
| `err not_ready no_reading` | the sensor is absent or its reading is not valid yet |

The floor itself is accepted (`sys set 60` with a valid reading is `ok target=60.00 state=PULL`).
Zero and `-` are the ambient target: both release the chamber, which is the same thing `sys vent`
does, and neither needs a reading. A token with no number in it reads as 0, so it releases too —
send `0` or `-` deliberately, and check the reply if a script builds the token.

A set, a vent and a reset are queued rather than carried out inside the reply, so their `state=` is
where the machine is going, not necessarily where it is. `sys get`, or the heartbeat, is what shows
the state that followed. The set's reply repeats the target to two decimals; the status lines carry
one.

| state | meaning |
| --- | --- |
| `IDLE` | resting: pump off, valve open, nothing demanded |
| `PULL` | the pump is bringing the chamber toward the target |
| `HOLD` | the reading is inside the band around the target |
| `VENT` | the valve is open: a release to ambient, or a vent to a shallower target |
| `FAULT` | a fault is latched, the pump is off and the valve is open |

A fault latches, and only `sys reset` clears it. When a fault is set, a reset clears the latch and
releases the chamber, so the machine is left in `VENT` and settles to `IDLE`.

| code | cause |
| --- | --- |
| `sensor` | the reading was lost or invalid while the machine was not resting |
| `vent_timeout` | the release did not reach the vent floor within its timeout |
| `overshoot` | the reading stayed past the deep edge after repeated recoveries |
| `no_progress` | the pump ran with no improvement in depth: a blocked hose or a dead pump |

### A session

```
sys get
ok uptime_ms=4812 pressure_kpa=0.42 target=0.0 state=IDLE hold_time_ms=0 duty=0.0 fault=none
sys set 30
ok target=30.00 state=PULL
sys get
ok uptime_ms=9120 pressure_kpa=18.73 target=30.0 state=PULL hold_time_ms=0 duty=100.0 fault=none
sys get
ok uptime_ms=24200 pressure_kpa=30.42 target=30.0 state=HOLD hold_time_ms=2350 duty=0.0 fault=none
sys vent
ok state=VENT
sys get
ok uptime_ms=25140 pressure_kpa=28.10 target=30.0 state=VENT hold_time_ms=0 duty=0.0 fault=none
```

### The heartbeat

While a host has the port open, the link writes a status line of its own every five seconds:

```
hb uptime_ms=15000 pressure_kpa=0.00 target=0.0 state=IDLE hold_time_ms=0 duty=0.0 fault=none
```

It is the `sys get` fields without the `ok`, under a word of its own, so a reader collecting `ok`
and `err` does not mistake it for a reply. A controller can watch it to tell a quiet machine from a
dead one without polling. Nothing is written while no host has the port open; on a plain UART the
board cannot tell, so the heartbeat always goes out.

## Network commands

The radio has one stored choice — the network it joins, that network's password, and whether the
setup access point outlives a successful join. Each field is written to the board's filesystem as
it is given, so the choice survives a power cut without a separate save.

| command | reply |
| --- | --- |
| `wifi` | `ok ssid=<name> mode=<sta\|ap\|ap+sta> ip=<address> keep_ap=<0\|1>` |
| `wifi scan` | `ok count=<n>`, then one line per network: `ssid<i>=<name> rssi<i>=<dBm>` |
| `wifi ssid <name>` | `ok ssid=<name>` — the rest of the line is the name, and it is stored |
| `wifi pass <password>` | `ok` — the rest of the line, stored and never repeated |
| `wifi keep <on\|off>` | `ok keep_ap=<0\|1>` |
| `wifi join` | the status line, after the stored choice is applied |
| `wifi forget` | the status line, after the stored choice is cleared |

The status fields are the stored choice and the radio as it stands:

| field | meaning |
| --- | --- |
| `ssid` | the network the board is set to join; empty when none is stored |
| `mode` | `ap` for the setup access point alone, `ap+sta` while it is kept up beside a join, `sta` otherwise |
| `ip` | the address the page answers on, or `-` when there is none |
| `keep_ap` | `1` when the setup access point stays up after a successful join |

A scan lists at most 16 networks, one per line, indexed in the order found. A scan while the radio
is still joining can answer `count=0`, and a network with a space or a quote in its name is quoted
the same way `wifi ssid` takes one.

`wifi join` is what applies what is stored; storing a name does not by itself make the board leave
the network it is on. A join with nothing stored is `err no_network`. The join is asynchronous: the
reply is the status as it stands when the join was asked for, so the address appears in a later
`wifi`, in the log line `wifi: joined <name> at <address>`, or in `GET /api/wifi`. A join that has
not succeeded after 15 seconds brings the setup access point up and retries every five seconds
until it does.

`wifi forget` clears the stored name, password and keep flag, and brings the setup access point
back; a forgotten board is provisioned again from scratch.

### Names and quoting

`wifi ssid` takes the rest of the line after the word, trimmed, so a name with spaces needs no
quotes:

```
wifi ssid My Net
ok ssid="My Net"
```

A surrounding pair of quotes is optional and is stripped, so `wifi ssid "My Net"` is the same name.
Inside quotes, `\"` is a quote and `\\` is a backslash; text after a closed pair is not part of the
name, and an unclosed quote is just a character in it. A reply quotes a value that is empty or has
a space, a quote or a backslash in it, and escapes the same two characters, so a name the wire
prints is one it takes back:

```
wifi ssid "Bob\"s Net"
ok ssid="Bob\"s Net"
```

`wifi pass` follows the same quoting rules on the way in and is never printed on the way out. A
bare `wifi pass` clears the password, which is how an open network is written.

### Provisioning over the wire

```
wifi scan
ok count=2
ssid0=Home rssi0=-55
ssid1="Bench Net" rssi1=-71
wifi ssid "Bench Net"
ok ssid="Bench Net"
wifi pass correct horse battery
ok
wifi keep off
ok keep_ap=0
wifi join
ok ssid="Bench Net" mode=ap+sta ip=192.168.4.1 keep_ap=0
[I] wifi: joined Bench Net at 192.168.1.47
wifi
ok ssid="Bench Net" mode=sta ip=192.168.1.47 keep_ap=0
```

## The HTTP API

The radio serves the page tree and the API on port 80, on whichever interface it has up: the
address it joined, or the setup access point's while that is all there is. The API lives under
`/api` on the same origin as the page, so the base is `http://<address>/api` or
`http://<host>.local/api`. The board advertises `_gardener._tcp` on the same port with an `api=/api`
TXT record, beside `_http._tcp` at `/`, so a service browser can find the API without reading the
page. The host name is the setup access point's name in lower case, with anything outside `a-z`,
`0-9` and `-` turned into a hyphen: the default `Gardener-AB12` answers as `gardener-ab12.local`.

Both the request body and the reply are `application/x-www-form-urlencoded`: `key=value` pairs
joined by `&`, with non-alphanumerics percent-encoded. There is no JSON anywhere. A reply carries
`ok=1` or `ok=0`, and a refusal carries `err=<code>` and whatever fields explain it.

| call | reply |
| --- | --- |
| `GET /api/status` | `ok=1&uptime_ms=<ms>&pressure_kpa=<x>&target=<y>&state=<word>&hold_time_ms=<ms>&duty=<z>&fault=<code\|none>` |
| `POST /api/set` with `target=<kPa\|->` | `ok=1&target=<kPa>&state=PULL`, or `ok=1&target=0.0&state=VENT` |
| `POST /api/vent` | `ok=1&state=VENT` |
| `POST /api/reset` | `ok=1&state=IDLE` |
| `GET /api/wifi` | `ok=1&ssid=<name>&mode=<sta\|ap\|ap+sta>&ip=<address>&keep_ap=<0\|1>` |
| `GET /api/wifi/scan` | `ok=1&count=<n>&ssid0=<name>&rssi0=<dBm>&ssid1=<name>&rssi1=<dBm>…` |
| `POST /api/wifi` with `ssid`, `pass`, `keep_ap` | `ok=1` |
| `POST /api/wifi/forget` | `ok=1` |

The fields carry the same meanings and the same precision as the serial line's. The set reply
repeats the target to one decimal here, where the wire writes two.

A call the machine took answers 200. A refused argument answers 400 with `err=out_of_range`,
`err=missing_target` or `err=missing_ssid`; a set the machine will not take answers 409 with
`err=not_ready` and a `reason` of `latch` or `no_reading`. The out-of-range reply names the `value`,
`min` or `max` beside the error, as the wire's does.

A body longer than 256 bytes is ignored, which is the same as sending nothing: a set without a
target is `missing_target` and a save without a name is `missing_ssid`. The vent, reset and forget
calls take no fields and ignore what the body carries.

```
curl -s http://192.168.1.47/api/status
curl -s -X POST -d 'target=30' http://192.168.1.47/api/set
curl -s -X POST http://192.168.1.47/api/vent
curl -s -X POST http://192.168.1.47/api/reset
curl -s http://192.168.1.47/api/wifi
curl -s http://192.168.1.47/api/wifi/scan
curl -s -X POST -d 'ssid=Bench+Net&pass=secret&keep_ap=0' http://192.168.1.47/api/wifi
curl -s -X POST http://192.168.1.47/api/wifi/forget
```

`POST /api/wifi` stores the three fields and joins at once, and answers before the join has
succeeded; `GET /api/wifi` is where the address that follows is read. The name is required; the
password may be empty for an open network, and a missing `keep_ap` is 0. Saving with `keep_ap=1`
leaves the setup access point up beside the join, which is how a board that moved to another
network can still be reached over its own access point.

Every other `GET` is a path inside the page tree, `/` and `/dir/` serving that directory's
`index.html`. A path that matches nothing is the portal's landing while the setup access point is
up: a client on that access point is redirected to `/`. With no page image on the board, a missing
path answers `404 ok=0&err=no_page`, and the serial interface still works.

The page and the API are unauthenticated on whatever network the board joins, which is the same
trust the serial cable carries. Anyone who can reach the board can vent it or point it at another
network, so keep the setup access point keyed and join a network you trust.

### Provisioning over HTTP

The first boot has no stored network, so the board comes up as its own access point. Join it with a
phone or laptop, then either use the page or the API:

```
curl -s http://192.168.4.1/api/wifi                    # the stored choice and the portal's address
curl -s http://192.168.4.1/api/wifi/scan               # what the radio can see
curl -s -X POST -d 'ssid=Bench+Net&pass=secret&keep_ap=0' http://192.168.4.1/api/wifi
curl -s http://192.168.1.47/api/wifi                   # the new address, after the join
```

The default access point address is 192.168.4.1; the log prints the name and address the board
actually came up with. The access point's own name and key are not part of this API: they come from
`ap_ssid` and `ap_pass` in the image's `/wifi.conf`, or from the build's defaults when the file
does not name them. The stored network lives in the same file, outside the page tree, so no URL
reaches it; provisioning a new image clears it and brings the setup access point back.

## Troubleshooting

| symptom | cause | what to do |
| --- | --- | --- |
| `err unknown_prefix sys` | the build does not register `sys`, or the prefix is misspelled | read the `serial: interface ready` line at boot |
| `err bad_argument` with nothing after it | the line was longer than 128 bytes, or a word that needs an argument was sent without one | keep the line under the limit, or send the argument |
| `err bad_argument <word>` | a verb the plugin does not serve, or a second argument where one is taken | check the tables above |
| `err not_ready latch` | a fault is latched | `sys reset`, then set again |
| `err not_ready no_reading` | the sensor is absent or has no filtered sample yet | check the sensor's wiring and reference port |
| `err no_network` | `wifi join` with no name stored | `wifi ssid <name>` first |
| `wifi scan` answers `count=0` | the radio is busy joining, or nothing is in range | try again once the join settles |
| `ok=0&err=no_page` | the board has no page image | write the data image, then reboot |
| the page will not load | the board is on another network, or mDNS is not resolved | use the address the log prints, or `GET /api/wifi` from a host that can reach it |
