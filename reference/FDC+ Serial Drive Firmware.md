# FDC+ Serial Drive Firmware (serialDrive.s)

Source: FDC+ firmware v1.8 source, `serialDrive.s` (M. Douglas; module v1.2, 06/19/20),
from `firmware v1.8.source.zip` at deramp.com (`.../altair/hardware/fdc+/`).

> The PIC24 code that runs the FDC+ when the Drive Type switches select **6** (serial drive as
> an Altair Minidisk) or **7** (serial drive as an Altair 8" drive). There is no disk: the card
> keeps **one track** in RAM and gets it from a drive server over its serial port. This file
> distills what the 8080 sees and what goes on the wire. The wire format itself is in
> [FDC Serial Drive Protocol](FDC_Serial_Drive_Protocol.md); the board around it is in the
> [FDC+ Manual](FDC%2B%20Manual.md). Emulated by the `fdcplus` board
> (`docs/boards/farmtek-fdcplus.md`).

## Structure

Two halves, and the split matters:

- **The port ISR** (`ioPortInt`) handles every 8080 `IN`/`OUT` in under 2 µs. It only moves
  bytes and flags. It never talks to the server.
- **The idle loop** talks to the server: a track READ when the ISR asks for one, a STAT every
  0.1 s, and the idle write-back.

Drive type bit 0 picks the geometry: **1 = 8"** (type 7), **0 = Minidisk** (type 6).

## Constants

| Name | Value | Meaning |
|---|---|---|
| `NUM_DRIVES` | 8 | drives; select mask `scDRIVE_MASK = 0x07` (three bits, not four) |
| `BYTES_PER_SECTOR` | 137 | the hard-sector slot |
| `TRACK_LENGTH8` / `5` | 32 × 137 = 4384 / 16 × 137 = 2192 | one track, on the wire |
| `SECTOR_TIME8` / `5` | 5.208 ms / 12.5 ms | sector timer (TMR1): 360 rpm / 300 rpm |
| `STEP_TIMER8` / `5` | 10.5 ms / 50 ms | head step time (OC1, 250 kHz, 4 µs a tick) |
| `STAT_RATE` | 0.1 s | STAT interval |
| `RESPONSE_TIMEOUT` | 1 s | from the START of a receive (see the note below) |
| `WRITE_TRIES` | 3 | track write attempts |

## The registers (as the 8080 sees them)

Status (`IN base`) is kept as the raw inverted byte; every flag is asserted **low**.

| Bit | Name | Asserted (0) when |
|---|---|---|
| 0 | ENWD | from WRITE ENABLE until the next sector-position read |
| 1 | MOVE HEAD | the step timer has run out |
| 2 | HEAD STATUS | head loaded and not stepping |
| 3 | (READY) | a ready drive is selected — the firmware's own flag; a real 88-DCDD leaves this bit at 0 whenever a drive is enabled |
| 5 | INTE | the bus PINTE line (the 8080's interrupt enable) |
| 6 | TRACK 0 | on track 0 |
| 7 | NRDA | from sector true until the 137th byte is read |

`dsINIT_VALUE = 0xE5`: the value after a select (MOVE asserted, bits 3 and 4 low).

## Drive select (`OUT base`)

1. Status and sector position ← `FF`. Interrupts off. Head unloaded.
2. Bit 7 set: deselect, done.
3. Drive = `data & 7`. **Not in the server's mount map: done** — both registers stay `FF`.
4. Status ← `E5`; TRACK 0 if the drive's track is 0; INTE from PINTE; motor timer reset.
5. Minidisk only: HEAD STATUS asserted and the head counted as loaded — there is no solenoid.

## Drive command (`OUT base+1`)

Ignored if no ready drive is selected. Then, in this order:

- Any of bits 0–3: on the 8", sector position ← `FF` at once ("8080 software may quickly read
  the status registers after writing this command").
- **Step** (bits 0–1): sector position ← `FF`; MOVE and HEAD de-asserted; motor timer reset.
  **Step out wins** over step in. Step out stops at 0 and asserts TRACK 0 on reaching it; step in
  de-asserts TRACK 0 and has **no upper limit** ("removed with addition of the 8Mb drive"). The
  step timer starts; when it expires, MOVE is asserted (if still ready) and HEAD too (if loaded).
- **Head unload** (bit 3): 8" only — head unloaded, HEAD de-asserted. The Minidisk ignores it.
- **Head load** (bit 2): motor timer reset. 8": head loaded, HEAD asserted. Minidisk: nothing
  more (it is the period BIOS's `cRESTMR`).
- **Write enable** (bit 7): the track becomes **dirty**; ENWD asserted.
- **Interrupts**: bit 5 disables (and drops the line), else bit 4 enables. Disable wins.

**Fast stepping (8 MB drive):** when a STAT reports a track ≥ 77, the firmware clears the high
byte of the step time for good. On the 8" that is `0x0A41 → 0x41` ticks = **260 µs**.

## Sector position (`IN base+1`) — `chkSector`

This runs **after** the 8080 has read the register, and works out the next value:

1. ENWD de-asserted. On the 8", the motor timer is reset (the read counts as activity).
2. If the value just read had **sector true** (bit 0 low): point at that sector in the track
   buffer, byte count ← 0, put its first byte in the data register, **assert NRDA**.
3. If the current drive:track is **not the one in the buffer**: request a track read, and
   sector position ← `FF`. Done.
4. If a sector interrupt has happened since the sector last moved **and** the next sector has
   arrived from the server (`secsBuffed`): sector position ← next sector, sector true
   asserted. Done.
5. Otherwise: sector true de-asserted, same sector.

So **sector true lasts for one read**, and the sector number moves on only when time has passed
*and* the data is there. There is **no ready check** here: a sector read on a drive that is not
ready still requests a track.

## Data (`IN`/`OUT base+2`)

- Read: after the 8080 takes a byte, the count goes up and the next byte of the buffer is put
  out. NRDA is de-asserted after the 137th, but the register keeps moving.
- Write: the byte goes into the buffer at the pointer, for the first 137; later bytes are taken
  and dropped. There is **no check of write enable** here.

There is **no byte clock**: bytes move as fast as the 8080 moves them.

## The track buffer and the link

- **One buffer**, `trackBuf`, and `driveTrack` = drive in the top nibble, track in the low 12
  bits; bit 15 set means "nothing valid".
- **READ** (`readTrack`): if the buffer is dirty, write it first. Then `driveTrack` ← the
  **current** drive:track, `secsBuffed` ← 0, send `READ`. `secsBuffed` goes up by one for every
  137 bytes received, so the 8080 can read early sectors while the rest of the track is still
  arriving. On a timeout or a bad checksum, bit 15 is set. The request flag is cleared either
  way: the next sector-position read asks again.
- **WRIT** (`writeTrack`): the dirty flag is cleared **at the start**. `WRIT` → wait for the
  `WRIT` reply with code 0 → send the track and its checksum → wait for `WSTA` code 0. Any other
  result retries the whole exchange; after 3 tries it **gives up without reporting it**.
- **STAT** every 0.1 s: parameter 1 = `0x00FF` if no ready drive is selected, else the drive
  number with `0xFF` in the high byte if the head is loaded; parameter 2 = the current drive's
  track. The reply's data word **replaces the ready map**. If the selected drive is no longer
  in it, status and sector position ← `FF` until the next select.
- Before every receive the firmware empties the UART FIFO, so a late reply is never taken for
  the current one.

## The motor timer (`motorTimer`)

Counts **sector interrupts**, which only happen while a ready drive is selected. It is reset by
a select, a step, a head load, and (8" only) a sector-position read. The idle loop checks it at
each STAT:

| Drive | Condition | Action |
|---|---|---|
| 8" | head **not** loaded, bit 8 (256 sectors, 1.3 s) | forget the buffer (bit 15) and write it back if dirty; timer ← 0 |
| Minidisk | bit 7 (128 sectors, 1.6 s) | forget the buffer and write it back if dirty |
| Minidisk | bit 9 (512 sectors, 6.4 s) | status and sector position ← `FF`, interrupts off, head unloaded; timer ← 0 |

Forgetting the buffer makes the next access fetch the track again — "this makes user perceived
performance for typed commands more accurate and properly handles a disk swap on the server."

## Interrupts

The sector timer asserts the card's interrupt at every sector while a ready drive is selected
and interrupts are enabled. **Any** port access clears it (the FDC+ manual §2.4.1).

## Notes for an emulator

- **The receive timeout.** `rcvBuffer` starts its 1 s timeout at the start of the receive, and
  does not restart it for each byte. At 403.2K a track takes 0.11 s. The protocol document says
  "one second after the last byte", which is the only reading that works on a slower host line
  (a track at 38,400 baud takes 1.14 s).
- **Baud rates.** v1.8's monitor offers 403.2K (preferred), 460.8K and 230.4K; any other stored
  value is forced back to 403.2K. The server added 9.6K, 19.2K, 38.4K and 76.8K in v1.4 and
  57.6K in v1.41 (its `release.txt`, confirmed by the rate strings in `FDC+ Server 1.51.exe`),
  and later FDC+ firmware offers the same eight.
- **An empty drive's READ gets no answer** from the server (observed on an ESP32 server, 2026-09-24).
  The firmware copes through its timeout.
- **A bus RESET is not handled.** Nothing in the serial-drive module or `common.s` responds to
  it: the PIC restarts at power-on only.
