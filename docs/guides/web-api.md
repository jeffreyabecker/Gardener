# Gardener web API guide

The board serves a page tree and an HTTP API from its radio, on whichever network it has joined or
on the setup access point it runs when it has none. The page is the API's first client; anything
else that can reach the board uses the same calls. Pressures are kPa below ambient everywhere: `30`
is a chamber pulled 30 kPa under ambient, and `0` is ambient.

## Where the API is

The API lives under `/api` on the same origin as the page, so the base is `http://<address>/api` or
`http://<host>.local/api`. The board advertises `_gardener._tcp` on port 80 with an `api=/api` TXT
record, beside `_http._tcp` at `/`, so a service browser can find the API without reading the page.
The host name is the setup access point's name in lower case, with anything outside `a-z`, `0-9` and
`-` turned into a hyphen: the default `Gardener-AB12` answers as `gardener-ab12.local`.

The radio serves on the address it joined, or on the setup access point's while that is all there
is. Every other `GET` is a path inside the page tree, `/` and `/dir/` serving that directory's
`index.html`. A path outside `/api` that matches nothing is the portal's landing while the setup
access point is up — a client on that access point is redirected to `/` — and otherwise the page
tree's `404.html` with a 404. A path under `/api` that nothing serves answers
`404 {"error":"not_found"}`. With no page image there is no `404.html` to serve, so the reply is a
plain 404 and the boot log says the page did not mount.

## Format

Both the request body and the reply are `application/json`. The status code is the outcome, and the
body holds only what the call answers with or the error that explains it: a success is an object of
the fields the call answers with, a refusal an object with an `error` member and whatever names the
reason beside it. Numbers are JSON numbers, `keep_ap` is a boolean, and a scan's networks are an
array of objects. There is no envelope: no `ok` flag, no `count` field.

A call the machine took answers 200; a call with nothing to return answers 204. A refused argument
answers 400 with `error` `out_of_range`, `missing_target`, `bad_target` or `missing_ssid`; a set the
machine will not take answers 409 with `error` `not_ready` and a `reason` of `latch` or
`no_reading`. The out-of-range reply names the `value` and the `min` or `max` beside the error. A
body that is not a JSON object, or is longer than 256 bytes, is `400 {"error":"bad_request"}`. The
vent, reset and forget calls take no fields and ignore what the body carries.

Values are rounded before they are sent: pressure to two decimals, target and duty to one, so the
wire carries the reading and not the sensor's noise.

## Machine calls

| call | reply |
| --- | --- |
| `GET /api/status` | `{"uptime_ms":…,"pressure_kpa":20.5,"target":30,"state":"HOLD","hold_time_ms":1000,"duty":100,"fault":"none","device_id":"E072A1234CD0"}` |
| `POST /api/set` with `{"target":30}` or `{"target":"-"}` | `{"target":30,"state":"PULL"}`, or `{"target":0,"state":"VENT"}` |
| `POST /api/vent` | `{"state":"VENT"}` |
| `POST /api/reset` | `{"state":"IDLE"}` — clears a latched fault |

The status fields are one instant's copy of the machine:

| field | meaning |
| --- | --- |
| `uptime_ms` | milliseconds since boot when the copy was taken |
| `pressure_kpa` | depth below ambient, rounded to two decimals; `0` while there is no valid reading |
| `target` | the target the machine holds, rounded to one decimal; `0` is the ambient target |
| `state` | `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT` |
| `hold_time_ms` | milliseconds since the state entered `HOLD`; `0` while it is not holding, and a new hold starts its own count |
| `duty` | the pump as commanded: `100` while it is driven, `0` while it is not |
| `fault` | the latched code, or `"none"` |
| `device_id` | the board's 48-bit station MAC as twelve upper-case hex digits; the same on every boot |

`POST /api/set` is a target and a run in one command. The run gets the machine's own ceiling, two
hours on the fitted build; nothing a client does extends it. Zero and `"-"` are the ambient target:
both release the chamber, which is the same thing `POST /api/vent` does, and neither needs a
reading. A target is a number, or the string `"-"`; a target written as any other string, such as
`"30"`, is refused as `bad_target` rather than read as a number. A target is refused, and queues
nothing, in these ways:

| reply | cause |
| --- | --- |
| `400 {"error":"missing_target"}` | the body carries no `target` member |
| `400 {"error":"bad_target"}` | `target` is neither a number nor `"-"` |
| `400 {"error":"out_of_range","field":"target","value":-5,"min":0}` | a target below ambient |
| `400 {"error":"out_of_range","field":"target","value":80,"max":60}` | a target deeper than the fitted floor |
| `409 {"error":"not_ready","reason":"latch"}` | a fault is latched; `POST /api/reset` clears it |
| `409 {"error":"not_ready","reason":"no_reading"}` | the sensor is absent or its reading is not valid yet |

The floor itself is accepted (`{"target":60}` with a valid reading is `{"target":60,"state":"PULL"}`).
Send `0` or `"-"` deliberately: a script that builds the target from a string should send the number
it means, and check the reply if it does not.

A set, a vent and a reset are queued rather than carried out inside the reply, so their `state` is
where the machine is going, not necessarily where it is. `GET /api/status` is what shows the state
that followed. The set's reply repeats the target, rounded as the status rounds it.

| state | meaning |
| --- | --- |
| `IDLE` | resting: pump off, valve open, nothing demanded |
| `PULL` | the pump is bringing the chamber toward the target |
| `HOLD` | the reading is inside the band around the target |
| `VENT` | the valve is open: a release to ambient, or a vent to a shallower target |
| `FAULT` | a fault is latched, the pump is off and the valve is open |

A fault latches, and only `POST /api/reset` clears it. When a fault is set, a reset clears the latch
and releases the chamber, so the machine is left in `VENT` and settles to `IDLE`.

| code | cause |
| --- | --- |
| `sensor` | the reading was lost or invalid while the machine was not resting |
| `vent_timeout` | the release did not reach the vent floor within its timeout |
| `overshoot` | the reading stayed past the deep edge after repeated recoveries |
| `no_progress` | the pump ran with no improvement in depth: a blocked hose or a dead pump |

## Network calls

| call | reply |
| --- | --- |
| `GET /api/wifi` | `{"ssid":"Bench Net","mode":"sta","ip":"192.168.1.51","keep_ap":false}` |
| `GET /api/wifi/scan` | `{"networks":[{"ssid":"Bench Net","rssi":-51},{"ssid":"Shop","rssi":-73}]}` |
| `POST /api/wifi` with `{"ssid":"…","pass":"…","keep_ap":false}` | `204`, no body |
| `POST /api/wifi/forget` | `204`, no body |

The status fields are the stored choice and the radio as it stands:

| field | meaning |
| --- | --- |
| `ssid` | the network the board is set to join; empty when none is stored |
| `mode` | `ap` for the setup access point alone, `ap+sta` while it is kept up beside a join, `sta` otherwise |
| `ip` | the address the page answers on, or `-` when there is none |
| `keep_ap` | `true` when the setup access point stays up after a successful join |

A scan lists at most 16 networks as a `networks` array, each entry a name and its signal, in the
order found. A scan while the radio is still joining can answer an empty array.

`POST /api/wifi` stores the fields and joins at once, answering `204` before the join has succeeded;
`GET /api/wifi` is where the address that follows is read. The name is required; the password may be
empty for an open network, and a missing `keep_ap` is `false`. Saving with `"keep_ap":true` leaves
the setup access point up beside the join, which is how a board that moved to another network can
still be reached over its own access point. A join that has not succeeded after 15 seconds brings
the setup access point up and retries every five seconds until it does.

`POST /api/wifi/forget` clears the stored name, password and keep flag, answering `204`, and brings
the setup access point back; a forgotten board is provisioned again from scratch.

```
curl -s http://192.168.1.47/api/status
curl -s -X POST -H 'Content-Type: application/json' -d '{"target":30}' http://192.168.1.47/api/set
curl -s -X POST http://192.168.1.47/api/vent
curl -s -X POST http://192.168.1.47/api/reset
curl -s http://192.168.1.47/api/wifi
curl -s http://192.168.1.47/api/wifi/scan
curl -s -X POST -H 'Content-Type: application/json' \
  -d '{"ssid":"Bench Net","pass":"secret","keep_ap":false}' http://192.168.1.47/api/wifi
curl -s -X POST http://192.168.1.47/api/wifi/forget
```

The page and the API are unauthenticated on whatever network the board joins. Anyone who can reach
the board can vent it or point it at another network, so keep the setup access point keyed and join
a network you trust.

## Provisioning

The first boot has no stored network, so the board comes up as its own access point. Join it with a
phone or laptop, then either use the page or the API:

```
curl -s http://192.168.4.1/api/wifi                    # the stored choice and the portal's address
curl -s http://192.168.4.1/api/wifi/scan               # what the radio can see
curl -s -X POST -H 'Content-Type: application/json' \
  -d '{"ssid":"Bench Net","pass":"secret","keep_ap":false}' http://192.168.4.1/api/wifi
curl -s http://192.168.1.47/api/wifi                   # the new address, after the join
```

The default access point address is 192.168.4.1; the log prints the name and address the board
actually came up with. The access point's own name and key are not part of this API: they come from
`ap_ssid` and `ap_pass` in the image's `/wifi.conf`, or from the build's defaults when the file does
not name them. The stored network lives in the same file, outside the page tree, as one JSON object,
so no URL reaches it; a file this firmware cannot read leaves the board as its own access point, and
provisioning a new image clears it.

## Troubleshooting

| symptom | cause | what to do |
| --- | --- | --- |
| `404 {"error":"not_found"}` | an API path nothing serves | check the call's path |
| the Not found page appears | the URL is not in the page image | check the file name, or write the data image |
| `400 {"error":"bad_request"}` | the body is not a JSON object, or is over 256 bytes | send a JSON object with the call's fields |
| `400 {"error":"missing_target"}` | the set request carried no `target` member | send `{"target":<kPa>}` |
| `400 {"error":"bad_target"}` | `target` is a string other than `"-"`, or another type | send a number, or `"-"` for ambient |
| `400 {"error":"missing_ssid"}` | the save request carried no `ssid` string | send `{"ssid":"<network>"}` |
| `400 {"error":"out_of_range"}` | the target is below ambient or past the floor | send a value from 0 to the fitted floor |
| `409 {"error":"not_ready","reason":"latch"}` | a fault is latched | `POST /api/reset`, then set again |
| `409 {"error":"not_ready","reason":"no_reading"}` | the sensor is absent or has no filtered sample yet | check the sensor's wiring and reference port |
| a scan answers `{"networks":[]}` | the radio is busy joining, or nothing is in range | try again once the join settles |
| the page will not load | the board is on another network, or mDNS is not resolved | use the address the log prints, or `GET /api/wifi` from a host that can reach it |
