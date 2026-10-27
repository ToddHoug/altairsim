# SD Systems ExpandoRAM II

Source: [SD Sales ExpandoRAM-II.pdf](#) (*ExpandoRAM II Operations Manual*, SD #7140042,
© SD Systems 1979, this printing "Revision C, March 1981") and
[SD Systems ExpandoRAM II Errata.pdf](#) (SD Systems Technical Bulletins, 1980)

The ExpandoRAM II is an S-100 dynamic-RAM board from SD Systems (Dallas TX). It holds 32 RAM
chips in **4 banks of 8**: MK4116 (16K×1) for 16K–64K, or MK4164 (64K×1) for 64K–256K. A write
to **port FF** latches a page number. An 82S130 PROM (U8) decodes the page, the board number on
S3 and bus A15–A14 into one active-low bank enable. Both PDFs are page-image scans with no text
layer, and every number here was read from the page image. Page numbers are the printed ones:
"p. N" for the manual's `PAGE N`, and "2-9" for Fig 2-4. This file omits the assembly and
check-out steps, the memory test, the timing diagrams and the parts detail.

The deramp.com copy of the manual (32 pp) is the one to use. Its Fig 2-4 is clean. An earlier
copy (s100computers.com, 31 pp) was too blurred to read the map.

Implemented as `bankmem card=expandoram2`.

⚠ **This is the ExpandoRAM *II*.** The original [ExpandoRAM (I)](SD%20Systems%20ExpandoRAM.md)
has no I/O port at all. The later [ExpandoRAM III](SD%20Systems%20ExpandoRAM%20III.md) keeps
port FF and the S/P/A PROM fields, but its PROM also drives RAM A14–A15, and its map differs.

---

## 1. Quick reference for emulation

| Item | Value | Page |
|---|---|---|
| Select port | **FF**, output only | p. 5, p. 12 |
| Port byte | **D0–D3 = page number**; D4–D5 latched but not connected; D6–D7 not latched | schematic |
| Pages | 0–9 by the text; Fig 2-4 also maps a page **A**. B–F reserved | p. 12, 2-9 |
| PROM | 82S130 (512 × 4), U8. 4 outputs = 4 bank enables. Nothing drives RAM A14–A15 | p. 4, schematic |
| PROM variants | 32K partition (6 boards) or 48K partition (8 boards), both with 16K chips | p. 11, p. 12 |
| Board number | 0–7 on S3 (D0–D2), ON = 0, OFF = 1 | p. 12 |
| Board size | 64K (4116) or 256K (4164). With 4164s, "4-64K banks are available" | p. 1, p. 2 |
| Power-on page | Not described (§6) | — |

---

## 2. Port FF and the page latch

- "Port FF is used to select the Memory page (32K or 48K) to be accessed by the CPU … The pages
  are accessed by outputting the page number to port FF." (p. 12)
- "The port FF board select decodes port FF and latches the output data on the board." (p. 5)
- Schematic (drawing 0100161, Rev E), read from the page image: U13 is a 74LS174. BDO0 → PROM
  A2, BDO1 → A3, BDO2 → A4, **BDO3 → A5 through jumper E19–E20**. BDO4 and BDO5 go to unconnected
  outputs. So **only the low nibble of the `OUT FF` byte is the page.** The III is wired the
  same way.
- The text refers to "Figure 4-2 for page mapping" (p. 12). The figure is printed as
  **"Figure 2-4. PROM Program and Page Mapping"** (2-9) and is bound after Appendix E. They are
  the same figure.

---

## 3. PROM fields (Fig 2-4 legend, 2-9)

| PROM address | Field | Source |
|---|---|---|
| A8, A7, A6 | **S** — board number ("BD NBR") | S3 D2, D1, D0 |
| A5, A4, A3, A2 | **P** — page ("USER NBR") | latch U13, port FF bits 3–0 |
| A1, A0 | **A** — bus A15, A14 | the CPU address |

| Bank selected | PROM output |
|:---:|:---:|
| 3 | 7 (0111) |
| 2 | B (1011) |
| 1 | D (1101) |
| 0 | E (1110) |

One output goes low for the bank that answers. If no output goes low, the board does not answer.
"P=B,C,D,E,F IS RESERVED." (2-9)

---

## 4. The page map (Fig 2-4, 2-9)

Fig 2-4 draws each board as one column of 4 banks. **With 16K chips a bank is 16K**, so each cell
is one 16K quarter **A** of the CPU address space (A=0 is 0000–3FFF, A=3 is C000–FFFF). S is the
board number.

### 48K boundary (8 boards)

| Board S | Banks 0–2 | Bank 3 |
|:---:|---|---|
| 0 | page 0, A=0/1/2 | **A=3, no page: the common 16K at C000** |
| 1 | page 1, A=0/1/2 | page 4, A=2 |
| 2 | page 2, A=0/1/2 | page 4, A=1 |
| 3 | page 3, A=0/1/2 | page 4, A=0 |
| 4 | page 5, A=0/1/2 | page 8, A=2 |
| 5 | page 6, A=0/1/2 | page 8, A=1 |
| 6 | page 7, A=0/1/2 | page 8, A=0 |
| 7 | page 9, A=0/1/2 | **page A, A=2** |

Pages 0–9 are 48K each (0000–BFFF). The common 16K (C000–FFFF) is the same RAM for every page.
Page A is a single 16K cell.

⚠ Page A's address. The text says "user ØAH has 16K positioned at @ CØØØH" (p. 12). Fig 2-4 puts
page A's one cell at **A=2 (8000–BFFF)**. Both are recorded. The figure is the PROM program, so
an emulator follows the figure and notes the text.

### 32K boundary (6 boards)

| Board S | Banks 0–1 | Banks 2–3 |
|:---:|---|---|
| 0 | page 0, A=0/1 | **A=2/3, no page: the common 32K at 8000** |
| 1 | page 1, A=0/1 | page 2, A=0/1 |
| 2 | page 3, A=0/1 | page 4, A=0/1 |
| 3 | page 5, A=0/1 | page 6, A=0/1 |
| 4 | page 7, A=0/1 | page 8, A=0/1 |
| 5 | page 9, A=0/1 | page A, A=0/1 |

Pages 0–A are 32K each (0000–7FFF). The common 32K (8000–FFFF) is the same RAM for every page.

### Fewer boards

A system with N boards (S=0 … N-1) carries only the pages whose cells are on those boards. For
example, four 48K-PROM boards (256K of 4116s) carry pages 0–4 and the common 16K. Four 32K-PROM
boards carry pages 0–6 and the common 32K.

### Boards of 4164s

⚠ The manual says only "With 4164's, 4-64K banks are available" (p. 1), plus the jumper changes
for 64K chips (p. 11). It does not say how the 32K or 48K PROM maps pages on a board of 4164s.
Fig 2-4 is drawn for 16K banks. Do not apply the 16K-bank map to a 4164 board.

### Part numbers

The parts list (BOM 0100160 Rev G) names U8 as **"82S130 U8 (EX2-48K)", SD 7010407**. The COSMOS
manual names the PROMs EX-48 (7010393) and EX-32 (7010392). ⚠ Three part numbers, and no page
says if they are the same programs.

---

## 5. Switch S3 (Fig 4-1, p. 12)

| Switch | Function |
|:---:|---|
| 1 | Bank enable 3 (ON = enabled) |
| 2 | Bank enable 2 |
| 3 | Bank enable 1 |
| 4 | Bank enable 0 |
| 5 | Board select D2 |
| 6 | Board select D1 |
| 7 | Board select D0 |
| 8 | PROM ENABLE (must be on) |

"Board may be designated 0-7 by setting switches to octal address (ON is Ø, OFF is 1)." The
switch numbers are printed on Fig 4-1 in this copy.

---

## 6. Reset and bus notes

⚠ **The manual does not describe reset.** J1 lists POC at pin 99 and PHANTOM at pin 67
(Table 1-2, p. 3). The text never says what the board does on POC. The schematic shows the clear
input of U13 wired, but that wire was not traced here. Do not state a power-on page as fact.

Other notes: the board is "optimized for operation with SD Systems' SBC-100/200" and uses the
Z-80 RFSH signal (p. 1). φ1 is on pin 25 (Table 1-2). It needs three supplies: +7 to +10 V, +14 to +18 V
and −14 to −18 V (p. 2). E9–E10 is the PHANTOM jumper, and E14–E15 adds an
M1 wait state at 4 MHz (p. 12, p. 13).

| Item | Value | Page |
|---|---|---|
| RAM chips | up to 32, in 4 banks of 8: U26–33 bank 0, U34–41 bank 1, U42–49 bank 2, U50–57 bank 3 | p. 4, p. 9 |
| 16K or 64K chips | standard is 4116. For 4164: cut E1–E2, E4–E5, E6–E8; connect E2–E3, E6–E7 | p. 11 |
| Access / cycle | 200 ns max / 375 ns min | p. 2 |

Marginalia on p. 11: faint handwriting beside the 4164 steps reads like "cut E16–E17" and
"connect E17–E18". It is not legible enough to record as a step.

---

## 7. Errata (SD Systems Technical Bulletins, 1980)

None of these change port FF or the page map. All are refresh, wait-state, clock-pin and
reset-pin rework.

| Bulletin | Date | Content |
|---|---|---|
| **105** | April 1980 | The board "could hang up in Wait State Refresh cycles". Needed "for any system running with 60K SDOS or COSMOS". U6 74LS74 → 74S74, R7 10K → 20K, and three trace cuts and three jumpers (U11-14 to U11-16, U7-1 to U16-7, U17-9 to U22-6) to start the wait-state refresh on the leading edge of φ1. A board diagram is on the next page. |
| **109** | June 1980 | Composite of the modifications for Revisions A, B and C: one cut and one jumper for each revision, then the 105 changes for all three. |
| **111** | July 1980 | Cromemco and North Star processor boards: install E9–E10 and E14–E15 (at 4 MHz); move U22-11 from pin 25 to **pin 24** (φ1); move U18-11 from pin 99 (POC) to **pin 75** (RESET). Bulletin 109 first. |
| **112** | July 1980 | Refresh fails with wait states longer than 15 µs, or when sMEMR or MWRITE overlaps refresh ("not a problem in the SD 100/SD 200/SD 700 computers"). Add a 74LS122 between U7-10 and U6-4. Bulletin 109 first. |

The errata PDF also holds a replacement page 2-10, "2.4.3 EXPANDORAM II IN NON SD SYSTEMS
ENVIRONMENTS". It gives the same steps as p. 12 as a list: cut E22–E23, jumper E23–E24, cut
E26–E25, jumper E26–E27, E9–E10 for other memory, E14–E15 at 4 MHz.

---

## Quirks worth carrying

- ⚠ **Low nibble only.** D4–D5 are latched to unconnected outputs.
- ⚠ **One bank per cell, 16K per bank.** The 82S130's four outputs are bank enables only. A page
  is built from banks spread across several boards (48K page 4 is bank 3 of boards 1, 2 and 3).
- ⚠ **Page A** is in Fig 2-4 but not in the text's "0-9". In the 48K map it is one 16K cell, and
  the text and the figure place it at different addresses.
- ⚠ **The 4164 board's page map is not documented.**
- ⚠ **Reset is not documented.**
- ⚠ **Three PROM part numbers** (7010407 EX2-48K, 7010393 EX-48, 7010392 EX-32).
