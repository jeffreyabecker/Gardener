# Eggplan Vacuum BLE protocol

Everything below is one primary service with four characteristics. All multi byte
integers are little endian.

## GATT layout

| Role | UUID | Properties |
| --- | --- | --- |
| Service | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00` | — |
| Command (`cmd`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01` | write, write without response |
| Response (`rsp`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02` | read, notify |
| Program (`prog`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03` | write, write without response |
| Info (`info`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04` | read |

The device advertises as `EggplanVac<ID>` where `<ID>` comes from the eFuse MAC.
The service UUID is in the advertisement and scan response.

## Responses

Every reply is UTF-8 text on `rsp`, either

```
ok <payload>
err <message>
```

Replies longer than one notification (`ATT MTU - 3` bytes) are split:

* chunk: `0x01`, sequence byte, then up to `MTU - 5` payload bytes
* end: `0x02`, last sequence byte

Concatenate the chunk payloads in sequence order (wrap at 256) to rebuild the
message. Reassemble by the `0x02` terminator, not by counting bytes.

Telemetry notifications are plain text prefixed with `tel ` and are never chunked:

```
tel {"hpa":412.31,"vacuum":601.02,"vacuum_pct":59.3,...,"uptime_ms":123456}
```

## Program transfer

One opcode per write to `prog`. Maximum frame size is 512 bytes including the
opcode byte; keep each write within `MTU - 3`.

| Opcode | Payload | Meaning |
| --- | --- | --- |
| `B` (0x42) | program name, ASCII | Start an upload. Reply: `ok begin <name>` |
| `D` (0x44) | raw bytes | Append a chunk. Reply: `ok data <total_bytes>` |
| `E` (0x45) | `uint32 crc32`, `uint32 size` | Commit. Either field may be `0` to skip validation. Reply: `ok stored <name> <bytes>` |
| `X` (0x58) | — | Abort and delete the staging file. Reply: `ok aborted` |

Rules:

* Names are `[A-Za-z0-9_-]{1,31}`, stored as `/programs/<name>.be`.
* Uploads land in `/programs/.upload.tmp` and are renamed only on a successful
  commit, so an interrupted transfer never replaces an existing program.
* CRC-32 is the standard IEEE (reflected, poly `0xEDB88320`), initial value
  `0xFFFFFFFF`, final XOR `0xFFFFFFFF` — i.e. `zlib.crc32`.
* A leading UTF-8 BOM is stripped before the CRC is computed.
* The maximum program size is 64 KiB (`cfg::PROGRAM_MAX_BYTES`).
* Disconnecting during an upload aborts it.

## Commands

Send newline terminated lines to `cmd`. `\r` is ignored, the buffer holds 512
bytes, and a single write may contain several commands.

| Command | Effect |
| --- | --- |
| `ping` | `ok pong <version>` |
| `info` | device JSON (also readable from `info`) |
| `status` | live status JSON |
| `list` | `ok [{name,size,crc,autostart}]` |
| `cat <name> [offset] [len]` | `ok file <name> <offset> <len> <total>`, then the raw text as its own message, then `eof` |
| `rm <name>` | delete a program |
| `run <name> [args…]` | start a stored program |
| `stop` | stop the program, the control loop and the pump |
| `eval <berry>` | evaluate an expression in the script runtime |
| `pump <0-100\|off>` | manual duty (capped at `PUMP_DUTY_MAX_PERCENT`) |
| `manual <0-100>` | alias of `pump` |
| `target <hpa\|off>`, `hold <hpa\|off>` | engage/release the closed loop |
| `ambient [hpa]` | capture or set the ambient reference |
| `autostart [<name>\|none]` | program started on boot |
| `telemetry <on\|off\|ms>` | telemetry notifications, minimum period 50 ms |
| `log [n]` | tail of the device log (default 20 lines) |
| `buttons` | cycle the local UI page |
| `reset` | clear the pump fault latch and sensor error counter |
| `reboot` | restart the device |
| `help` | command list |

### `status` fields

| Field | Meaning |
| --- | --- |
| `hpa` | filtered absolute pressure (hPa = mbar) |
| `vacuum` | `ambient - hpa`, clamped at 0 |
| `vacuum_pct` | `vacuum / ambient * 100` |
| `ambient` | ambient reference in hPa |
| `duty` | pump duty % |
| `rpm` | tach reading, `0` unless `PUMP_TACH_ENABLED` |
| `state` | `IDLE`, `PUMP`, `HOLD`, `MAN` or `FAULT` |
| `target` | hold target in hPa, or `null` |
| `fault` | pump fault latch reason, or `null` |
| `running` | name of the running program, if any |
| `ble` | a client is connected |
| `uptime_ms` | milliseconds since boot |

### `info` fields

`fw`, `chip`, `idf`, `runtime` (`none` or `berry`), `programs`, `store_kb`,
`sensor`, `tach`, `heap`.

## Serial console

The same command grammar is available on USB serial at 115200 baud. There is no
console parser wired up yet — the serial port currently carries the log stream
only, which is enough to debug boot and I2C problems. Wiring the console is a
one-liner in `App::update()` once the BLE path is proven on hardware.

## Example session

```text
-> ping
<- ok pong 0.1.0
-> telemetry 250
<- ok telemetry on 250
<- tel {"hpa":1012.8,"vacuum":0.45,...,"state":"IDLE"}
-> B hold_vacuum
<- ok begin hold_vacuum
-> D # hold_vacuum.be…
<- ok data 137
-> E <crc32> <size>
<- ok stored hold_vacuum 137
-> run hold_vacuum
<- ok running hold_vacuum
-> status
<- ok {"hpa":402.11,...,"state":"HOLD",...}
-> stop
<- ok stopped
```
