# cadzilla — an HD63484 ACRTC graphics board with a Bt453 RAMDAC

**Status:** milestone 1 — registers, FIFOs, a drawing subset, scan-out through the LUT into a
fixed VESA monitor. Built 2026-09-18; the monitor model and the 8 bpp / GAI +8 wiring added
2026-09-19.

## The real hardware

There is no period product called cadzilla: it is a board of **Todd's own design**, assembled
from two real chips of the mid 1980s the way a CAD-station card of that era was.

- **Hitachi HD63484 ACRTC** (Advanced CRT Controller, 1984). A CRT controller with a
  microcoded *drawing processor* and its own frame-buffer interface: up to 2 MB of DRAM on the
  card that the host never addresses. The host sets ~30 timing and display registers, then draws
  by writing 16-bit *commands* into an 8-word FIFO — move, line, rectangle, polyline, polygon,
  circle, ellipse, arc, paint, pattern, copy — in logical X-Y pixel coordinates at 1, 2, 4, 8 or
  16 bits per pixel. Four logical screens (upper, base, lower, window), zoom, cursors, a light
  pen, DMA to the host. Interfaces as an 8- or 16-bit peripheral occupying **two locations**,
  selected by its RS pin.
- **Brooktree Bt453** (1986). A 256 × 24 color-palette RAM with three 8-bit video DACs. Eight
  pixel inputs (P0–P7) select an entry every pixel clock; three overlay registers on two more
  inputs; an 8-bit MPU port with two command inputs, so it occupies **four locations**.

The board's own decisions, which no chip made — and every one of them is visible from the bus:

| Decision | cadzilla |
|---|---|
| ACRTC bus mode | 8-bit (DACK\* strapped at reset), the only mode an S-100 8080/Z80 can use |
| **Shift register** | wired for **8 bits per pixel and 8 words per display fetch**: each display cycle the board fetches eight consecutive words (128 bits) at the address the ACRTC puts on MAD and shifts them out as sixteen 8-bit pixels, low byte of the low word first. This is a hardware fact, not a register — see *Programming model* below |
| **Monitor** | a fixed-frequency VESA display chosen by the `mode` strap: **640x400, 640x480 (default), 800x600, 1024x768**. The frame is always the mode's size; the ACRTC's picture is placed in it by HDS/VDS against the mode's porches |
| **Access modes** | single (OMR ACM = `0x`) and interleaved (ACM = `10`). Superimposed (`11`) is not wired |
| Frame memory fitted | `vram` kilobytes, default **2048** — 2 MB, the full 1 M words the ACRTC addresses (1024x768 at 8 bpp needs 768 KB); a smaller fit aliases above its size |
| Pixel bus to the DAC | P0–P7 from the shift register; the Bt453 sees exactly the byte the frame memory holds |
| Overlay inputs | OL0, OL1 tied low: the overlay registers can be loaded but nothing selects them |
| IRQ\* | not wired to the bus (a later milestone) |

### Programming model

The ACRTC does not serialize pixels — it only puts a word address on its MAD bus once per
display cycle and advances that address by GAI words for the next one. What the board does
with each address is the board's business, and cadzilla's is fixed: **eight words, sixteen
8-bit pixels**. So a program must tell the chip the same thing, or the two disagree:

- **CCR GBM = `011` (8 bits per pixel)**, so the drawing engine's pixel arithmetic matches the
  bytes the shift register emits. At any other GBM the drawing commands still run — the chip
  has no idea what the board fetches — but they pack pixels the board will not unpack, and the
  picture is scrambled exactly as it would be on the real card.
- **OMR GAI = `011` (+8 words per display cycle)**, so the ACRTC steps over exactly the eight
  words the board just fetched. At GAI +1 the board still fetches eight words per cycle from an
  address that only moved by one: the raster shows each eight-word run overlapping the last.
- **OMR ACM** single or interleaved. In interleaved mode the ACRTC takes two memory cycles per
  display cycle, so **every horizontal register is in doubled units** for the same picture
  (`HC`, `HSW`, `HDS`, `HDW`, `HWS`, `HWW`) and one memory cycle is worth 8 pixels of the
  frame instead of 16.

`SHOW <id>` has a `wiring` line that reads `ok` or names which of GBM / GAI / ACM is off.

### The monitor modes

VESA timings in the board's units — memory cycles of 16 pixels (single access mode) and
rasters. Where a VESA porch is not a whole number of cycles the board's timing rounds it and
keeps the line total, as a timing PROM on a real card would.

| `mode` | Pixel clock | H: sync / back porch / active / front porch (cycles) | V: sync / back porch / active / front porch (rasters) |
|---|---|---|---|
| `640x400` | 25.175 MHz | 6 / 3 / 40 / 1 = 50 | 2 / 35 / 400 / 12 = 449 |
| `640x480` | 25.175 MHz | 6 / 3 / 40 / 1 = 50 | 2 / 33 / 480 / 10 = 525 |
| `800x600` | 40.000 MHz | 8 / 6 / 50 / 2 = 66 | 4 / 23 / 600 / 1 = 628 |
| `1024x768` | 65.000 MHz | 8 / 10 / 64 / 2 = 84 | 6 / 29 / 768 / 3 = 806 |

The register values that put a full picture in each frame (single access mode; the manual's
"set to N−1" already applied). For interleaved mode double `HC`, `HSW`, `HDS` and `HDW`
(`HC = 2·total − 1`, `HSW = 2·sync`, `HDS = 2·back porch − 1`, `HDW = 2·active − 1`) and set
ACM = `10`:

| `mode` | `HSR` r82 (HC, HSW) | `HDR` r84 (HDS, HDW) | `VSR` r86 (VC) | `VDR` r88 (VDS, VSW) | `SSW` r8A (SP1) | `MWR1` rCA |
|---|---|---|---|---|---|---|
| `640x400` | `$3106` (49, 6) | `$0227` (2, 39) | `$01C1` (449) | `$2202` (34, 2) | `$0190` (400) | `$0140` (320) |
| `640x480` | `$3106` (49, 6) | `$0227` (2, 39) | `$020D` (525) | `$2002` (32, 2) | `$01E0` (480) | `$0140` (320) |
| `800x600` | `$4108` (65, 8) | `$0531` (5, 49) | `$0274` (628) | `$1604` (22, 4) | `$0258` (600) | `$0190` (400) |
| `1024x768` | `$5308` (83, 8) | `$093F` (9, 63) | `$0326` (806) | `$1C06` (28, 6) | `$0300` (768) | `$0200` (512) |

Then `CCR = $0300` (8 bpp), `OMR = $4030` (STR, GAI +8; `$4038` interleaved), `DCR = $4000`
(SE1), `SAR1 = 0`, and an `ORG` wherever the program wants its origin — on the bottom raster
(`DPA = (height−1)·MW`) if it wants +Y to be up on the screen.

**Placement.** The ACRTC's display starts `HDS` memory cycles after HSYNC's rising edge; the
monitor's picture starts a fixed number of cycles after the same edge — the mode's back porch.
A picture programmed with `HDS = back porch − 1` fills the frame from the left edge; one cycle
later it is 16 pixels to the right (8 in interleaved mode) with its last cycle off the edge, one
cycle earlier it is clipped on the left. Vertically the same with `VDS` and the vertical back
porch. `SHOW`'s `picture` line reports the programmed size and where its top-left corner lands.

## Sources

| Source | Path | Authority |
|---|---|---|
| Hitachi *HD63484 ACRTC* datasheet, 47 pp. | `reference/Hitachi HD63484 ACRTC.md` | **Authoritative** for the register file (Figure 5), the drawing parameter registers (Table 1), the opcode table (Table 4) and the status register |
| Hitachi *HD63484 ACRTC User's Manual* | companion (see `docs/sources.md`) | The semantics: 8-bit mode byte order (§5.2, §6.2), the FIFO and status rules (§5.3), GBM/OMR/DCR field meanings (§5.5–5.7), timing and display-control RAM (§5.8–5.9), the operation / color / area modes (§6.6) and every command's worked example (ORG-3, CLR-3/4, WPR-2, RPR-2, WPTN-2, RPTN-2, ALINE-2, ARCT-2, AFRCT-1, DRD/DWT-2) |
| Hitachi *HD63484 ACRTC Application Note* #U90, April 1986 | bitsavers | The reset state (Table 7-1) and the initialization sequence (§7.2) |
| Brooktree *Bt453*, L453001 Rev. J | `reference/Brooktree Bt453 RAMDAC.md` | **Authoritative** for the four locations (Table 1), the address/phase logic (Table 2) and the pixel/overlay truth table (Table 3) |
| MAME `src/devices/video/hd63484.cpp` | `LICENSE-MAME-HD63484` | **Structural basis only** of the ACRTC model, per the terms in `docs/sources.md`. Where it and the Hitachi documents disagreed, the documents won — see Quirks |

## Register reference

Six I/O ports, no memory. ACRTC at `port` (even, default `0x70`); Bt453 at `dac` (a multiple of
four, default `0x74`).

| Addr | OUT (write) | IN (read) |
|---|---|---|
| `BASE+0` | **Address register (AR)**: which control register `BASE+1` reaches (`r00`–`rFF`) | **Status register (SR)**: `CER ARD CED LPD RFF RFR WFR WFE` (D7..D0) |
| `BASE+1` | the control register AR names, **one byte**: even AR = high byte, odd AR = low byte. AR = 0/1: a **command word into the write FIFO**, high byte then low | the same register's byte; AR = 0/1: the next byte **out of the read FIFO**, high then low |
| `DAC+0` | Bt453 address register (which LUT entry the next R,G,B cycles touch); resets the color phase to red | the address register |
| `DAC+1` | color palette RAM: red, then green, then blue; the entry is written on the blue cycle and the address increments | R, G, B of the entry, one per read; increments after blue |
| `DAC+2` | the address register again (an alias; there is no separate read-mode register on this part) | the address register |
| `DAC+3` | overlay registers 1–3 (ADDR1..0 = 01, 10, 11), same R,G,B protocol | the same |

**Status register.** `WFE` write FIFO empty (up to 8 words may be written), `WFR` write FIFO
not full (one word may), `RFR` read FIFO has a word, `RFF` read FIFO full, `CED` command end —
the chip will take a new command (cleared when one is written), `ARD` area detect (set by the
AREA modes, cleared by RPR or ABT), `CER` command error — an undefined opcode, a bad parameter,
or an opcode this model recognizes but does not execute (cleared only by ABT). After RESET\* and
after ABT: `$23`.

**Control registers** (through AR). `r02` CCR — bit 15 ABT, 14 PSE, 13 DDM, 12 CDM, 11 DRC,
10–8 **GBM** (000 = 1 bpp, 001 = 2, 010 = 4, 011 = 8, 100 = 16), 7–0 interrupt enables matching
the SR bits. `r04` OMR — bit 15 M/S, **14 STR** (start), 13 ACP, 12 WSS, 11–10 CSK, 9–8 DSK,
7 RAM, **6–4 GAI** (words per display cycle: 000 = 1, 001 = 2, 010 = 4, 011 = 8), 3–2 ACM,
1–0 RSM. `r06` DCR — bit 15 DSP, **14 SE1** (base screen shown), 13–12 SE0 (upper: 1x
enabled, 11 shown), 11–10 SE2 (lower), 9–8 SE3 (window), 7–0 ATR. Timing RAM `r80`–`r9F`:
`r84` HDR (HDS `<<8` | HDW, both in memory cycles, value = cycles − 1), `r88` VDR (VDS `<<8` |
VSW), `r8A`/`r8C`/`r8E` SP1/SP0/SP2 (rasters of the base/upper/lower screens), `r92` HWR,
`r94`/`r96` VWS/VWW. Display-control RAM `rC0`–`rEF`: per screen *n* (0 upper, 1 base, 2 lower,
3 window) `rC2+8n` MWR (bit 15 CHR, 11–0 memory width in words), `rC4+8n`/`rC6+8n` SAR (start
address high nibble / low word). Accesses to `r80`–`rFF` auto-increment AR by one; `r00`–`r7F`
do not.

**Commands** (16-bit words into the FIFO, then their parameters; manual Figure 6.1):

| Opcode | Mnemonic | Parameters | What it does |
|---|---|---|---|
| `$0400` | ORG | DPH, DPL | set the origin: screen number, word address, dot address; CP = (0,0) |
| `$0800+RN` | WPR | D | write drawing-parameter register RN (CL0 `00`, CL1 `01`, CCMP `02`, EDG `03`, MASK `04`, PRC `05`–`07`, area `08`–`0B`, RWP `0C`–`0D`) |
| `$0C00+RN` | RPR | — | read it into the read FIFO (DP `10`–`11` and CP `12`–`13` too); clears ARD |
| `$1800+PRA` | WPTN | n, D1..Dn | write the pattern RAM from word PRA; **in 8-bit mode n counts bytes** |
| `$1C00+PRA` | RPTN | n | read n pattern words into the read FIFO |
| `$4400` / `$4800` / `$4C00+MM` | RD / WT / MOD | — / D / D | one word at RWP; RWP++ (MOD under MASK) |
| `$5800` / `$5C00+MM` | CLR / SCLR | D, AX, AY | fill (AX+1)×(AY+1) words from RWP; RWPe = RWP's column on the last raster |
| `$2400` / `$2800` / `$2C00+MM` | DRD / DWT / DMOD | AX, AY | a block through the FIFOs under program control |
| `$8000` / `$8400` | AMOVE / RMOVE | X, Y | move CP |
| `$8800` / `$8C00` + fields | ALINE / RLINE | X, Y | line from CP to Pe, **Pe not drawn**, CP = Pe |
| `$9000` / `$9400` + fields | ARCT / RRCT | X, Y | the rectangle CP–(X,Y), X direction first, CP unchanged |
| `$9800` / `$9C00`, `$A000` / `$A400` + fields | APLL / RPLL, APLG / RPLG | n, points | polyline (CP = end) / polygon (closes on CP) |
| `$C000` / `$C400` + fields | AFRCT / RFRCT | X, Y | fill the rectangle with the pattern; CP.y = one past the far edge |
| `$CC00` + fields | DOT | — | the pixel at CP |

The low byte of a graphic command is `AREA` (bits 7–5), `COL` (4–3), `OPM` (2–0): eight
operation modes (replace, OR, AND, EOR, and replace if P = CCMP / P ≠ CCMP / P < CL / P > CL),
four color modes (pattern bit selects CL0/CL1; CL1 or suppress; CL0 or suppress; pattern RAM
direct), eight area modes (none; stop on exit with ARD+CED; suppress outside without / with ARD;
and the same three for *inside*).

## How it is simulated

- **Bus cycles decoded**: `IN`/`OUT` on `port`, `port+1`, and `dac`..`dac+3`. No memory.
- **Two chips, no bus-level timing**: `Hd63484` (`src/chips/hd63484.h`) and `Bt453`
  (`src/chips/bt453.h`) hold all the state; the board forwards RS = A0 to one and C1C0 = A1A0
  to the other. A drawing command executes **at the instant its last parameter lands** — the
  FIFO drains immediately, so `WFE` is the steady state.
- **`pump()`**: the three gates every video board here uses — did either chip change anything
  (a drawing, a register, a LUT entry)? does the host want a frame? — then the board builds the
  **monitor's** frame: a Surface the size of the `mode`, black, into which it runs its own shift
  register. For every raster of the frame it asks the ACRTC which background raster (if any) and
  which window raster fall there (`Hd63484::backgroundRaster` / `windowRaster`, VDS and VWS
  against the mode's vertical back porch) and, for each memory cycle of `HDW`/`HWW`, fetches
  eight words at the ACRTC's address — advancing by `gaiWords()` per display cycle — and places
  sixteen 8-bit pixels at the cycle's frame x (HDS/HWS against the horizontal back porch),
  clipped to the frame. The Bt453's 256-entry table goes to `Display::setPalette()`. Nothing is
  translated: the surface *is* the pixel bus and the palette *is* the RAMDAC. Off (STR or SE1
  clear) the frame is black -- the monitor is there from power-on, signal or not, so the
  window opens at the prompt like the Dazzler's and shows the picture once a program starts
  the chip.
- **`properties()`**: straps `port`, `dac`, `mode` (the monitor), `vram` (kilobytes, a power of
  two 8–2048; refits the frame memory), `width` (the window); live, read-only `video`, `picture`
  (programmed size and position in the frame), `wiring` (GBM/GAI/ACM against the board) and
  `status` (the SR).
- **Interrupts**: none wired. `Hd63484::irq()` answers what IRQ\* would be; a later milestone
  adds the `interrupt` strap.
- **Snapshot**: both chips — registers, FIFOs, a command in flight, the drawing state, the
  pattern RAM, **the whole frame memory** (no memory board holds it) and the LUT.

### Reset

- `Reset::PowerOn`: the ACRTC's registers, pattern RAM and frame memory zero, then RES\*; the
  Bt453's table black, address 0, phase red.
- `Reset::Bus`: RES\* to the ACRTC only — SR `$23`, ABT set, STR and M/S clear, FIFOs cleared,
  everything else kept, so the picture blanks (nothing is displaying) and a driver that restarts
  the chip finds its timing intact. **The Bt453 has no reset pin**: the LUT survives.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| 8-bit mode: an even AR is the high byte, an odd AR the low byte; `r80`–`rFF` auto-increment, `r00`–`r7F` do not (manual 5.2) | a driver's one-address block load of the timing RAM writes one register 30 times, or its two-write CCR load lands both bytes in one |
| The FIFOs are **8 words**, and WFE/WFR/RFR/RFF track them (manual 5.3) — MAME's model has 16 | a driver that writes eight words on WFE and then polls WFR sees the wrong flag |
| CED clears the moment a command word is written and sets when it ends; CER only ever clears on ABT | a driver polling CED between commands hangs or runs ahead |
| The end point of a line is **not drawn** (ALINE-1); a rectangle's four edges therefore draw each corner exactly once | an EOR-drawn frame leaves its corners lit after being undrawn |
| Dot address *n* is bits `n·bpp` upward from the LSB (ORG-3: DPD=4 at 4 bpp is "bit position 4–7"), and +Y is MW words **lower** in memory | the picture is mirrored, or upside down, or both |
| A line's pattern scan starts at **PPX** (manual 6.8.3) — MAME starts at PSX | a dashed line drawn with a phase offset comes out in the wrong phase |
| RWPe after CLR/DRD/DWT is RWP's column on the **last raster** (CLR-4: `$56`, AY = −6, MW `$10` → `$B6`) — MAME leaves it one raster further | a driver that clears in strips by chaining CLRs skips or overlaps a raster |
| In 8-bit mode WPTN's *n* counts **bytes**, so it is twice the word count (WPTN-1) | half the pattern loads and the other half is parsed as the next command |
| A DRD "goes into an indefinite wait state after the last transfer" — CED never sets; ABT ends it (manual 6.5) | a driver waiting on CED after a DRD hangs on real silicon and would not here |
| The shift register is 8 bpp × 8 words whatever CCR/OMR say: a wrong GBM packs pixels the board will not unpack, a wrong GAI makes each fetch overlap the last | an off-spec program shows a coherent picture here and garbage on the card, or the reverse |
| In interleaved mode every horizontal register is in doubled units and a memory cycle is 8 frame pixels | the picture is half as wide as intended, or the timing is off by half a line |
| The picture is placed by HDS/VDS against the mode's porches; the frame does not grow to fit | a program that lands one cycle early is clipped, not shifted |
| A Bt453 address-register **write** resets the R/G/B phase; a **read** does not; both `DAC+0` and `DAC+2` are the same register; the address increments after the blue cycle and wraps `$FF` → `$00` | colors land one channel out of phase, or a Bt458 driver's "read mask" write moves the address |

## Limitations and deliberate departures

- **No time.** Drawing is instantaneous, the raster counter reads 0, there is no DTACK and no
  wait state. A guest that times a command, or that syncs to the raster to avoid flicker, sees
  an infinitely fast chip and an unmoving beam. The status bits a guest *polls* (the FIFO
  flags, CED) are exact.
- **Commands not executed**: CPY, SCPY, CRCL, ELPS, AARC, RARC, AEARC, REARC, PAINT, PTN,
  AGCPY, RGCPY. They are recognized and their parameters consumed — the command stream stays in
  step — and **CER is set**, so a guest can tell. DRD/DWT/DMOD run only in the manual's
  "under program control" mode (no DMAC on the board) and only in the positive X/Y directions.
- **Scan-out**: graphic screens only (CHR = 1 character screens are scanned as graphic), the
  three background screens stacked and the window over them, non-interlaced; single and
  interleaved access only (superimposed mode's second phase is not fetched). The monitor is the
  four listed VESA modes, each at the one refresh rate in the table; the ACRTC's own HC/VC and
  the sync widths are accepted but not checked against the mode — a timing a real monitor would
  lose lock on shows here as a picture in the wrong place. No zoom, no cursors, no light pen,
  no blink, no smooth scroll (SDA is ignored), no DISP/CUD skew.
- **No interrupts**: CCR's enables are honored by `irq()` but nothing raises a bus interrupt.
- **The frame memory is zero at power-on**; real DRAM is not.
- **Bt453**: the analog side (sync, blank, the CS\*-blanks-video artifact) is not modeled and
  cannot be observed from the bus.

## Verification

`tests/test_bt453.cpp` drives the DAC through Tables 1–2 of its data sheet.
`tests/test_hd63484.cpp` (22 sections) drives the ACRTC through its two locations: reset state,
8-bit register access, FIFO/status transitions, every worked example the manual gives (WPR-2,
RPR-2, WPTN-2, RPTN-2, ORG-3, CLR-3/4, ALINE-2, ARCT-2, AFRCT-1, DRD/DWT-2), the operation,
color and area modes, scan-out geometry, the window overlay, and a snapshot round trip.
`tests/test_cadzilla.cpp` proves the board: the six-port decode both directions, the ports
reaching the chips, and an **end-to-end 640x480 picture** — LUT loaded through the four DAC
ports, a rectangle, a line and a dot drawn through the two ACRTC ports, the frame read back
with `CHECK_FRAME_OPTS` sampled every 32nd pixel for a reader and `CHECK_FRAME_PIXELS`
against an oracle for **every one of the 307,200 pixels** (`tests/framecheck.h`), its colors
resolved through the palette; the picture's placement by HDS/VDS (one
cycle right, one cycle clipped, one raster down, a narrower picture); the wiring — GAI +1
repeating each fetch and a 4 bpp dot landing in the wrong nibble, as the hardware would, with
`wiring` naming each; **1024x768 in interleaved mode** with the doubled registers, sampled
every 64th pixel and checked pixel by pixel; every mode's frame filled by one `AFRCT` and
every pixel of it checked; the window at
HWS/VWS; the `mode` strap re-opening the frame at 800x600; then a palette-only change moving
the frame, a snapshot repainting it, and RESET\* leaving a black frame while the LUT survives.

No period software exists for this board; the register table above is what a program would
load, and `machines/cadzilla.toml`'s header walks the 640x480 case from the monitor prompt.

## References

- `reference/Hitachi HD63484 ACRTC.md`, `reference/Brooktree Bt453 RAMDAC.md`
- `docs/sources.md` — the sourcing terms, including MAME's role
- `docs/devguide/video-board.md` — how a video board is written, with this one as the example
