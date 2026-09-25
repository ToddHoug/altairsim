# FarmTek FDC+ — serial drive (`fdcplus`)

**Status:** partial. Drive types **6** (serial drive as an Altair Minidisk) and **7** (serial
drive as an Altair 8" drive, including the 8 MB drive) are done. The other drive types, the
on-board RAM and PROM, and the sector interrupt are not emulated — see Limitations.

## The real hardware

The FDC+ (FarmTek, PC038, Mike Douglas) is a modern one-board replacement for the two-board MITS
88-DCDD and 88-MDS floppy controllers. A PIC24 microcontroller answers the 8080's port accesses,
and a Drive Type switch bank (S3, **latched at power-on**) picks what it drives: real 8" or
Minidisk drives, 5.25" HD drives, the iCOM FD3712 format — or **no drive at all**.

In drive types 6 and 7 the card has no rotating media. Its high-speed serial port (J2) talks to
a **drive server** on a PC, which holds the disk images. The card keeps one track in RAM and
fetches it from the server, and writes it back, a whole track at a time. To the 8080 it is an
88-DCDD (type 7) or an 88-MDS (type 6); period software runs unchanged.

Why a person wants this in a simulator (issue #560): the images can live on another computer,
the simulator and a real FDC+ Altair can use the same images through the same server, and a new
server implementation can be tested without a real FDC+.

## Sources

| Source | Path | Authority |
|---|---|---|
| FDC+ firmware v1.8, `serialDrive.s` | `reference/FDC+ Serial Drive Firmware.md` | **The oracle.** The firmware is the card in types 6 and 7: every register behavior, timer and link rule. |
| FDC+ Serial Drive Protocol v1.0 | `reference/FDC_Serial_Drive_Protocol.md` | The wire format. |
| FDC+ Manual v2.0 | `reference/FDC+ Manual.md` | The port map, the drive types, the address jumpers, the 8 MB drive (§3.7.4). |

The firmware and the protocol text disagree on one point: the firmware times a receive from its
start, the protocol says "one second after the last byte". The board follows the protocol — see
Quirks.

## Register reference

Four ports at `08`–`0B`, or `80`–`83` with the address jumpers moved. Status bits are asserted
**low**.

| Addr | OUT (write) | IN (read) |
|---|---|---|
| base+0 | drive select: bit 7 = deselect, bits 2–0 = drive (8 drives) | status: 0 ENWD, 1 MOVE HEAD, 2 HEAD STATUS, 3 drive ready (the firmware's own), 5 INTE, 6 TRACK 0, 7 NRDA |
| base+1 | command: 0 step in, 1 step out, 2 head load (Minidisk: timer reset), 3 head unload, 4 int enable, 5 int disable, 7 write enable | sector position: bits 5–1 sector, bit 0 sector true (low); `FF` when there is no track |
| base+2 | write data | read data |
| base+3 | ignored | `00` |

## How it is simulated

**Not a `HardSectorFdc`.** The 88-DCDD and 88-MDS share a register model built around a
spinning disk: the sector under the head is a reading off the clock, and a byte comes off the
medium every 32 or 64 µs. The serial drive has no medium, and its firmware behaves differently
where the guest can see it:

- The sector number moves on only when the 8080 **reads** the sector port, a sector time
  (5.208 ms / 12.5 ms) has passed, **and** that sector has arrived from the server. Sector true
  lasts for **one read**.
- NRDA is asserted as soon as the 8080 reads sector true, and a new byte is ready each time it
  takes one. There is no byte clock. ENWD is asserted from WRITE ENABLE to the next sector read.
- A sector-port read on a drive:track that is not in the buffer asks for it, and the port reads
  `FF` until the track starts to arrive. Sectors are readable while the rest of the track is
  still on the wire.

So `FdcPlusBoard` is its own `Board`, modeled on `serialDrive.s` and nothing else.

**The link runs in `pump()`, never in a bus cycle** (DESIGN.md §7.5). A port access only sets
flags — "this track is wanted", "the buffer is dirty". `pump()` runs a small state machine,
without waiting: STAT every 0.1 s, READ when a track is wanted (after writing a dirty one back),
WRIT → track → WSTA with three tries. That is the same split the firmware makes between its port
ISR and its idle loop. `rxBytes()` counts the bytes the server sends, so the run loop does not
nap during a transfer.

**Two clocks.** A duration the guest can read back is emulated time; one it cannot is wall
time.

| Timer | Clock | Why |
|---|---|---|
| step settle (10.5 ms, 50 ms, 260 µs) | emulated | the guest polls MOVE HEAD |
| sector time | emulated | the guest sees the sector number move |
| Minidisk 6.4 s turn-off | emulated | the guest sees status go to `FF` |
| 1.3 s / 1.6 s idle write-back | **wall** | only the link sees it |
| STAT interval, 1 s timeout | **wall** | the link |

The idle write-back was on emulated time at first. On a machine running flat out, 1.3 emulated
seconds pass long before a track arrives, and the board threw away every track in flight.

**Properties:** `port` (`08` or `80`), `drivetype` (6 or 7, read at power-on), `baud` (one of the
serial drive's eight rates, default 403200 — see below), `connect` (the drive server; `CONNECT fdc0:line` sets it). One
serial unit, `line`. No disk units: `MOUNT` is refused, because the server mounts the images.

**Debug flags:** `seek` (every step) and `link` (every READ/WRIT sent, every track received,
and a change in the server's mount map).

### Reset

- `Reset::PowerOn` (POWER): the dirty track is written back first, then the drive type is
  latched, every track number goes to 0, no drive is ready until the next STAT answers, and the
  buffer is empty.
- `Reset::Bus` (RESET): **nothing.** The firmware has no bus-RESET handler; the PIC restarts at
  power-on only.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| Sector true lasts one read, and the sector moves on only when the data is there | A BIOS reads a sector the server has not sent yet |
| NRDA at once, no byte clock | A BIOS written for the real card's speed is slowed for no reason (harmless, but not the card) |
| Select of a drive the server has not mounted: status `FF` | CP/M's select timeout never fires; a missing disk looks present |
| STAT drops the selected drive: `FF` until the next select | A disk unmounted on the server keeps answering |
| The track is written back before the next one is read, and after 1.3 s (8", head unloaded) / 1.6 s (Minidisk) idle | Writes wait forever for a track change; a disk swapped on the server is never noticed |
| A sector read on a drive that is not ready still requests a track | Nothing breaks either way; it is the firmware, and it is why the server's silence for an empty drive matters |
| Step in has no upper limit; past track 76 the 8" steps in 260 µs | The 8 MB drive (2048 tracks) cannot be reached, or seeks take minutes |
| Minidisk: the head is loaded on select; bit 2 only restarts the timer; 6.4 s idle turns the drive off | The period Minidisk BIOS, which restarts the timer before every access, sees the wrong status |
| Three select bits: `OUT 08, 0A` selects drive 2 | — |
| **The receive timeout runs from the last byte**, not from the start of the transfer | At the slow rates (38,400 baud: 1.14 s a track) every track times out |
| A failed write-back (3 tries) is reported on the host | The firmware says nothing; the guest cannot be told, but the operator can |
| The dirty track is written back on DISCONNECT, POWER and quit | The last writes are lost if you quit inside the 1.3 s idle window. (The real card would lose them if you switched off; an operator closing a window should not.) |

## Limitations and deliberate departures

- **Drive types 0–5 and 8 are not emulated.** Types 0–3 are what `dcdd` and `mds` already do;
  type 5 (the 1.5 MB drive) is planned.
- **The on-board RAM (64K) and PROM (8K) are not emulated.** Use a `memory` board with
  `builtin:dbl` or `builtin:cdbl` for the boot PROM.
- **The sector interrupt is not wired.** Interrupt enable/disable are stored, but the card never
  asserts an interrupt, and the INTE status bit (the bus PINTE line) always reads de-asserted.
  Period CP/M does not use the disk interrupt.
- **The machine needs its real crystal** (`SET cpu0 clock_hz=2000000`) — the same rule as any
  guest that times something outside the machine. The period BIOS gives up on a sector hunt
  after a 65,536-pass loop (1.4 s at 2 MHz, `dNxtSec` in BIOS.ASM). Flat out, that loop takes a
  few milliseconds, and every track transfer is longer.
- **`baud` takes the serial drive's rates and no others:** 9600, 19200, 38400, 57600, 76800,
  230400, 403200 (preferred) and 460800. The v1.8 firmware's monitor offered only the last three;
  the slow rates came later, with Serial Drive Server v1.4 (9.6K–76.8K) and v1.41 (57.6K), and
  current FDC+ firmware has the same list. The simulator does not clock the line, so the rate
  only has to match the server. A slow rate makes a slow disk: at 38,400 baud a track takes
  1.14 s, close to the BIOS's 1.4 s limit for one sector hunt.

## Verification

- `tests/test_fdcplus.cpp` drives real bus cycles against an in-test drive server: the port
  map, the latched drive type, STAT contents and timing, ready and dropped drives, the track
  request, the one-read sector true, NRDA without a byte clock, partial tracks, timeouts and bad
  checksums, the write-back order and its three tries, both idle write-backs (and that the
  8" one ignores emulated time), the Minidisk turn-off, the step timers and the fast step, drain
  on POWER and DISCONNECT, and a snapshot round trip. The tests were checked by breaking the code
  they cover.
- **Real hardware** (2026-09-24): an ESP32 FDC+ Serial Drive Server at 38,400 baud, with
  `cpm22b23-56k.dsk`, `games.dsk` and `zork1.dsk` in drives 0–2. `default` machine, `dsk0`
  replaced by `fdcplus`, `clock_hz = 2000000`, DBL at `FF00`: CP/M 2.2b booted to `A>`, and
  `DIR B:` and `DIR C:` listed the games and Zork disks.

## References

- `reference/FDC+ Serial Drive Firmware.md`
- `reference/FDC_Serial_Drive_Protocol.md`
- `reference/FDC+ Manual.md`
- `docs/boards/mits-dcdd.md`, `docs/boards/mits-88mds.md` — the controllers it replaces
