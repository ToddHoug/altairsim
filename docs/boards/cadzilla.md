# cadzilla — an HD63484 ACRTC graphics board with a Bt453 RAMDAC

**Status:** milestone 1 — registers, FIFOs, a drawing subset, scan-out through the LUT into a
fixed VESA monitor. Built 2026-09-18; the monitor model and the 8 bpp / GAI +8 wiring added
2026-09-19; the MODE register and the single 8-port I/O map added 2026-09-21; the I/O map
reordered (MODE between the ACRTC's two ports), interrupts (SW1-8), and 2 MB of fixed SRAM
added 2026-09-23; the monitor modes narrowed to the three primary VESA resolutions (640x400
dropped -- it was never a VESA standard, just a VGA text-mode timing) and the default raised
to 1024x768, 2026-09-24.

## The real hardware

There is no period product called cadzilla: it is a board of **Todd's own design**, assembled
from two real chips of the mid 1980s the way a CAD-station card of that era was.

- **Hitachi HD63484 ACRTC** (Advanced CRT Controller, 1984). A CRT controller with a
  microcoded *drawing processor* and its own frame-buffer interface: up to 2 MB of memory on the
  card that the host never addresses — cadzilla fits 2 MB of **SRAM**, which needs no refresh
  cycles at all (a real design choice this board makes; the chip itself also supports DRAM with
  refresh addressing, which cadzilla has no use for). The host sets ~30 timing and display
  registers, then draws by writing 16-bit *commands* into an 8-word FIFO — move, line, rectangle,
  polyline, polygon, circle, ellipse, arc, paint, pattern, copy — in logical X-Y pixel coordinates
  at 1, 2, 4, 8 or 16 bits per pixel. Four logical screens (upper, base, lower, window), zoom,
  cursors, a light pen, DMA to the host. Interfaces as an 8- or 16-bit peripheral occupying **two
  locations**, selected by its RS pin.
- **Brooktree Bt453** (1986). A 256 × 24 color-palette RAM with three 8-bit video DACs. Eight
  pixel inputs (P0–P7) select an entry every pixel clock; three overlay registers on two more
  inputs; an 8-bit MPU port with two command inputs, so it occupies **four locations**.

The board's own decisions, which no chip made — and every one of them is visible from the bus:

| Decision | cadzilla |
|---|---|
| ACRTC bus mode | 8-bit (DACK\* strapped at reset via SW1-6 held Off), the only mode an S-100 8080/Z80 can use |
| **I/O map** | ONE 8-port block from `port` (default `0x70`, a multiple of 8): `+0` the ACRTC's RS=0, `+1` the board's own **MODE register** (write-only), `+2` the ACRTC's RS=1, `+3` undecoded, `+4`..`+7` the Bt453. The ACRTC's own two ports are **not adjacent** — MODE sits between them. There is no separate DAC strap — the RAMDAC always sits four ports above `port` |
| **MODE register** (`port+1`, write-only) | the board's own glue-logic strap, not a register on either chip: `HSPOL`/`VSPOL` sync polarity, `AMODE` the access mode the board's *own* fetch logic runs (single/interleaved — must agree with the ACRTC's own OMR ACM bit, or `wiring` says so), `OLEN` overlay enable (TBD, wired to nothing). Bit layout below |
| **Shift register** | wired for **8 bits per pixel and 8 words per display fetch**: each display cycle the board fetches eight consecutive words (128 bits) at the address the ACRTC puts on MAD and shifts them out as sixteen 8-bit pixels, low byte of the low word first. This is a hardware fact, not a register — see *Programming model* below |
| **Monitor** | a fixed-frequency VESA display chosen by the `mode` strap: the three primary VESA resolutions -- **640x480, 800x600, 1024x768 (default)** -- settable in the machine file or with `SET`. The frame is always the mode's size; the ACRTC's picture is placed in it by HDS/VDS against the mode's porches |
| **Access modes** | single and interleaved, selected by **MODE AMODE** (not read from the ACRTC's own OMR ACM bit — the two are independent straps a driver must set in agreement). Superimposed is not wired |
| **Frame memory** | **fixed at 2 MB of SRAM** — the reference design's own fit, the full 1 M words the ACRTC addresses. Not a strap: the real card has no jumper for it, and needs none, since it never refreshes |
| Pixel bus to the DAC | P0–P7 from the shift register; the Bt453 sees exactly the byte the frame memory holds |
| Overlay inputs | OL0, OL1 tied low regardless of OLEN: the overlay registers can be loaded but nothing selects them yet |
| **IRQ\*** | strapped by `interrupt` (SW1-8): `none` (default, SW1-8 Off) disconnects it; `int` or `vi0`..`vi7` (SW1-8 On) raises that S-100 line whenever `acrtc_.irq()` — an enabled SR flag — is true |

### The MODE register (`port+1`, write-only)

| Bit | Field | Meaning |
|---|---|---|
| 0 | `HSPOL` | horizontal sync polarity: 0 = positive sync pulse, 1 = negative |
| 1 | `VSPOL` | vertical sync polarity: 0 = positive sync pulse, 1 = negative |
| 2 | `AMODE` | access mode the board's own fetch logic runs: 0 = single, 1 = interleaved |
| 3 | `OLEN` | overlay enable — TBD, keep 0 |
| 4–7 | — | unused |

The bit positions are this implementation's own choice — 0..3 in the order the fields were
specified — recorded in `src/boards/cadzilla.h`, not settled by any external spec. The
register is write-only on the wire, like the Dazzler's format port; `SHOW <id>` decodes it
back into four read-only status lines (`hspol`, `vspol`, `amode`, `olen`) the same way the
Dazzler turns its write-only control/format bytes into `video`/`resolution`/`color`.

`HSPOL`/`VSPOL` are recorded and reported but drive nothing else — this model has no separate
sync-pulse signal for a polarity to invert (see Limitations). `OLEN` is likewise recorded but
not wired: OL1..0 stay tied low regardless, so setting it changes nothing about the picture
yet.

### The programmed picture uses the board's own AMODE, not the chip's

`programmedWidth()`, `programmedX()` and the paint loop all read **MODE AMODE**, not the
ACRTC's OMR ACM bit — the board's external fetch logic cannot see inside the chip's register
file, so it needs its own strap, exactly as `wiring()` already treats GBM and GAI as things a
driver must set in agreement with fixed board behavior. If OMR ACM and MODE AMODE disagree,
`wiring` names it (`"OMR ACM is interleaved, MODE AMODE says single"`) and the picture follows
MODE, not OMR — that is what "the board's own fetch logic" means concretely.

### The monitor modes

VESA timings in the board's units — memory cycles of 16 pixels (single access mode) and
rasters. Where a VESA porch is not a whole number of cycles the board's timing rounds it and
keeps the line total, as a timing PROM on a real card would.

| `mode` | Pixel clock | H: sync / back porch / active / front porch (cycles) | V: sync / back porch / active / front porch (rasters) |
|---|---|---|---|
| `640x480` | 25.175 MHz | 6 / 3 / 40 / 1 = 50 | 2 / 33 / 480 / 10 = 525 |
| `800x600` | 40.000 MHz | 8 / 6 / 50 / 2 = 66 | 4 / 23 / 600 / 1 = 628 |
| `1024x768` | 65.000 MHz | 8 / 10 / 64 / 2 = 84 | 6 / 29 / 768 / 3 = 806 |

The register values that put a full picture in each frame (single access mode; the manual's
"set to N−1" already applied). For interleaved mode double `HC`, `HSW`, `HDS` and `HDW`
(`HC = 2·total − 1`, `HSW = 2·sync`, `HDS = 2·back porch − 1`, `HDW = 2·active − 1`) and set
ACM = `10`:

| `mode` | `HSR` r82 (HC, HSW) | `HDR` r84 (HDS, HDW) | `VSR` r86 (VC) | `VDR` r88 (VDS, VSW) | `SSW` r8A (SP1) | `MWR1` rCA |
|---|---|---|---|---|---|---|
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
| Hitachi *HD63484 ACRTC User's Manual*, #U75, Nov 1984 | `reference/Hitachi HD63484 ACRTC User's Manual.md` | **Authoritative for semantics**: 8-bit mode byte order (§5.2, §6.2), the FIFO and status rules (§5.3), GBM/OMR/DCR field meanings (§5.5–5.7), timing and display-control RAM (§5.8–5.9), the operation / color / area modes (§6.6) and every command's worked example (ORG-3, CLR-3/4, WPR-2, RPR-2, WPTN-2, RPTN-2, ALINE-2, ARCT-2, AFRCT-1, DRD/DWT-2). Transcription stops at source page 308 — Sections 1–5 and all 38 commands are covered; the Electrical Specification section (timing, not needed here) is not |
| Hitachi *HD63484 ACRTC Application Note* #U90, April 1986 | bitsavers | The reset state (Table 7-1) and the initialization sequence (§7.2) |
| Brooktree *Bt453*, L453001 Rev. J | `reference/Brooktree Bt453 RAMDAC.md` | **Authoritative** for the four locations (Table 1), the address/phase logic (Table 2) and the pixel/overlay truth table (Table 3) |
| MAME `src/devices/video/hd63484.cpp` | `LICENSE-MAME-HD63484` | **Structural basis only** of the ACRTC model, per the terms in `docs/sources.md`. Where it and the Hitachi documents disagreed, the documents won — see Quirks |

## Register reference

One 8-port I/O block, no memory: everything at `port` (a multiple of 8, default `0x70`).
The 2 MB frame memory is not in this table — it is fixed, private to the ACRTC, and the CPU
never addresses it directly; see *The real hardware* above.

| Addr | OUT (write) | IN (read) |
|---|---|---|
| `BASE+0` | **Address register (AR)**: which control register `BASE+2` reaches (`r00`–`rFF`) | **Status register (SR)**: `CER ARD CED LPD RFF RFR WFR WFE` (D7..D0) |
| `BASE+1` | **MODE register**: `HSPOL`(0) `VSPOL`(1) `AMODE`(2) `OLEN`(3), bits 4–7 unused. Write-only | floats — MODE cannot be read back |
| `BASE+2` | the control register AR names, **one byte**: even AR = high byte, odd AR = low byte. AR = 0/1: a **command word into the write FIFO**, high byte then low | the same register's byte; AR = 0/1: the next byte **out of the read FIFO**, high then low |
| `BASE+3` | — not decoded — | — not decoded — |
| `BASE+4` | Bt453 address register (which LUT entry the next R,G,B cycles touch); resets the color phase to red | the address register |
| `BASE+5` | color palette RAM: red, then green, then blue; the entry is written on the blue cycle and the address increments | R, G, B of the entry, one per read; increments after blue |
| `BASE+6` | the address register again (an alias; there is no separate read-mode register on this part) | the address register |
| `BASE+7` | overlay registers 1–3 (ADDR1..0 = 01, 10, 11), same R,G,B protocol | the same |

The ACRTC's own RS=0/RS=1 pair is deliberately **not adjacent**: MODE sits between them at
`BASE+1`, reflecting a real address-bit decode (A1 is the ACRTC's RS pin, A0 within the low
half of the block picks MODE vs. the ACRTC), not an accident of layout.

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

- **Bus cycles decoded**: `IN`/`OUT` on `port` and `port+2` (the ACRTC's RS=0/RS=1); `OUT`
  only on `port+1` (MODE); `IN`/`OUT` on `port+4`..`port+7` (the Bt453). `port+3` answers
  nothing. No memory.
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
- **`properties()`**: straps `port` (the whole 8-port block's base), `mode` (the monitor),
  `width` (the window), `interrupt` (`none|int|vi0`..`vi7` — SW1-8); live, read-only `video`,
  `picture` (programmed size and position in the frame), `wiring` (GBM/GAI against the board,
  OMR ACM against MODE AMODE), the MODE register decoded into `hspol`/`vspol`/`amode`/`olen`,
  `status` (the SR), and `irq` (whether IRQ\* is asserted right now). Frame memory is not a
  property: it is fixed at 2 MB, with no strap to refit it.
- **Interrupts**: `assertsInt()`/`assertsVi()` read `Hd63484::irq()` — already a pure,
  combinational function of SR against CCR's own enable bits — and gate it on `interrupt`
  (SW1-8): `none`, the default (SW1-8 off), asserts nothing, exactly as an unstrapped IRQ\*
  on the real card reaches nothing. `intChanged()` is called after every ACRTC register and
  FIFO access, and on reset, power-on and snapshot restore, so the S-100 interrupt line
  tracks the chip's own request the instant it can move.
- **Snapshot**: both chips — registers, FIFOs, a command in flight, the drawing state, the
  pattern RAM, **the whole frame memory** (no memory board holds it) and the LUT — plus the
  board's own MODE register, since nothing else holds that either.

### Reset

- `Reset::PowerOn`: the ACRTC's registers, pattern RAM and frame memory zero, then RES\*; the
  Bt453's table black, address 0, phase red.
- `Reset::Bus`: RES\* to the ACRTC — SR `$23`, ABT set, STR and M/S clear, FIFOs cleared,
  everything else kept, so the picture blanks (nothing is displaying) and a driver that restarts
  the chip finds its timing intact. The MODE register is glue on the same RESET\* line and is
  cleared too — a driver must reprogram both the ACRTC's timing and MODE after a reset.
  **The Bt453 has no reset pin**: the LUT survives.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| 8-bit mode: an even AR is the high byte, an odd AR the low byte; `r80`–`rFF` auto-increment, `r00`–`r7F` do not (manual 5.2) | a driver's one-address block load of the timing RAM writes one register 30 times, or its two-write CCR load lands both bytes in one |
| The FIFOs are **8 words**, and WFE/WFR/RFR/RFF track them (manual 5.3) — MAME's model has 16 | a driver that writes eight words on WFE and then polls WFR sees the wrong flag |
| CED clears the moment a command word is written and sets when it ends; CER only ever clears on ABT | a driver polling CED between commands hangs or runs ahead |
| The end point of a line is **not drawn** (ALINE-1); a rectangle's four edges therefore draw each corner exactly once | an EOR-drawn frame leaves its corners lit after being undrawn |
| Dot address *n* is bits `n·bpp` upward from the LSB (ORG-3: DPD=4 at 4 bpp is "bit position 4–7"), and +Y is MW words **lower** in memory | the picture is mirrored, or upside down, or both |
| A line's pattern scan starts at **PPX** (manual 6.8.3) — MAME starts at PSX | a dashed line drawn with a phase offset comes out in the wrong phase |
| The **pattern pointer is live**: PPX/PZCX step once per pixel position a drawing visits (drawn, COL-suppressed or AREA-clipped alike) and are left where they stopped, so the edges of ARCT/APLL/APLG continue one dash pattern and RPR Pr05 reads the advanced pointer; PZCX/PZCY are the zoom counters' *starting* values (manual 5.10.2.6, 6.8.3) | a dashed polyline restarts its dash at every vertex, and a driver that reads Pr05 back to chain patterns gets its own last write |
| A plane fill (AFRCT/RFRCT) starts **every row** from the PPX/PZCX it found and steps PPY once per row; it leaves PPY on the row after its last, so a fill drawn at the new CP continues the tiling. The row restart is inferred from PTN-4, not stated | stacked fills show a seam, or a tile skewed one bit per row |
| AREA 001/101 leave **CP where the drawing crossed** the area boundary, not at Pe ("drawing is executed as long as the CP resides in the defined area", 6.6.3) | a driver that resumes from CP after a hit starts from the wrong place |
| RWPe after CLR/DRD/DWT is RWP's column on the **last raster** (CLR-4: `$56`, AY = −6, MW `$10` → `$B6`) — MAME leaves it one raster further | a driver that clears in strips by chaining CLRs skips or overlaps a raster |
| In 8-bit mode WPTN's *n* counts **bytes**, so it is twice the word count (WPTN-1) | half the pattern loads and the other half is parsed as the next command |
| A DRD "goes into an indefinite wait state after the last transfer" — CED never sets; ABT ends it (manual 6.5) | a driver waiting on CED after a DRD hangs on real silicon and would not here |
| A read that does not fit the 8-word read FIFO (RPTN *n* > 8, RD or RPR onto a full FIFO) **waits for room** (RD-1), and the commands behind it wait in the write FIFO; CED sets when its last word is in | an RPTN of the whole pattern RAM returns half of it, and the command after it runs early |
| **RWP is one register**; DN rides in its high word, so rewriting Pr0C to change screens keeps RWPL (5.10.2.8) | a driver that selects the screen after setting the address transfers to address 0 |
| ELPS's *a* and *b* are **not the semi-axes**: they are the ratio *a* : *b* = dX² : dY² of the squared semi-axes, dX the X one (ELPS-2/3: 9 : 4 with dX = 9 is dY = 6). An arc's radius is CP's own distance from the center; RARC/REARC give both the center and Pe relative to CP | an ellipse comes out the wrong shape, or an arc round the wrong center |
| A curve is drawn **in order** from its start — CCW, or CW when C = 1 — so the pattern runs along it and AREA 001/101 stop where it crosses; CRCL/ELPS start at (A + r, B), go once round, and leave CP at the center; arcs leave CP at Pe, **undrawn** (CRCL-1, AARC-1). Which pixels: the nearest to the curve in each column where it runs mostly horizontally and each row where it runs mostly vertically, so a circle is 8-way symmetric and C changes the order, never the pixels. The manual gives no rule for detecting Pe; an arc ends where Pe's ray crosses it, so a Pe slightly off the curve still ends it | a dashed circle's dashes run backwards, or an EOR-drawn circle leaves its start pixel lit |
| CPY/SCPY scan the source by **S** (rows or columns, from the corner the signs of AX/AY name) and write the destination from RWP in **DSD**'s order — bit 2 columns, bit 1 leftward, bit 0 downward (Tables C14-1/C14-2, read from the scan) — so S ≠ DSD bit 2 transposes the block; RWPe ends **one line past** the last on the slow axis (CPY-6: `$B0` → `$70`), unlike CLR's | a rotated or mirrored copy comes out in the wrong orientation, or a chained copy overlaps its predecessor by a line |
| DRD/DWT/DMOD with **negative AX/AY** walk the block the way CLR does: leftward, and down in Y (up in memory) (DRD-2, DWT-2) | a bottom-up block transfer is rejected, or lands mirrored |
| The shift register is 8 bpp × 8 words whatever CCR/OMR say: a wrong GBM packs pixels the board will not unpack, a wrong GAI makes each fetch overlap the last | an off-spec program shows a coherent picture here and garbage on the card, or the reverse |
| **MODE AMODE, not OMR ACM, governs the board's own fetch pattern** — the two are independent straps a driver must agree, and only `wiring` compares them | a driver that sets the ACRTC to interleaved but forgets `port+1` gets a SINGLE-access picture out of doubled registers: half the frame, or a picture that never fills |
| MODE (`port+1`) is write-only and the ACRTC's own two ports are not adjacent — `port+3` is a gap, not a register | a driver probing the block with `IN` for a live register at `+1` or `+3` finds nothing, correctly; one that assumes RS=0/RS=1 are back to back writes its FIFO data to MODE instead |
| In interleaved mode every horizontal register is in doubled units and a memory cycle is 8 frame pixels | the picture is half as wide as intended, or the timing is off by half a line |
| The picture is placed by HDS/VDS against the mode's porches; the frame does not grow to fit | a program that lands one cycle early is clipped, not shifted |
| A Bt453 address-register **write** resets the R/G/B phase; a **read** does not; both `DAC+0` and `DAC+2` are the same register; the address increments after the blue cycle and wraps `$FF` → `$00` | colors land one channel out of phase, or a Bt458 driver's "read mask" write moves the address |

## Limitations and deliberate departures

- **No time.** Drawing is instantaneous, the raster counter reads 0, there is no DTACK and no
  wait state. A guest that times a command, or that syncs to the raster to avoid flicker, sees
  an infinitely fast chip and an unmoving beam. The status bits a guest *polls* (the FIFO
  flags, CED) are exact.
- **Commands not executed**: PAINT, PTN,
  AGCPY, RGCPY. They are recognized and their parameters consumed — the command stream stays in
  step — and **CER is set**, so a guest can tell. DRD/DWT/DMOD run only in the manual's
  "under program control" mode (no DMAC on the board).
- **Scan-out**: graphic screens only (CHR = 1 character screens are scanned as graphic), the
  three background screens stacked and the window over them, non-interlaced; single and
  interleaved access only (superimposed mode's second phase is not fetched). The monitor is the
  three listed VESA modes, each at the one refresh rate in the table; the ACRTC's own HC/VC and
  the sync widths are accepted but not checked against the mode — a timing a real monitor would
  lose lock on shows here as a picture in the wrong place. No zoom, no cursors, no light pen,
  no blink, no smooth scroll (SDA is ignored), no DISP/CUD skew.
- **The frame memory is zero at power-on**; real SRAM, though it needs no refresh, is not
  necessarily zero at power-on either — this is a simulator convenience, not a hardware fact.
- **Bt453**: the analog side (sync, blank, the CS\*-blanks-video artifact) is not modeled and
  cannot be observed from the bus.
- **MODE HSPOL/VSPOL are recorded, not modeled.** This board has no separate sync-signal
  representation for a polarity bit to invert — the picture is painted directly into a
  Surface, not generated as a raster with distinct sync pulses — so the two bits are decoded
  into `SHOW`'s `hspol`/`vspol` and drive nothing further. **OLEN is likewise recorded but
  not wired**: it is the spec's own "TBD", and OL1..0 stay tied low regardless of its value.
- **The MODE register's bit assignment (0=HSPOL, 1=VSPOL, 2=AMODE, 3=OLEN) is this
  implementation's own choice**, not drawn from any external document — the board is our own
  design and the spec that named the four fields did not fix their bit positions. See
  `src/boards/cadzilla.h`.

## Verification

`tests/test_bt453.cpp` drives the DAC through Tables 1–2 of its data sheet.
`tests/test_hd63484.cpp` (22 sections) drives the ACRTC through its two locations: reset state,
8-bit register access, FIFO/status transitions, every worked example the manual gives (WPR-2,
RPR-2, WPTN-2, RPTN-2, ORG-3, CLR-3/4, ALINE-2, ARCT-2, AFRCT-1, DRD/DWT-2), the operation,
color and area modes, scan-out geometry, the window overlay, and a snapshot round trip.
`tests/test_cadzilla.cpp` proves the board: the one-8-port-block decode (both directions on
the ACRTC's RS=0/RS=1 pair at `+0`/`+2` and the Bt453 at `+4`..`+7`, write-only on MODE at
`+1`, nothing at the `+3` gap), the MODE register's four fields decoding independently into
live status, the ports reaching the chips, and an **end-to-end 640x480 picture** — LUT loaded
through the four DAC ports, a rectangle, a line and a dot drawn through the two ACRTC ports,
the frame read back with `CHECK_FRAME_OPTS` sampled every 32nd pixel for a reader and
`CHECK_FRAME_PIXELS` against an oracle for **every one of the 307,200 pixels**
(`tests/framecheck.h`), its colors resolved through the palette; the picture's placement by
HDS/VDS (one cycle right, one cycle clipped, one raster down, a narrower picture); the wiring
— GAI +1 repeating each fetch, a 4 bpp dot landing in the wrong nibble, and MODE AMODE
disagreeing with OMR ACM (the picture follows MODE, not OMR), as the hardware would, with
`wiring` naming each; **1024x768 in interleaved mode** with the doubled registers, sampled
every 64th pixel and checked pixel by pixel; every mode's frame filled by one `AFRCT` and
every pixel of it checked; the window at HWS/VWS; the `mode` strap re-opening the frame at
800x600; a palette-only change moving the frame, a snapshot repainting it, and RESET\*
leaving a black frame while the LUT survives; and a dedicated **interrupts** section proving
`interrupt=none` (the default, SW1-8 off) asserts nothing regardless of the ACRTC's own
pending state, `interrupt=int`/`vi0`..`vi7` (SW1-8 on) raises that line exactly when
`Hd63484::irq()` is true, and disabling CCR's enable bits drops the request even while SR's
own CED bit is still pending.

No period software exists for this board; the register table above is what a program would
load, and `machines/cadzilla.toml`'s header walks the default 1024x768 case from the monitor
prompt.

## References

- `reference/Hitachi HD63484 ACRTC.md`, `reference/Brooktree Bt453 RAMDAC.md`
- `docs/sources.md` — the sourcing terms, including MAME's role
- `docs/devguide/video-board.md` — how a video board is written, with this one as the example
