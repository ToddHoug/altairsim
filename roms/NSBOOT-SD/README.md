# NSBOOTV2 — North Star MDS-A boot PROM (`builtin:nsboot-sd`)

The 256-byte bootstrap of the **North Star MDS-A** single-density floppy controller. This is
the later of the two versions that North Star shipped. On the board it is two 4-bit PROMs
(IC-3E and IC-3F).

- **Run address:** `E900h`. The standard controller occupies `E800`–`EBFF`, and the PROM
  answers at `E800`–`E8FF` and at `E900`–`E9FF`. The code is assembled for `E900`.
- **Decoded image:** `E900`–`E9FF`, 256 bytes, CRC32 `754E53E5`.

## What it does

1. It turns the drive motors on and selects drive 1.
2. It steps the head to track 0.
3. It reads track 0, sector 4 (256 bytes) into `2000h`.
4. It jumps to `2004h`.

A read error makes the PROM try again. After 10 errors it stops in a loop.

The PROM also holds the disk read routine `DISKOP` at `E91Eh`. North Star DOS and the CP/M
boot loader call that routine, so the PROM must be present after the boot too.

The earlier version of the PROM tries again without end, and its sector timeout is half as
long. The source file header gives both differences.

## Use it

The `mdsa` board loads this PROM itself. It is not a ROM that you mount. `altairsim northstar`
starts it with `RUN E900`. See [`docs/boards/northstar-mds.md`](../../docs/boards/northstar-mds.md).

## Files here

| File | What it is |
|---|---|
| `NSBOOTV2.HEX` | The 256-byte image. The build embeds this file. |
| `NSBOOTV2.ASM` | The source. It assembles with Digital Research `ASM` to the bytes of the PROM. |
| `NSBOOTV2.PRN` | The assembler listing. `SYMBOLS LOAD` reads it. |

**Source:** disassembled by Martin Eberhard (corrected 30 January 2018) from the PROMs of an
MDS-A controller. Fetched 2026-10-02 from deramp.com
(`.../north_star/hardware/Single Density Controller/PROMs/`). The CRC32 test is in
[`docs/roms.md`](../../docs/roms.md).
