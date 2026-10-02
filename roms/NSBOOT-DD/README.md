# NSBOOT — North Star MDS-A-D boot PROM (`builtin:nsboot-dd`)

The 256-byte bootstrap of the **North Star MDS-A-D** double-density floppy controller.

- **Run address:** `E800h`. The standard controller occupies `E800`–`EBFF`, and the PROM is
  the first 256 bytes of that block.
- **Decoded image:** `E800`–`E8FF`, 256 bytes, CRC32 `7AAFA134`.

## What it does

The PROM uses no RAM and no stack. A subroutine gets its return address in `HL`.

1. It turns the drive motors on and selects drive 1.
2. It waits for the index hole. A diskette with no index hole in 12 sectors makes the PROM
   stop in a loop.
3. It steps the head to track 0.
4. It waits for sector 4 and reads the density of the diskette from the status byte.
   - A double-density diskette: it reads track 0, sector 4.
   - A single-density diskette: it steps to track 1 and reads sector 8.
5. It reads 512 bytes. The first byte is the page (the high address byte) where the sector
   goes. The bytes go into memory from byte 1 of that page.
6. It jumps to byte `0Ah` of that page.

A read error makes the PROM try again. After 10 errors it stops in a loop.

## Use it

The `mdsad` board loads this PROM itself. It is not a ROM that you mount. `altairsim
northstardd` starts it with `RUN E800`. See
[`docs/boards/northstar-mds.md`](../../docs/boards/northstar-mds.md).

## Files here

| File | What it is |
|---|---|
| `NSBOOT.HEX` | The 256-byte image. The build embeds this file. |
| `NSBOOT.ASM` | The source. It assembles with Digital Research `ASM` to the bytes of the PROM. |
| `NSBOOT.PRN` | The assembler listing. `SYMBOLS LOAD` reads it. |

**Source:** disassembled by Martin Eberhard, 28 November 2016, from the PROMs of an MDS-A-D
controller. Fetched 2026-10-02 from deramp.com
(`.../north_star/hardware/Double Density Controller/PROMs/`). The CRC32 test is in
[`docs/roms.md`](../../docs/roms.md).
