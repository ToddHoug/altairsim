# FDC+ HD Floppy Firmware (hdfloppy.s)

Source: FDC+ firmware v1.8 source, `hdfloppy.s` (M. Douglas; module v1.1, 07/01/20), from
`firmware v1.8.source.zip` at deramp.com (`.../altair/hardware/fdc+/`). The CP/M that goes with
it is at `.../altair/software/1.5mb_floppy/CPM 2.2/`: `BIOS.ASM` (v1.2), `HDFBL.ASM` (v1.1),
`BOOT.ASM`, `-ReadMe.pdf`, and `CPM22-48K-HDF.dsk`.

> The PIC24 code that runs the FDC+ when the Drive Type switches select **5**: a 96 tpi
> high-density drive (a Teac 55-GFR, or an 8" DSDD drive) in a format of Mike Douglas's own,
> with **1.5 MB** on a disk. This file says what the 8080 sees. The board around it is in the
> [FDC+ Manual](FDC%2B%20Manual.md). Emulated by the `fdcplus` board at `drivetype = 5`
> (`docs/boards/farmtek-fdcplus.md`).

## The format

- **One sector per track, 10,240 bytes** (`XFER_LENGTH`), MFM at the 8" double-density rate:
  **16 µs a byte**. The disk turns at 360 rpm: **166.67 ms** a turn (`INDEX_TIME`), with one
  index pulse a turn and no sector holes.
- On the disk, a track is: zeros to 800 µs after the index, a **sync byte** (bit 7 set, the
  track number in bits 6–0), the 10,240 bytes, a 16-bit **checksum** (the sum of the bytes, low
  byte first), and two zero bytes.
- **Both sides.** Track t is on cylinder t/2: the bottom side has the even tracks and the top
  side the odd ones, up to track 143. The last eight cylinders of the top side are not used,
  "for better reliability with aging media". Tracks 144–148 are cylinders 72–76 on the bottom,
  and 76 is the last, "for compatibility with 8" drives".

| Cylinder | 0 | 1 | … | 71 | 72 | 73 | 74 | 75 | 76 |
|---|---|---|---|---|---|---|---|---|---|
| Top | 1 | 3 | … | 143 | – | – | – | – | – |
| Bottom | 0 | 2 | … | 142 | 144 | 145 | 146 | 147 | 148 |

149 tracks × 10,240 = **1,525,760 bytes**. A disk image is those tracks in order, and nothing
else: `CPM22-48K-HDF.dsk` is exactly that size.

## The registers

Four ports. Status bits are asserted **low**.

| Addr | OUT | IN |
|---|---|---|
| base+0 | drive select: bit 7 = deselect, bits 3–0 = drive | status: 0 ENWD, 1 MOVE HEAD, 2 HEAD STATUS, 3 drive ready (the firmware's own), **4 WRITE PROTECT**, 5 INTE, 6 TRACK 0, 7 NRDA |
| base+1 | command: 0 step in, 1 step out, 2 head load, 3 head unload, **4 read enable**, 6 low write current, 7 write enable | sector position: sector 0 or 15; bit 0 sector true (low) |
| base+2 | write data | read data |
| base+3 | **the track number** (0–148) | **I/O status**: 0 checksum error, 1 track error, 2 end of sector too soon, 7 done (asserted **high**) |

`dsINIT_VALUE = 0xF5`: MOVE HEAD and drive ready asserted.

## Drive select

- Bit 7 set, or a drive number of 4 or more: **enterIdle**. Status and sector position ← `FF`,
  I/O status ← 0, every drive signal off (the motor too), every flag false.
- The drive already selected: nothing happens.
- Otherwise: status ← `F5`, then TRACK 0, INTE and WRITE PROTECT from the drive and the bus.
  The motor is turned on, the motor timer ← 0, all flags cleared, and the sector loop restarts at
  **waitMotor**, which throws away the first index pulse.

## The drive command

Ignored if no drive is selected. Only one kind of command acts, in this order:

1. **Any of bits 0–3.** HEAD STATUS de-asserted and sector position ← `FF` at once.
   - **A step** (bits 0–1). MOVE de-asserted. Step out wins, and gives no pulse at track 0.
     The step timer (`ptStepTHD`, **3 ms** by default) then asserts MOVE and copies TRACK 0 from
     the drive. The head-settle timer (**18 ms**) asserts HEAD STATUS if the head is loaded. A
     head load in the same command is **not** looked at.
   - **Head load** (bit 2). HEAD STATUS asserted **at once**. If the motor was off, it is turned
     on, and the loop restarts at waitMotor.
   - **Head unload** (bit 3). The head-loaded flag is cleared.
2. Otherwise, **read enable** (bit 4): the read flag is set, and NRDA de-asserted.
3. Otherwise, **write enable** (bit 7): the write flag is set, and ENWD de-asserted. **Low
   write current** (bit 6) sets its flag.

## Track number (`OUT base+3`)

It is kept for the sync-byte check and the write. It also **picks the side**: tracks of 144 or
more are on the bottom, and below that odd tracks are on the top and even on the bottom. The
cylinder is wherever the steps put the head. The BIOS keeps the two in step.

## The sector loop, one turn at a time

At each index pulse:

1. **The fake boot sector.** If the read flag is **not** set, the read pointer is set to
   `bootImage`, its first byte goes to the data register, and NRDA is asserted. The read flag is
   cleared either way. So a read enable set before the index is **lost**: the 8080 must send it
   after the index.
2. **The motor timer.** It is cleared if the head is loaded, then counted. At 32 (5.3 s with
   the head unloaded) the motor goes off, and the index pulses stop until a head load.
3. The rotation time must be within ±1.5%.
4. **The sector number** toggles between 0 and 15, "because the original Altair boot PROM looks
   for sector zero and the Combined Disk Boot Load (CDBL) looks for sectors 0 and 15". It goes
   to the sector register, with sector true, only if HEAD STATUS is asserted. **Sector true lasts
   32 µs.**
5. **Read clear**, to 400 µs after the index. If the write flag is set during it, the write
   starts. When it ends, the read starts if the read flag is set. Otherwise nothing happens until
   the next index.

### Read

- Both buffer pointers go to `trackBuf`, and the I/O status is cleared. The **data register is
  not reloaded**, so the 8080's first `IN` is a dummy read.
- The firmware hunts for the sync byte. If its track number (7 bits) is not the one from
  `OUT base+3`, **track error** is set.
- Each byte goes into the buffer as it arrives, **every 16 µs**. NRDA is asserted after the
  fifth. The 8080 reads the data port **with no handshake**: each `IN` gives the next byte in
  the buffer, **whether or not it has arrived**.
- After the checksum: **done**, plus checksum error if it did not match.
- No sync byte before the next index (a track that was never written): the next index sets
  **end of sector** and done and asserts NRDA, "so the 8080 is not stuck in a loop". That index
  is used up.

### Write

- ENWD is asserted, the pointers go to `trackBuf`, and the I/O status is cleared. The drive's
  write gate goes on, and zeros are written until **800 µs** after the index.
- The 8080 writes the track into the buffer with **no handshake**. After the zeros come the
  sync byte (the track number from `OUT base+3`, with bit 7 set), then the buffer's bytes, taken
  **one every 16 µs**, then the checksum and two zero bytes.
- Write gate off, the write flag cleared, and **done**. A write-protected disk is protected by
  the drive, which ignores the write gate.

## The fake boot sector

"The disk format and data interchange between the 8080 and the FDC+ is completely different…
This means a different boot PROM is required. As a way to get around this requirement, the main
sector loop returns a standard Altair 137 byte sector … whenever the index pulse is reached AND
an HD floppy read is not in progress."

The sector (`bootSector`, assembled from `HDFBL.ASM`) is the CP/M boot-sector format that DBL
reads:

| Byte | Value | |
|---|---|---|
| 0 | `80` | sync bit, track 0 |
| 1–2 | `0080` | the load ends at 0080 — one sector |
| 3–130 | code | HDFBL, at 0000 |
| 131 | `FF` | marker |
| 132 | `A6` | checksum of bytes 3–130 |
| 133–136 | `00` | the rest of the sector |

DBL loads it to 0000 and jumps there. HDFBL selects drive 0, gives track 0, loads the head,
restores to track 0, waits for sector true, sends read enable, waits for NRDA, reads the whole
track to **4000h** (a dummy read, then 10,240 bytes at 17 µs each), checks the I/O status
(starting again on an error), deselects, and jumps to 4000h.

`bootImage` is 256 bytes, and `trackBuf` comes right after it in the PIC's RAM.

## The CP/M BIOS (1.5 MB)

- **It needs a 2 MHz 8080.** The read loop takes 34 T-states a byte (17 µs at 2 MHz: "must be
  slower than the physical transfer rate of 16 µs/byte"). The write loop takes 28 T-states a
  byte (14 µs: "must be faster"). That window is 1.75 to 2.1 MHz.
- A whole track is buffered for reads and writes. A write is checked by a read (`dVerify`),
  which checks the checksum only.
- `dNxtSec` waits for sector true for up to 1.4 s (a 65,536-pass loop). A 5.25" drive cannot
  report that no disk is in it, so this is how a missing disk becomes an error.
- The BIOS gives the track number (`OUT DRVTRK`) before each seek, and steps to cylinder
  track/2 (or track − 72 for tracks 144 and up).
- After a write it waits 600 µs "for tunnel trim erase".

## Notes for an emulator

- There are **no card interrupts** in this module.
- The checksum and the sync byte are on the disk, not in an image. An image read has no bad
  checksums, and its sync byte is the track's own number.
- The spin-up time of a real drive is not in the firmware. It only waits for the first index
  pulse and throws it away.
