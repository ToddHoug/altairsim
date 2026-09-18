# Brooktree Bt453 — 66 MHz Monolithic CMOS 256 × 24 Color Palette RAMDAC

Source: [Brooktree_Bt453.pdf](https://www.ardent-tool.com/datasheets/Brooktree_Bt453.pdf) —
Brooktree Corporation, *Bt453 66 MHz Monolithic CMOS 256 x 24 Color Palette RAMDAC*, document
**L453001 Rev. J**, "PC Graphics" section 4 of a Brooktree product data book, 9 pp. of content
(4-3 … 4-11). Scanned image, no text layer — read as page images. The same part appears in the
*1991 Brooktree Product Databook* and the *1990 Graphics and Image Products Application
Handbook* (both on bitsavers). This is the emulation reference for `src/chips/bt453.{h,cpp}`,
the color look-up table on the `cadzilla` video board (`docs/boards/cadzilla.md`).

The Bt453 is a **256-entry, 24-bit color look-up table with three 8-bit video D/A converters
on one chip**. Eight pixel inputs (P0–P7) select one of the 256 entries every pixel clock;
the entry's three bytes drive the red, green and blue DACs. Three more 24-bit **overlay**
registers, selected by two overlay inputs (OL0, OL1), take precedence over the palette for
cursors, grids and menus. The MPU side is an ordinary 8-bit bus with two command inputs,
asynchronous to the video side. Nominal video clock 66 MHz or 40 MHz (two speed grades),
RS-343A/RS-170-compatible outputs, 40-pin DIP or 44-pin PLCC, +5 V, ~1 W.

**What it is not.** It has **no pixel read-mask register** and **no command register** — those
are on the Bt458 / Bt471 / Bt476 family. A driver that writes a "read mask" to the Bt453's
third location is writing the address register (see Table 1: locations `10` and `00` are the
*same* register).

---

## 1. MPU interface (datasheet pp. 4-4 … 4-5)

Pins: **D0–D7** (data, D0 = LSB), **CS\*** (chip select, active low), **RD\*** (read), **WR\***
(write), **C0, C1** (command inputs — "which register"). Data is latched into the device on the
rising edge of WR\* or CS\*, whichever occurs first; a read is valid while CS\* and RD\* are both
low (Figure 1). All TTL-compatible. On a board, C1 and C0 are normally the two low address
lines, so the part occupies **four consecutive I/O locations**.

### Table 1 — Control input truth table

| C1 | C0 | Addressed by MPU |
|:--:|:--:|---|
| 0 | 0 | address register |
| 0 | 1 | color palette RAM |
| 1 | 0 | address register |
| 1 | 1 | overlay registers |

Locations `00` and `10` both reach the one address register — there is no separate "read-mode"
and "write-mode" address register on this part.

### The address register (ADDR)

The address register is **10 bits wide, of which the MPU sees 8**:

| Bits | Name | What it is |
|---|---|---|
| ADDR0–7 | the address | which palette entry (0–255) or which overlay register the next color cycles touch. Written and read through locations `00`/`10`. ADDR0 corresponds to D0. |
| ADDRa, ADDRb | the color phase | a **modulo-3 counter** that tracks which of red, green, blue the next data cycle is. **Not accessible to the MPU.** |

### Table 2 — Address register operation

| Field | Value | C1 | C0 | Addressed by MPU |
|---|---|:--:|:--:|---|
| ADDRa,b (counts modulo 3) | 00 | x | 1 | red value |
| | 01 | x | 1 | green value |
| | 10 | x | 1 | blue value |
| ADDR0–7 (counts binary) | `$00`–`$FF` | 0 | 1 | color palette RAM |
| | `xxxx xx00` | 1 | 1 | reserved |
| | `xxxx xx01` | 1 | 1 | overlay color 1 |
| | `xxxx xx10` | 1 | 1 | overlay color 2 |
| | `xxxx xx11` | 1 | 1 | overlay color 3 |

### Writing a color

1. Write the entry's address to the address register (`C1C0 = 00` or `10`). **This resets
   the modulo-3 counter to red.**
2. Perform **three successive write cycles** to the color palette RAM (`01`) or the overlay
   registers (`11`): 8 bits of red, then green, then blue. The three bytes are concatenated
   into one 24-bit word and written to the addressed location **during the blue cycle**.
3. The address register then **increments** to the next location, and the MPU may continue
   with another red/green/blue sequence without rewriting the address.

### Reading a color

1. Write the address to the address register (resets the phase to red).
2. Perform **three successive read cycles** (red, green, blue) from `01` or `11`.
3. **Following the blue read cycle** the address register increments; the MPU may read the
   next location's red/green/blue directly.

### Rules that bite

- **A write to the address register resets ADDRa,b to zero (red). A read of the address
  register does NOT.** So a driver may read ADDR back mid-sequence without losing its place;
  a driver that re-writes ADDR "to be safe" has restarted the sequence at red.
- **Wrap.** "The address register resets to `$00` following a blue read or write cycle to
  RAM location `$FF`." A 256-entry load is therefore one address write and 768 data writes.
- **Overlay addressing uses ADDR1..0 only.** While the MPU is accessing the overlay registers
  (`C1C0 = 11`) the six most significant bits of the address register (ADDR2–7) are ignored.
  `xx00` is reserved. (The datasheet does not say what a write to the reserved location does;
  the model treats it as a fourth, never-displayed register so a driver that touches it is
  neither refused nor corrupting a real entry.)
- **CS\* low blanks the video.** "Any time the CS\* input is a logical zero, the video outputs
  are forced to the black level." An MPU access therefore visibly blanks the picture for its
  duration on real hardware; nothing on the MPU side can observe that, and the model ignores
  it.
- Increment-after-blue applies to the **overlay** registers too, modulo the two-bit field.

---

## 2. Video (frame buffer) interface (pp. 4-6 … 4-8)

Pins: **CLOCK** (pixel clock; every input below is latched on its rising edge), **P0–P7**
(pixel select, P0 = LSB), **OL0, OL1** (overlay select, OL0 = LSB), **SYNC\***, **BLANK\***,
outputs **IOR, IOG, IOB** (current-source DACs) and **ISYNC** (sync current, normally tied to
IOG), plus **FS ADJUST**, **VREF**, **COMP** (analog reference and compensation), VAA/GND.
Unused P/OL inputs are to be tied to GND.

### Table 3 — Pixel and overlay control truth table

| OL1 | OL0 | P0–P7 | Addressed by frame buffer |
|:--:|:--:|:--:|---|
| 0 | 0 | `$00` | color palette RAM location `$00` |
| 0 | 0 | `$01` | color palette RAM location `$01` |
| : | : | : | : |
| 0 | 0 | `$FF` | color palette RAM location `$FF` |
| 0 | 1 | `$xx` | overlay color 1 |
| 1 | 0 | `$xx` | overlay color 2 |

Table 3 as printed stops at overlay 2; overlay 3 (addressable from the MPU at `xx11`, Table 2,
and the third of the "3 × 24 overlay palette" on the front page) is selected by `OL1 OL0 = 11`
by the same encoding — recorded here as an inference from the two tables, not a quoted row.
**When either overlay input is set, P0–P7 are ignored.**

The addressed location's 24 bits go to the three DACs. **When BLANK\* is low the pixel and
overlay inputs are ignored** and the outputs go to the blanking level; SYNC\* low switches
a 40 IRE sync current off the ISYNC output and affects nothing else (Table 4).

### Table 4 — Video output truth table (IOG with ISYNC tied to it; IOR/IOB)

| Description | IOG (mA) | IOR, IOB (mA) | SYNC\* | BLANK\* | DAC input data |
|---|---|---|:--:|:--:|---|
| WHITE | 26.67 | 19.05 | 1 | 1 | `$FF` |
| DATA | data + 9.05 | data + 1.44 | 1 | 1 | data |
| DATA – SYNC | data + 1.44 | data + 1.44 | 0 | 1 | data |
| BLACK | 9.05 | 1.44 | 1 | 1 | `$00` |
| BLACK – SYNC | 1.44 | 1.44 | 0 | 1 | `$00` |
| BLANK | 7.62 | 0 | 1 | 0 | `$xx` |
| SYNC | 0 | 0 | 0 | 0 | `$xx` |

(Typical, full-scale IOG = 26.67 mA, RSET = 280 Ω, VREF = 1.235 V, 75 Ω doubly-terminated
load; Figure 3 gives the 7.5 IRE black-level setup and 92.5 IRE white.) For emulation the
only fact that matters is the first column of the palette: **`$00` is black, `$FF` is
full-scale, and the DAC is linear** — an 8-bit palette byte maps 1:1 onto an 8-bit host
color channel.

---

## 3. What the model keeps

`Bt453` (`src/chips/bt453.h`) is the MPU side and the look-up: four locations by `C1C0`,
the 256 × 3 palette RAM, the 4 × 3 overlay registers (three real, one reserved), the 8-bit
address and its hidden modulo-3 phase, with exactly the write-reset / read-preserve / wrap /
increment-after-blue rules of §1. `lookup(p, ol)` is Table 3 — the color a pixel value
would put on the wire — and `palette()` hands all 256 entries to the host's `Display::
setPalette()` in one call, which is the whole reason an `Indexed8` surface plus a palette
is the right shape for a RAMDAC board. Nothing analog is modeled: no sync, blank, current
levels or the CS\*-blanks-video artifact, none of which the MPU can observe.

## 4. Pinout (for the board's schematic, p. 4-10)

40-pin DIP: 1–8 D0–D7, 9 GND, 10 VAA, 11–18 P7…P0, 19 OL1, 20 OL0, 21 BLANK\*, 22 SYNC\*,
23 CLOCK, 24 C0, 25 C1, 26 CS\*, 27 RD\*, 28 WR\*, 29–31 VAA, 32 GND, 33 IOB, 34 ISYNC,
35 IOG, 36 IOR, 37 COMP, 38 VAA, 39 VREF, 40 FS ADJUST.
