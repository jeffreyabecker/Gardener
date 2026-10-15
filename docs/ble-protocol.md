# Eggplan Vacuum BLE protocol

One primary service with four characteristics. Pressures are **kPa** everywhere.
All multi byte integers are little endian.

## GATT layout

| Role | UUID | Properties |
| --- | --- | --- |
| Service | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00` | — |
| Command (`cmd`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01` | write, write without response |
| Response (`rsp`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02` | read, notify |
| Program (`prog`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03` | write, write without response |
| Info (`info`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04` | read |

The device advertises as `EggplanVac<ID>` where `<ID>` comes from the eFuse MAC, with
the service UUID in the advertisement and scan response.

## Reply format

Replies are UTF-8 text on `rsp`, either

```
ok <payload>
err <message>
```

The payload is space separated `key=value` tokens with a stable key order. Values are
space free tokens and `-` means "not applicable", so nothing needs quoting or escaping.
Unknown keys can be ignored, which is what lets fields be added later.

```
ok kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 floor=41.3 fault=- up=123456 ble=1
```

Telemetry notifications use the same payload with a `tel ` prefix and are never
chunked:

```
tel kpa=41.23 vac=60.10 pct=59.3 duty=72.0 state=PUMP target=41.0 floor=41.3 fault=- up=123460 ble=1
```

Replies longer than one notification (`ATT MTU - 3` bytes) are split:

* chunk: `0x01`, sequence byte, then up to `MTU - 5` payload bytes
* end: `0x02`, last sequence byte

Concatenate the chunk payloads in sequence order (wrap at 256) and terminate on the
`0x02` marker rather than by counting bytes.

Multi line replies use a header, one row per item, then a bare `eof` on its own:

```
-> list
<- ok programs 2
<- p hold_vacuum 137 0x1A2B3C4D auto
<- p pulse 331 0xDEADBEEF -
<- eof
```

## Program transfer

One opcode per write to `prog`. Maximum frame size is 512 bytes including the opcode;
keep each write within `MTU - 3`. The opcodes are private to the firmware's uploader —
clients only need this table.

| Opcode | Payload | Meaning |
| --- | --- | --- |
| `B` (0x42) | program name, ASCII | start an upload. Reply: `ok begin <name>` |
| `D` (0x44) | raw bytes | append a chunk. Reply: `ok data <total_bytes>` |
| `E` (0x45) | `uint32 checksum`, `uint32 size` | commit. Either field may be `0` to skip validation. Reply: `ok stored <name> <bytes>` |
| `X` (0x58) | — | abort and delete the staging file. Reply: `ok aborted` |

Rules:

* Names are `[A-Za-z0-9_-]{1,31}`, stored as `/programs/<name>.be`.
* Uploads land in a staging file and are renamed only on a successful commit, so an
  interrupted transfer never replaces an existing program.
* The checksum is the standard CRC-32 (reflected, poly `0xEDB88320`, init
  `0xFFFFFFFF`, final XOR `0xFFFFFFFF`) — i.e. `zlib.crc32`. It is opaque to the
  firmware's command layer, which only passes the value through.
* A leading UTF-8 BOM is stripped before the checksum is computed.
* The maximum program size is 64 KiB.
* Disconnecting during an upload aborts it.

## Commands

Send newline terminated lines to `cmd`. `\r` is ignored, the buffer holds 512 bytes,
and a single write may contain several commands.

| Command | Effect |
| --- | --- |
| `ping` | `ok pong <version>` |
| `info` | `ok fw=… chip=… runtime=… progs=… store_kb=… sensor=… tach=… heap=…` |
| `status` | live status line (see below) |
| `list` | `ok programs <n>`, then one `p` row per program, then `eof` |
| `cat <name> [offset] [len]` | `ok file <name> <offset> <len> <total>`, then the text, then `eof` |
| `rm <name>` | delete a program |
| `run <name> [args…]` | start a stored program |
| `stop` | stop the program, the control loop and the pump |
| `eval <berry>` | evaluate an expression in the script runtime |
| `pump <0-100\|off>` | manual duty (capped at `PUMP_DUTY_MAX_PERCENT`) |
| `manual <0-100>` | alias of `pump` |
| `target <kPa\|off>`, `hold <kPa\|off>` | engage/release the closed loop |
| `ambient [kPa]` | capture or set the ambient reference |
| `autostart [<name>\|none]` | program started on boot |
| `telemetry <on\|off\|ms>` | telemetry notifications, minimum period 50 ms |
| `log [n]` | tail of the device log (default 20 lines) |
| `buttons` | ask the local UI to move to the next page |
| `reset` | clear the pump fault latch and the sensor error counter |
| `reboot` | restart the device |
| `help` | command list |

`cat` is paged: it returns at most 2048 bytes per call and defaults to 512, so walk a
large program with `offset`/`len`.

### `status` keys

| Key | Meaning |
| --- | --- |
| `kpa` | filtered absolute pressure |
| `vac` | `ambient - kpa`, clamped at 0 |
| `pct` | `vac / ambient * 100` |
| `duty` | pump duty % |
| `state` | `IDLE`, `PUMP`, `HOLD`, `MAN` or `FAULT` |
| `target` | hold target in kPa, or `-` |
| `floor` | deepest reachable target, `ambient - 60 kPa` |
| `fault` | `none`, `max_run` or `sensor` |
| `up` | milliseconds since boot |
| `ble` | `1` while a client is connected |

### `info` keys

| Key | Meaning |
| --- | --- |
| `fw` | firmware version |
| `chip` | chip model |
| `runtime` | `none` or `berry` |
| `progs` | stored program count |
| `store_kb` | filesystem size |
| `sensor` | `1` when the MPRLS answered at boot |
| `tach` | `1` when the tach input is enabled |
| `heap` | free heap in bytes |

## Serial console

The same command grammar is available on USB serial at 115200 baud; the serial port
currently carries the log stream only, which is enough to debug boot and I2C problems.
Wiring the console is a small addition in `App` plus a `hosts/serial/` transport.

## Example session

```text
-> ping
<- ok pong 0.1.0
-> telemetry 250
<- ok telemetry on 250
<- tel kpa=101.28 vac=0.05 pct=0.0 duty=0.0 state=IDLE target=- floor=41.3 fault=- up=1200 ble=1
-> B hold_vacuum
<- ok begin hold_vacuum
-> D # hold_vacuum.be…
<- ok data 137
-> E <checksum> <size>
<- ok stored hold_vacuum 137
-> run hold_vacuum
<- ok running hold_vacuum
-> status
<- ok kpa=40.21 vac=61.07 pct=60.3 duty=72.0 state=HOLD target=40.0 floor=41.3 fault=- up=45000 ble=1
-> stop
<- ok stopped
```
