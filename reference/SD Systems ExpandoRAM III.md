# SD Systems ExpandoRAM III

Source: [SD Systems ExpandoRAM III.pdf](#) (*ExpandoRAM III Operations Manual*, SD #7140107,
Revision C, June 8, 1983)

The ExpandoRAM III is SD Systems' (Dallas TX, "A Syntech Company") third S-100 dynamic-RAM
board: 32 × MK4164 (64K×1) in **4 banks of 8**, so **64K per bank and 256K per board**, with a
**port-FF page latch** decoded by an on-board PROM (U8) into bank enables *and* the RAM's own
A14–A15. The latest version, the **ExpandoRAM III/696**, follows IEEE-696. The manual is 42 PDF
pages, a **pure page-image scan with no text layer**; every table and figure here was read from
the page image. This file omits the circuit walk-through, the board photo, the assembly drawing
and the parts-list details that do not bear on emulation. Printed page numbers are cited (PDF
page = printed page + 9 in Section II).

Implemented as `bankmem card=expandoram3`: port FF, the Fig 2-2 map for one or two boards,
the S3 configuration switch and the S3 bank enables. The map is built with the fixes listed at
the end of §4.

⚠ **This is the ExpandoRAM *III*.** It shares port FF and "page number = user number" with the
[ExpandoRAM II](SD%20Systems%20ExpandoRAM%20II.md), but its PROM, switch layout and board count
differ. The original [ExpandoRAM (I)](SD%20Systems%20ExpandoRAM.md) has no port at all.

---

## 1. Quick reference for emulation

| Item | Value | Page |
|---|---|---|
| Select port | **FF**, output only (`OUT`; full 8-bit decode A0–A7 = FF, qualified by sOUT) | 2-3 |
| Port byte | **D0–D3 = page number P** (0–F); D4–D5 latched but unused; D6–D7 not latched | schematic |
| Valid pages | 0–9 (CP/M, MP/M, COSMOS), 0–A (OASIS map); **B–F reserved** | 2-7, 2-10 |
| Page = user | "A page number (0-9) is equivalent to a user number (0-9)" | 2-7 |
| Shipped PROM | 48K partition per page (parts list: "COSMOS-48K PROM", SD 7010479, U8) | 2-7, E-3 |
| Board capacity | 256K: 4 banks × 64K (32 × 4164) | 1-1, 1-3 |
| Typical system | 10 × 48K pages on **2 boards** (Fig 2-2) | 2-8 |
| Board number | 0–3, set on S3 (ON = 0, OFF = 1) | 2-10 |
| Power-on page | Latch U13 (74LS174) is cleared by the board reset (POC by etch) → page 0 (inferred from schematic) | 2-11, schematic |
| Wait state | One wait on **M1 (opcode fetch)** only, with E14–E15 in (factory) | 2-6 |
| Refresh | Z-80 RFSH on pin 66 (NDEF), or internal: after 10 idle φ clocks | 2-4, 2-5 |
| PHANTOM | pin 67 disables output when E9–E10 is in (factory) | 2-4 |

---

## 2. Port FF and the page latch

- "The port address decodes port FF and latches the page select data." (2-3) U19 (74LS30) NANDs
  the lower address lines; with sOUT and no refresh in progress, U7 pin 13 clocks the data bus
  into **U13**, a 74LS174 hex latch (2-3, 2-4).
- The data input buffer (U25) is always enabled, so the latch sees the system data-out bus
  directly (2-4).
- Schematic wiring of U13 (read from the page image): BDO0 → PROM A2, BDO1 → PROM A3,
  BDO2 → PROM A4, **BDO3 → PROM A5 through jumper E19–E20**. BDO4 and BDO5 are latched to
  unconnected outputs. So **only the low nibble of the `OUT FF` byte matters**.
- E19/E20/E21 (2-11, 2-12): cut E19–E20 and jumper E21–E20 to feed **A13\*** (inverted bus
  A13) to the PROM's page MSB instead of latch bit 3 — "may require firmware alteration".
- No `IN FF`: the port is write-only. Nothing in the manual reads the page back.

---

## 3. PROM decode (U8)

The PROM has **9 address lines in three fields** and 6 used outputs (2-8, Fig 2-2 page 2):

| PROM address | Field | Source |
|---|---|---|
| A8, A7, A6 | **S** — "BD NBR" | S3 switches (board number, configuration) |
| A5, A4, A3, A2 | **P** — page / "USER NBR" | latch U13 (port FF bits 3–0) |
| A1, A0 | **A** — bus A15, A14 | bus A14/A15, buffered and **inverted** by U18 |

| PROM output | Use |
|---|---|
| 4 outputs (O1–O4) | Active-low bank enable, one per bank, through the S3 bank-enable switches to RAS |
| 2 outputs | Become **RAM A14–A15** in the upper address buffer — which 16K quarter of the 64K bank |

Bank-enable nibble (Fig 2-2 page 2, 2-10):

| Bank selected | PROM output |
|:---:|:---:|
| 3 | 7 (0111) |
| 2 | B (1011) |
| 1 | D (1101) |
| 0 | E (1110) |

If no bank-enable output goes low, the board does not drive the bus (U4 pin 8 gates the output
latch, 2-4) — this is how a board ignores pages that live on another board.

⚠ The text calls the S field "the board number and a PROM enable" (2-8), but Fig 2-3 has a
separate **PROM ENABLE** switch *and* a **CONFIGURATION** switch, and the schematic sends four
grounding switch positions to PROM A6, A7, A8 and the chip enable. The reading that closes is
**A6/A7 = board number D0/D1, A8 = configuration (CP/M vs OASIS), PROM enable = chip enable**:
Fig 2-2 page 1 holds two complete maps, one per configuration, and boards 0–3 need exactly two
bits. Which of A6/A7 is D0 is not printed — confirm on the schematic before relying on it.

---

## 4. Page map (Fig 2-2 page 1, printed 2-9)

The figure draws each board as a **4 × 4 grid of 16K cells**: rows are **banks 0–3**, columns
are the **RAM quarter** (the 2 PROM outputs that drive RAM A14–A15). Each cell says which CPU
quarter **A** (bus A15–A14: 0 = 0000–3FFF … 3 = C000–FFFF) of which page **P** it holds. A
bracketed column labelled `S=s P=p` means banks 0–2 (or 1–3) of that column hold page *p*.

### CP/M, MP/M, COSMOS 48 boundary (configuration switch ON)

Pages are **A=0–2 (0000–BFFF)**; the top quarter A=3 (C000–FFFF) is the common area.

| Board S | RAM quarter 0 | quarter 1 | quarter 2 | quarter 3 |
|---|---|---|---|---|
| S=0, banks 0–2 | P=0, A=0/1/2 | P=1, A=0/1/2 | P=2, A=0/1/2 | P=3, A=0/1/2 |
| S=0, bank 3 | **A=3, P=0** | A=3, P=4 | A=3, P=4 | A=3, P=4 |
| S=1, banks 0–2 | P=5, A=0/1/2 | P=6, A=0/1/2 | P=7, A=0/1/2 | **P=8, A=3/3/3** |
| S=1, bank 3 | **A=3, P=0** | A=2, P=9 | A=1, P=9 | A=0, P=9 |

### OASIS 48 boundary (configuration switch OFF)

Pages are **A=1–3 (4000–FFFF)**; the bottom quarter A=0 (0000–3FFF) is the common area.

| Board S | RAM quarter 0 | quarter 1 | quarter 2 | quarter 3 |
|---|---|---|---|---|
| S=0, banks 1–3 | P=0, A=1/2/3 | P=1, A=1/2/3 | P=2, A=1/2/3 | P=3, A=1/2/3 |
| S=0, bank 0 | **A=0, P=0** | A=0, P=4 | A=0, P=4 | A=0, P=4 |
| S=1, banks 1–3 | P=5, A=1/2/3 | P=6, A=1/2/3 | P=7, A=1/2/3 | **P=8, A=0/0/0** |
| S=1, bank 0 | A=3, P=9 | A=2, P=9 | A=1, P=9 | **A=2, A=1, P=A** (as printed) |

Accounting that closes: each board has 16 cells; two boards hold **10 pages × 3 cells + the
common cell** = 31, plus one more cell per map (S=1's bank-3/bank-0 quarter 0). The page
structure (pages 0–3 one per column on board 0; page 4 in the leftover bank; 5–7 per column on
board 1; 8 in its last column; 9 in its leftover bank) is unambiguous.

⚠ **Several cell labels look like typing errors and are not settled:**
- Page 4's three cells all read **A=3** (CP/M) / **A=0** (OASIS) — the *common* quarter — where
  the page needs A=0/1/2 (CP/M) or A=1/2/3 (OASIS). Board 1's matching page 9 row counts
  A=2/1/0 and A=3/2/1, which is the pattern page 4 should follow.
- Page 8's column reads **A=3** ×3 (CP/M) and **A=0** ×3 (OASIS) — the same problem.
- Board 1 bank 3 quarter 0 reads **S=1 A=3 P=0** — a second copy of page 0's common quarter.
  Two boards cannot both answer C000–FFFF, so either it is unused, or the common area is
  per-board. The manual does not say.
- OASIS board 1 bank 0 quarter 3 reads **A=2, A=1, P=A**: two A values and a page A (10), while
  §2.4.1 says pages are 0–9. The note "P=B,C,D,E,F IS RESERVED" (2-10) leaves P=A legal.

**Evidence from the ExpandoRAM II.** The II's Fig 2-4 is the same kind of drawing for an earlier
board of the same family ([ExpandoRAM II](SD%20Systems%20ExpandoRAM%20II.md) §4). It is evidence,
not proof, for the III, because the two boards use different PROMs. Three things in it bear on
the cells above:
- The II's page 4 and page 8 are each three cells with **three different quarters** (A=2, 1, 0).
  This supports reading the III's "A=3 ×3" and "A=0 ×3" cells as three different quarters.
- The II has **one** common cell (board 0, bank 3). This supports one common cell on the III.
- The II's last cell (board 7, bank 3) is **page A, a single 16K cell**. The III's leftover
  board-1 cell may also be page A. The III prints it as "A=3 P=0", so it stays unresolved.

The only firm cure is a dump of the COSMOS-48K PROM (SD 7010479). Until one exists,
`bankmem card=expandoram3` builds the map with these fixes, and its table comments name each one:
- **Page 4** gets the three quarters of the switched window: A=0/1/2 (CP/M) or A=1/2/3 (OASIS).
  Each cell stays in the bank and RAM quarter the figure prints.
- **Page 8** gets, in each bank, the quarter that pages 5–7 have in that bank.
- **Board 1's leftover cell** (CP/M "S=1 A=3 P=0", OASIS "A=2, A=1, P=A") is not built. Board 0
  carries the one common cell.

Software cannot see which RAM quarter holds which CPU quarter, so the first two fixes change
what a guest sees only when a bank is switched off.

---

## 5. S3 switch settings (Fig 2-3, printed 2-10)

8-position DIP. **Factory setting: all switches ON** (2-8).

| Switch | Function |
|:---:|---|
| 1 | Bank enable 3 (ON = bank enabled) |
| 2 | Bank enable 2 |
| 3 | Bank enable 1 |
| 4 | Bank enable 0 |
| 5 | Configuration: **ON = CP/M, MP/M, COSMOS; OFF = OASIS** |
| 6 | Board select D1 |
| 7 | Board select D0 |
| 8 | **PROM ENABLE (must be on)** |

Board select: "Board may be designated 0-3 by setting switches to octal address (ON is 0, OFF
is 1)." All ON = board 0.

⚠ The schematic numbers the four bank-enable positions as S3 rows 5–8 and the four grounding
positions as rows 1–4 — the reverse of Fig 2-3. Use Fig 2-3 for the *function* of each switch;
the physical position number depends on which drawing you trust.

---

## 6. Jumpers

Factory straps E9–E10 and E14–E15 are "required for most SDSystems configurations and
software" (2-8). The others are etched for SD Systems CPU boards and are cut to change (2-11):

| Jumper | Factory | Change |
|---|---|---|
| E9–E10 | in | PHANTOM (pin 67) disables the output buffer; remove to ignore PHANTOM (2-4) |
| E14–E15 | in | One wait state on every sM1 cycle (for 4 MHz+ CPUs) (2-6) |
| E22/E23/E24 | φ on **pin 24** (IEEE-696) | cut E24–E23, jumper E23–E22 → clock from pin 25 (not IEEE-696: pin 25 is pSTVAL) |
| E25/E26/E27 | reset = **POC** | cut E26–E25, jumper E26–E27 → reset from RESET |
| E16/E17/E18 | PROM page-decode A14 input from bus A14 | cut E18–E17, jumper E17–E16 → bus **A7** instead ("may require firmware modification") |
| E19/E20/E21 | page MSB from latch U13 pin 10 | cut E19–E20, jumper E21–E20 → **A13\*** |
| E6/E7/E8 | RAM A7 from address mux | cut E6–E7, jumper E6–E8 → RAM A7 tied high: 64K → 16K parts (4116) |

---

## 7. Bus interface (Table 1-2, printed 1-4)

Inputs: A0–A15, DO0–DO7, sM1 (44), sMEMR (47), MWRT (68), pDBIN (78), sOUT (45), POC\* (99),
PHANTOM\* (67), NDEF/RFSH (66), φ1 (24 on the /696, 25 on the plain III), RFU/pWAIT (27).
Outputs: DI0–DI7, RDY (72). Power +8 V on 1 and 51, ground on 50 and 100.

- **pWAIT (27):** with older SD CPU boards (SBC-200) the CPU echoes RDY low back on pin 27 as
  pWAIT; the board uses it to stop the refresh counter from being halted by a POC during a wait
  (2-7).
- The table title says "for 32K/64K" — a leftover from an earlier manual.

---

## 8. Timing and refresh

| Item | Value | Page |
|---|---|---|
| Access | 200 ns max | 1-3 |
| Cycle | 375 ns min | 1-3 |
| Clock | 4 MHz | 1-1 |
| Delay line U2 taps | 50 ns (pin 12), 100 ns (pin 4), 250 ns (pin 2) | 2-5 |
| Internal refresh | U16 (74LS162) counts φ; after **10** clocks with no read/write/refresh it forces a RAS-only refresh | 2-5 |
| Refresh address | U9 (74LS393) counter, gated on by U14 | 2-5 |

Refresh, wait and RDY are bus-timing detail with no effect on an instruction-level emulator.

---

## 9. Shipped software

Appendix A (A-3, A-4) is a generic Z-80 address-storage memory test ("translated from Dec 1976
Interface Magazine"), ORG 0, test top set by `EPAGE` (10H = 4K … FFH = 64K). **It never touches
port FF**, so it settles nothing about paging.

---

## Quirks worth carrying

- ⚠ **Low nibble only.** Port FF keeps D0–D3 (D4–D5 latched, unused). An emulator that compares
  the whole byte against a page number is wrong for `OUT FF` with high bits set.
- ⚠ **The PROM remaps RAM A14–A15.** A bank is 64K, not 16K; the PROM picks *which 16K* of the
  bank answers a CPU quarter. A "bank = 16K window" model will not reproduce Fig 2-2.
- ⚠ **Fig 2-2 has cell labels that cannot be right** (page 4 and page 8 cells, board 1's second
  page-0 cell, P=A). Recorded as printed in §4; unresolved without a PROM dump. `bankmem`
  builds the map with the fixes listed in §4.
- ⚠ **§2.2.1 is stale:** it says "up to 32 (16K) dynamic random access memory elements … total
  storage capacity of the EXPANDORAM III is 65,536" (2-1). Section I, Table 1-1 (262,144 bytes)
  and the parts list (32 × 4164, U26–U57) win; the 16K figure is the E6–E8 4116 option (the
  assembly drawing marks "USED FOR 4116").
- ⚠ **S3 numbering disagrees** between Fig 2-3 and the schematic (§5).
- ⚠ **Power-on page 0 is inferred,** from U13's clear being tied to the board reset; the prose
  never states it.
- One B-section page (the "Various Combinations of Refresh Timing Waveforms" sheet, with ALE,
  15 µs OSC and SEL'D RAS) appears to come from a DRAM-controller data sheet; the parts list has
  no such controller. Ignore it for this board.
- Scan defects: printed page 1-1 is scanned **twice** (PDF pp 5–6); the two schematic sheets
  (an ExpandoRAM III/696 sheet, Rev C, and an earlier ExpandoRAM III sheet) are bound **before**
  the Appendix D divider; most figures are bound sideways.
