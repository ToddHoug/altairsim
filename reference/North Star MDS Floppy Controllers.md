# North Star Micro-Disk System (MDS-A single-density / MDS-A-D double-density)

Source: [Single Density Controller (2nd version).pdf](#) (North Star *Micro-Disk System
MDS-A*, © 1977, **Revision 5**, for the MDS-A4 PC board; July 31 1978 errata),
[Double Density Controller.pdf](#) (North Star *Micro-Disk System MDS-A-D Double Density*,
© 1978, **Revision 1**; January 16 1979 errata)

Two generations of one North Star S-100 floppy-disk controller for **8080 or Z80** systems,
documented together because the double-density board is the single-density board's successor and
they share their whole personality: a 5¼″ **Shugart SA-400** hard-sectored minifloppy (10 sector
holes + 1 index hole per revolution), **35 tracks × 10 sectors**, an onboard **256-byte bootstrap
PROM**, a 4 MHz crystal ÷2 → **2 MHz** controller clock, and — the trait that makes this board
unlike every other floppy controller in the tree — a **memory-mapped command interface**: the
controller is not an I/O-port device at all. It occupies a **1 K byte block of CPU memory**
(standard origin **`E800H`**), and every command is a **memory *read*** whose address bits *are*
the command. Data transfer is by **CPU wait state** (the board stalls the processor via `PRDY`
until the write shift register empties or the read shift register fills), never DMA and never
interrupts.

The **MDS-A** (1977) is **single density only** (FM), 256-byte sectors, ~89.6 KB/diskette. The
**MDS-A-D** (1978) is a superset: it keeps single density and adds **double density** (MFM),
512-byte sectors, **179.2 KB/diskette** (up to 4 drives → 716.8 KB), plus a double-sided `SS`
select. In gaining double density the MDS-A-D **reorganized the command interface** — the four
address "cases" mean different things on the two boards, the status bytes differ, and drive/side/
density/step moved into a new "Controller Orders" case. That reorganization is the point of
grouping them; see §2 and the differences table in §1.

This is a distilled emulation reference. Kit assembly, soldering, parts lists, power-supply and
cabinet options, the PLL/data-separator analog circuitry, and the checkout waveform tables are
omitted except where they set a software-visible value. The FM/MFM bit-cell encoding is
described only at the byte/sector level a controller model needs; North Star's format is **not**
IBM 3740 — it is North Star's own hard-sectored layout (§4).

---

## 1. What differs between MDS-A and MDS-A-D

| Feature | MDS-A (single density, 1977) | MDS-A-D (double density, 1978) |
|---|---|---|
| Recording | FM (single density) only | FM **and** MFM (density chosen per operation) |
| Sector data size | 256 bytes | 256 (SD) **or** 512 (DD) |
| Capacity / diskette | 35 × 10 × 256 ≈ **89.6 KB** | **179.2 KB** (DD); up to 4 drives → 716.8 KB |
| Sides | 1 (SA-400) | `SS` bit selects side of a double-sided diskette |
| **Address case map** | 0/1 = PROM · 2 = write-data · 3 = command | 0 = PROM · **1 = write-data** · **2 = Controller Orders** · 3 = command |
| Drive/side/density/step | folded into Case-3 command codes + `M1M0` | separate **Case-2 "Controller Orders"** register (`DD SS DP ST DS`) |
| Drive-select encoding | `M1M0` (binary: `01`,`10`,`11` = drives 1–3, `00` = none — **three drives**, see §5) via `CC=0` | `DS` field **one-hot**: `1`/`2`/`4`/`8` = drive 1/2/3/4, `0` = none |
| Status bytes | **two** (A, B) | **three** (A, B, C) — different bit layouts (§3) |
| Case-3 fields | `MO RD BST CC(3) M1 M0` | `DM(3) CC(3)` (`DM` = which byte on the DI bus) |
| Motor-on | `MO` bit in every Case-3 command | Case-3 command **code 5** ("turn on drive motors") |
| Interrupt control | `CC=3` (arm/disarm via `M0`) | Case-3 **codes 2 (disarm) / 3 (arm)** |
| Sector sync char | `FB` ×1 | `FB` ×1 (SD) / ×2 (DD) |
| Standard PROM origin | `E800H` (PROMs `LE820-3`, `RE820-3`, `SE8-1`) | `E800H` (PROMs `DWE-1`, `DSEL-E8-1`, `DPGM-E8-1`) |
| Auto-motor-off | **16 revolutions** (3.2 s) | **9.6 s** default (48 revolutions), jumper-selectable **3.2–38.4 s** |

Everything in §2–§5 is common to both boards except where a heading calls out SD or DD.

---

## 2. The memory-mapped command interface

The controller responds to references anywhere in a **1 K block** of CPU address space. Commands
are issued as **memory read cycles**; the address decodes as three fields:

```
 A15 .................. A0
| BS (high 6 bits)  | CASE (2) | low 8 bits = data / command / PROM address |
```

- **BS** — high 6 address bits, matched by the board-select PROM. When they match, the board is
  selected (its 1 K window). Standard window is **`E800H`–`EBFFH`**.
- **CASE** — the next two bits (address bits 9,8) pick the subcase.
- **low 8 bits** — meaning depends on the case: a PROM offset, the data byte to write, an orders
  byte, or a command byte.

Because a "write a byte to disk" is performed by *reading* an address whose low 8 bits hold the
data, an emulator must decode this board on the **memory-read path over its 1 K region**, not on
`IN`/`OUT`. A read that lands in the write-data case with the shift register still busy **hangs
the CPU** (wait state) until it drains.

### Case map — MDS-A (single density)

| Case | Meaning | Low 8 bits |
|---|---|---|
| 0 | Optional PROM addressing | PROM offset (**errata: now identical to Case 1** — both read the standard 256-byte PROM) |
| 1 | PROM addressing | PROM offset 0–255 |
| 2 | Write byte of data | data byte (hangs CPU until write shift register empty) |
| 3 | Controller command | `MO RD BST CC CC CC M1 M0` (see §3) |

### Case map — MDS-A-D (double density)

| Case | Meaning | Low 8 bits |
|---|---|---|
| 0 | PROM addressing | PROM offset 0–255 |
| 1 | Write byte of data | data byte (hangs CPU until write shift register empty) |
| 2 | Controller **Orders** | `DD SS DP ST | DS DS DS DS` — load the 8-bit order register |
| 3 | Controller **Commands** | `DM DM DM | CC CC CC` (top bit unused) |

**MDS-A-D Case-2 order fields:** `DD` density (1 = double, 0 = single, on write); `SS` side
(0 = bottom/only side, 1 = top); `DP` shared — step direction on a step (1 = in, 0 = out) *and*
write-precompensation enable (precomp iff `DP=1`) during a write; `ST` head-step signal level;
`DS` drive-select, **one-hot** (0 none, 1 drive 1, 2 drive 2, 4 drive 3, 8 drive 4).

---

## 3. Commands and status

### MDS-A (single density) — Case 3 command byte

`MO RD BST CC(3 bits) M1 M0`

- **MO** — 1 = turn drive motors on (if off) and reset the auto-motor-off timer; 0 = no action.
- **RD** — 1 = read a data byte from the read shift register onto the DI bus, hanging the CPU
  until the register is full; 0 = gate a status byte onto the DI bus instead.
- **BST** — 1 = gate **B-status**; 0 = gate **A-status** (only meaningful when `RD=0`).
- **CC** — command code:
  | CC | Action |
  |---|---|
  | 0 | Load drive-select register from `M1,M0` (`00` = no drive); lower head on selected drive. The register is held clear while the motors are off |
  | 1 | Write record — start a write-sector sequence |
  | 2 | Load track-step flip-flop from `M0` |
  | 3 | Load interrupt-armed flip-flop from `M0` |
  | 4 | No operation |
  | 5 | Reset sector flag |
  | 6 | Reset controller, raise heads, stop motors |
  | 7 | Load step direction from `M0` (1 = step in, 0 = step out) |

**MDS-A status bytes** (two):

```
A-Status:  SF WN 0 MO WRT BDY WP TR0
B-Status:  SF WN 0 MO | SP SP SP SP        (SP = sector position, 4 bits)
```

### MDS-A-D (double density) — Case 3 command byte

`DM(3 bits) CC(3 bits)`

- **DM** — what gets multiplexed onto the DI bus during the command: `1` = A-status, `2` =
  B-status, `3` = C-status, `4` = read data (may enter wait state until the read register fills).
- **CC** — command code:
  | CC | Action |
  |---|---|
  | 0 | No operation |
  | 1 | Reset sector flag |
  | 2 | Disarm interrupt |
  | 3 | Arm interrupt |
  | 4 | Set body (diagnostic) |
  | 5 | Turn on drive motors |
  | 6 | Begin write |
  | 7 | Reset controller, de-select drives, stop motor |

**MDS-A-D status bytes** (three):

```
A-Status:  SF IX DD MO WI RE SP BD
B-Status:  SF IX DD MO WR SP WP T0
C-Status:  SF IX DD MO | SC SC SC SC       (SC = sector counter, 4 bits)
```

### Status bit glossary (union of both boards)

| Bit | Meaning |
|---|---|
| SF | Sector Flag — a sector hole was detected (set by hardware, reset by software command) |
| WN / WI | Window — status/byte was read during the ~96 µs post-sector-pulse window |
| MO | Motor On |
| WRT / WR | Write — controller ready to receive a data byte to write (SD) / valid write in progress (DD) |
| BDY / BD | Body — sync character found; data bytes can now be read |
| WP | Write Protect — the selected drive's diskette is write-protected |
| TR0 / T0 | Track 0 — the selected drive is at track 0 |
| SP (single bit) | Spare (DD A/B-status) |
| SP (field) | Sector Position / Sector Counter — current sector (SD B-status / DD C-status) |
| IX | Index Detect — index hole seen during previous sector (DD) |
| DD | Double-Density Indicator — data being read is double-density encoded (DD) |
| RE | Read Enable — phase-locked loop enabled (DD) |

---

## 4. Disk data format

35 tracks, 10 hard-sectored sectors/track. Each sector's data is recorded starting **~96 µs after
its sector hole** is detected (the "window"). A read or write command must be issued within that
96 µs window.

| Field | Single density | Double density |
|---|---|---|
| Zeros (preamble) | 16 bytes | 32 bytes |
| Sync char (`FB`) | 1 byte | 2 bytes |
| Data | 256 bytes | 512 bytes |
| Check char | 1 byte | 1 byte |
| **Sector total** | **274 bytes** | **547 bytes** |

**Check character** — *not* a CRC. It is computed iteratively: start at zero, then for each data
byte, **XOR** it into the running value and **rotate the result left by one bit** (left-cycle).
The final value is the stored check byte. A verify/read compares the recomputed value against the
stored one.

**Write sequence** (software, per sector, after positioning): issue begin-write → wait for the
write-status bit → write 15 more bytes of zeros (the hardware writes the first zero byte itself) →
write the sync char(s) `FB` → write the data bytes while accumulating the check char → write the
check char → stop at the next sector pulse. **Read sequence:** wait for sync detection (body
mode; MDS-A reports an error if no sync within 16 byte-times) → read the data bytes while
accumulating the check char → read and compare the stored check char. **Verify** is the read
path but compares each byte against RAM instead of storing it.

---

## 5. Timing, motor, and interrupts

- **Rotation:** 300 RPM ⇒ **20 ms/sector** (10 sectors/rev). Confirmed by the drivers' "wait 2
  sector times (40 ms)" after a head step. ⚠ The MDS-A manual's spin-up step reads *"wait 1
  second (i.e. 5 sector times)"* — but 5 sector-times is only 100 ms; 1 s ≈ **5 revolutions**.
  Treat the parenthetical as a manual slip and honor the ~1 s spin-up if timing matters (see
  [[altairsim-plausible-but-wrong-timing]]).
- **Head step:** set step flip-flop → wait ≥10 µs → reset it → wait 2 sector times (40 ms) per
  track stepped.
- **Read loop deadline (MDS-A):** the per-byte read loop must complete in **< 64 µs** or data is
  lost.
- **Auto-motor-off:** MDS-A-D turns motors off **9.6 s** after the last activity by default
  (jumper table: 3.2 / 6.4 / 9.6 / 12.8 / 16.0 / 19.2 / 25.6 / 28.8 / 32.0 / 38.4 s).
- **Interrupts:** the stock North Star DOS is **not** interrupt-driven. The board can raise an
  interrupt on **any** S-100 vectored-interrupt / `PINT` line (jumper at the lower-left corner)
  on **every sector pulse** while armed. **⚠ Interrupts during a transfer corrupt data** — the
  stock software runs with interrupts disabled, and both errata sheets warn that a POLY-88 with
  the 4.0 monitor's continuous RTC interrupt must have that interrupt disconnected to use the
  disk at all.

### Facts from the MDS-A schematics and the MDS-A-D checkout tables

The manuals' prose leaves these open. The four MDS-A schematic pages (© 1976, pp. 27–30 of
the scan) and the MDS-A-D checkout tables (steps C1 and C3) settle them.

| Fact | Value | Where |
|---|---|---|
| Window | **96 µs** = 12 bit times of 8 µs | MDS-A schematic p. 3 ("sets WINDOW width to 12 bit times"); MDS-A-D C3 |
| Read enable `RE` (MDS-A-D) | goes high **480 µs** after the sector pulse | MDS-A-D C3 |
| Double-density indicator (MDS-A-D) | goes high at **512 µs** ("32us after RE") | MDS-A-D C3 |
| Body | at the end of the sync character(s): 96 µs + 17 × 64 µs (SD) or 34 × 32 µs (DD) = **1184 µs** | from the sector format; writing begins at the end of the window |
| Sector pulse with no hole to see | every **32.768 ms** (2 MHz ÷ 2¹⁶): motors off, or no diskette | MDS-A p. 3 `HOLE NOT FOUND`; MDS-A-D C1 "CC15 … 32ms square wave" |
| Sector counter | MDS-A: a **74LS160**, 0–9 always. MDS-A-D: 4-bit binary, 0–15 when it runs free ("SC03 … 500ms square wave") | MDS-A p. 3; MDS-A-D C1 |
| Index hole | **loads 9** into the sector counter, so the next sector hole starts sector 0. The index hole is in the last sector of the revolution. MDS-A-D `IX` ("index hole detected during previous sector") is therefore true in sector 0 | MDS-A p. 3 (1G: `D3..D0 = H L L H`, load on `INDEX HOLE`) |
| MDS-A drive select | two flip-flops; three lines decoded: `01` = drive 1, `10` = drive 2, `11` = drive 3. `00` = none. **Three drives, not four.** `MOTOR-ENB` false holds the register clear | MDS-A p. 4 |
| MDS-A motor off | counter 1F: "turns motors off after **16 revolutions**" = 3.2 s with a diskette, 5.2 s without (160 × 32.768 ms — the checkout text's "about 5 seconds") | MDS-A p. 3 |
| MDS-A status sampling | `MOTOR-SAMP` and `WINDOW-SAMP` are flip-flops clocked by the **leading edge of the read**, so a status byte shows the state *before* the command in the same read acts. The PROM's `LDA CTLMO+CTLNOP / ANI SAMO` depends on it | MDS-A p. 4 |
| MDS-A begin-write | `WRITE REQ` takes `WINDOW-SAMP` on its J input: the command is accepted only in the window. `WRITE`, `HUNT` and `BODY` are cleared by the next window | MDS-A p. 2 |
| MDS-A status bit 5 | a pad "jumper for diagnostic"; reads 0 | MDS-A p. 1 |
| MDS-A interrupt | `SECTOR FLAG` AND `INT ARM`, to one of `PINT`, `VI0`–`VI7` by jumper. It stays active until the sector flag is reset | MDS-A p. 1 |
| MDS-A reset | `RST` = `POC` or the reset command (`ROS`). The front-panel reset does not reach the board. It clears motor-enable, the step flip-flop and interrupt-arm, not the sector flag | MDS-A p. 1, p. 4 |
| Step | the step flip-flop drives the drive's `STEP` line; an SA-400 moves on the trailing edge of the pulse | MDS-A p. 4 |

### What the period software shows

- **DD write stream** (North Star DOS 5.1DQ, at `2230`): begin-write (`EB16`), wait for `WI`
  false, zeros to `E900`, `E9FB` once, `E9FB` a second time **only at double density**, the
  data, the check character. The read path reads 512 bytes and the check character with no
  byte to skip, so body starts after **both** sync characters.
- **DD read** (the same driver, at `217B`): wait for `RE`, delay 72 µs, then test `DD`
  against the density that the caller asked for.
- **⚠ Double-density North Star DOS reads `2000` before anything writes it.** The MDS-A-D
  PROM uses no RAM and loads the boot sector from byte 1 of its page (`<PA>00` is never
  written). The DOS keeps drive 1's track at `2000` and uses it on its first read. A value of
  `80` or more sends the head in from track 0 and the boot loads the wrong sectors. The DOS
  stores the right value before it steps, so a second boot works. The MDS-A PROM does not have
  this problem: it stores `59` (not initialized) in the table first.
- **Side 2** (Lifeboat CP/M 2.23 on a two-sided image): the tracks of side 2 follow side 1 in
  the image **from the innermost track out** (image track 35 = physical track 34, side 2).
- **North Star DOS prompts:** `*` single density (5.1S), `+` double density (5.1DQ).

---

## 6. Emulation notes and gotchas

- **This is a memory device, not a port device.** Unlike the 88-DCDD, 88-MDS, VersaFloppy,
  Cromemco FDC, and CompuPro Disk 1 — all `IN`/`OUT` port controllers — the North Star MDS
  decodes on the **CPU memory-read path** across a 1 K window (standard `E800H`), and the low
  address bits carry the data/command. Model it as a memory-mapped region that side-effects on
  read. This also means the on-board bootstrap PROM is simply the low 256 bytes of that window.
- **Hard-sectored media** (10 sector holes + index), same family as the 88-MDS minidisk and the
  88-DCDD — see [[altairsim-88mds-minidisk]] and [[hard-sector-blank-disks]] for the sim's
  hard-sector drive base and growable-image handling.
- **CPU wait-state (`PRDY`) transfer**, not DRQ polling and not DMA — the same stall-the-CPU
  pattern the VersaFloppy uses on its data port; see [[altairsim-versafloppy-board]]. A read of
  the write-data case while the shift register is busy, or of the read case before the register
  fills, must hold the CPU until ready.
- **The check byte is a rotate-left XOR, not a CRC** — anyone porting a generic FDC verifier will
  compute the wrong value. Re-derive it exactly: `chk = 0; for b in data: chk = rol8(chk ^ b)`.
- **The two boards' command interfaces are not interchangeable.** The address-case meanings, the
  status-byte layouts, drive-select encoding (binary `M1M0` vs one-hot `DS`), and where motor-on
  and interrupt-arm live all differ (§1). A single decoder cannot serve both without branching on
  board type.
- **Emulated** as the `mdsa` and `mdsad` boards (`src/boards/northstar-mds.{h,cpp}`,
  `docs/boards/northstar-mds.md`).
- **Standard origin `E800H`.** The MDS-A-D bootstrap is entered by jumping to `E800H`, the
  MDS-A bootstrap (assembled for case 1) at `E900H`; non-standard PROM
  origins move both the PROM and the whole 1 K window (`xx` in a PROM label is the two high hex
  digits of the origin).
- **CPU-agnostic bootstrap.** The on-board PROM holds 8080/Z80 machine code and the board is
  explicitly specified for both; nothing here assumes one CPU — relevant when driven from the
  sim's shared 8080/Z80 core ([[altairsim-z80-isa-next]]).

---

## 7. Key facts at a glance

| | MDS-A | MDS-A-D |
|---|---|---|
| Density | FM (single) | FM + MFM (single/double) |
| Geometry | 35 trk × 10 sec × 256 B | 35 trk × 10 sec × 256/512 B |
| Capacity/diskette | ≈ 89.6 KB | 179.2 KB (DD) |
| Drives | up to 3 | up to 4 |
| Interface | 1 K memory window, command-by-read | same |
| Standard origin | `E800H` | `E800H` |
| Controller clock | 4 MHz xtal ÷2 = 2 MHz | 4 MHz xtal ÷2 = 2 MHz |
| Transfer | CPU wait-state (`PRDY`) | CPU wait-state (`PRDY`) |
| Sync char | `FB` ×1 | `FB` ×1 (SD) / ×2 (DD) |
| Check | rotate-left XOR | rotate-left XOR |
| Rotation | 300 RPM, 20 ms/sector | 300 RPM, 20 ms/sector |
| Status bytes | A, B | A, B, C |
| Interrupts | optional per-sector-pulse (any VI/PINT line), off by default | same, + arm/disarm command codes |
