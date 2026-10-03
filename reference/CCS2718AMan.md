# CCS Model 2718 — Serial / Parallel I/O Interface

Source: [CCS2718AMan.pdf](#)

California Computer Systems (CCS), Sunnyvale CA. *Owner's Manual, Model 2718 Parallel/Serial
Interface*, manual no. 89000-02718, © 1980, 45 PDF pages. One S-100 board with **two serial
ports** (Port A a **1602-family UART**, Port B an **Intel 8251 USART**), **two parallel ports**
with four-line handshaking, and an optional **2K ROM socket** with phantom-line overlay. The
board is the **2718**; "rev A" in the manual is only the PC-board revision (`PC Board, 2718, rev A`,
part `02718-00002`, parts list p. A-5). It has no suffix "A" as a model name.

**Not emulated.** This file omits the warranty (Appendix C) and the generic UART/USART chip
descriptions (Appendix B); the 8251 is covered by [Intel 8251 USART](Intel%208251%20USART.md).

**Scan notes.** No text layer — every page was read as an image. PDF page = printed page + 3 for
Chapters 1–3 (printed 1-1 is PDF page 4). The schematic (A-10, A-11, PDF pages 36–37) is rotated
and too coarse to read signal names; this file does not use it. The scan carries **handwritten
marginalia** from a previous owner (see ⚠ list); it is marked where it matters.

## 1. Quick reference for emulation

| Item | Value |
|---|---|
| Bus | S-100, 8080-style (`pSYNC`, `pDBIN`, `pWR*`, `pRDY`, `sINP`, `sOUT`); **no interrupt logic anywhere in the manual** |
| Serial ports | **A** = UART (1602 family, U20, marked `1883/1602/6011`), **B** = 8251 USART (U19) |
| Serial block | **4 consecutive ports**, base = any multiple of 4 (SER ADDR jumpers, A2–A7). A1 of the base must be 0 |
| Parallel block | **2 consecutive ports**, base = any multiple of 2 (PAR ADDR jumpers, A1–A7) |
| Baud | 4702 programmable bit-rate generator, 2.4576 MHz crystal; one register sets both ports |
| ROM | One 2716 (2K) socket, user-supplied, base on a 2K boundary (ROM ADDR jumpers A15–A11) |
| Power | +8 V 0.75 A, ±16 V 0.05 A, regulated on board to +5/+12/−12 |

## 2. Port map

Everything is jumper-dependent. With **SPR = N, SAI = N, SBI = N** (Table 3.1, p. 3-3):

| Port | Read | Write |
|---|---|---|
| `base+0` | Serial A / parallel status | Baud-rate register |
| `base+1` | UART data in | UART data out |
| `base+2` | USART status | USART command (and mode / sync words) |
| `base+3` | USART data in | USART data out |

Jumpers that move registers:

| Jumper | Effect |
|---|---|
| **SPR** (Serial Ports Reverse) | Inverts A1. N: Serial A at A1=0, Serial B at A1=1. I: swapped (A = `base+2/+3`, B = `base+0/+1`). |
| **SAI** | Inverts A0 for Serial A: swaps (baud / status) with (data). N: data at A0=1. I: data at A0=0. |
| **SBI** | Same for Serial B: swaps (command / status) with (data). N: data at A0=1. I: data at A0=0. |

Table 3.1 lists all eight combinations; each of SAI and SBI moves only its own port.

**Parallel ports** (`par_base`, `par_base+1`): A0 picks port A (0) or B (1); an `IN` reads the
input latch, an `OUT` writes the output latch (Table 1.1). Reading a port sets its *Input Empty*
flip-flop; writing it clears its *Data Available* flip-flop. No other handshake software is needed.

**Overlay.** When the parallel base equals the serial base (jumpers 2–7 equal) and PAR ADDR 1 is 1
(0 if A1 is inverted by SPR), the parallel ports overlay one serial port, and the parallel status
is still readable at the UART status address (§1.1, p. 1-2).

## 3. Serial Port A — UART

Data transfer and handshaking are done by the UART. Format is set by **jumpers, not software**
(§2.1.10): WLS1/WLS2 (word length, `00`=5 … `11`=8 bits), SBS (0 = 1 stop bit, 1 = 2; 1½ for 5-bit
words), PI (1 = parity inhibited), PS (1 = even, 0 = odd).

**Ser A / Par status byte (`base+0` read).** Eight bits, **each position chosen by the 16-pin
Status Header** (§2.2, p. 2-8). The header takes eight signals and routes each to any data bit:

| Signal | Meaning |
|---|---|
| `DR` | Data received: a character is in the receiver holding register |
| `THRE` | Transmitter holding register empty, **ANDed with the peripheral's handshake line**: 1 = a character may be loaded |
| `TRE` | Transmitter register empty |
| `ERROR` | Parity, overrun or framing error (see ⚠) |
| `PAIMT`, `PBIMT` | Parallel A / B **Input Empty** |
| `PADAV`, `PBDAV` | Parallel A / B **Data Available** |

The sample wiring (Fig. 2.5) makes `DR` = bit 1 and `THRE` = bit 0 (the USART's RxRDY / TxRDY
positions), parallel A in bits 2–3, parallel B in bits 4–5, and drops `TRE` and `ERROR`. This is
how the board imitates other boards' status bytes for existing software. **The right-hand pin
numbers of Fig. 2.4 were not transcribed**; re-read the page when building the straps.

Port A carries only TxD, RxD, RTS and CTS (Fig. 1.1; A.6). Receive: the UART forces **RTS high
while its receive register is empty** (ready for data); reading the data register clears `DR`.

## 4. Serial Port B — 8251 USART

Initialised by writing, **in order**, to the command address: a **mode word**, then (for sync mode)
one or two **sync words**, then a **command word**. After any reset the USART is idle and must be
re-initialised. Chip detail is in [Intel 8251 USART](Intel%208251%20USART.md); the board facts are:

| | |
|---|---|
| Mode word, async | bits 1–0: `01` = 1×, `10` = 16×, `11` = 64×; bits 3–2: `00`..`11` = 5..8 bits; bit 4 parity enable; bit 5 `1` = even; bits 7–6: `01` = 1, `10` = 1½, `11` = 2 stop bits (`00` invalid) |
| Mode word, sync | bits 1–0 = `00`; bit 6 (SYNDET in/out) **must be 0** (the board does not use pin 16); bit 7: `1` = one sync char, `0` = two |
| Command word | bit 0 TxEN · 1 "DSR" (forces the DSR line) · 2 RxEN (recommended 1) · 3 SBRK (break) · 4 ER (error reset) · 5 CTS (forces CTS) · 6 IR (internal reset) · 7 EH (enter hunt) |
| Status byte | bit 0 TxRDY · 1 RxRDY · 2 TxE · 3 PE · 4 OE · 5 FE · 6 SYNDET (always 0, internal sync) · 7 **DTR** |

**Handshake naming is swapped** from the Am8251 sheet: the board is a data *terminal*, so the 8251's
DTR/DSR pins act as the board's DSR/DTR signals. The board **inverts the 8251's low-active
handshake pins**, so command bits 1 and 5 and status bit 7 show the **actual line level** (Table B.1
note, p. B-5). Status bit 7 is the peripheral's pin-20 level; the sense (ready or busy) belongs to
the peripheral. The shipped printer driver treats bit 7 = 1 as **busy**.

Port B alone supports DSR (pin 6), received line signal detect `CF` (8), receive clock `DD` (17)
and DTR (20) on the DB-25 (A.6). TxD, RxD, RTS, CTS, signal ground and the transmit clocks `DB`/`DA`
are on both ports.

## 5. Baud-rate register and generator

One write to `base+0` sets both ports when the BAUD SEL jumpers are on **S** (soft); the **low
nibble is Serial A**, the **high nibble Serial B** (Fig. 3.5). A port whose jumpers are on **1/0**
(hard) ignores the register and uses its wired code. Position #1 is the low-order bit S0, #4 is S3.

| Code | Rate | Code | Rate |
|---|---|---|---|
| `0` | 19200 | `8` | 9600 |
| `1` | **EXT** (external clock via IM) | `9` | 4800 |
| `2` | 50 | `A` | 1800 |
| `3` | 75 | `B` | 1200 |
| `4` | 134.5 | `C` | 2400 |
| `5` | 200 | `D` | 300 |
| `6` | 600 | `E` | 150 |
| `7` | 2400 | `F` | 110 |

(Table 2.4 and Table 3.1 print the same sixteen codes.) See ⚠ on the duplicate 2400.

## 6. ROM, phantom and wait state

- **ROM**: one 2716 at a 2K boundary from ROM ADDR jumpers 15–11 (Table 2.5, `0000`…`F800`). Enabled
  when the address matches and `MEM/IO*` is high; output only when `pWR*` is high.
- **FF detect**: when all eight data lines are `FF` (an unprogrammed byte), the board's input buffer
  is disabled, so an **empty ROM location does not drive the bus**. A partly filled ROM therefore
  coexists with other memory at the same addresses.
- **PHNTM** (E = enabled, D = disabled): pulls `PHANTOM*` low while the ROM answers, to disable CPU
  ROM beneath it; `FFD*` low (empty location) lets it go. The manual's example is a CCS 2810 CPU with
  ROM base `F000` and PHNTM = E.
- **WAIT** (E = enabled, D = disabled): pulls `pRDY` low in each cycle where the board is active
  (`pSYNC` and `FFD*` high). The I/O devices do not need wait states.

## 7. Parallel jumpers

| Jumper | Sets |
|---|---|
| PADSI / PBDSI | Polarity of the peripheral's DATA STROBE that gates the input latch (N = active high, I = active low, E = latch always open) |
| PASSI / PBSSI | Polarity of DATA STROBE used to **clear the Input Status flip-flop** (Table 2.2) |
| PAIMT / PBIMT | Input Empty output polarity (H = high-active, L = low-active) |
| PADAS / PBDAS | DATA ACCEPTED polarity that clears Output Status (N = low, I = high) |
| PADAV / PBDAV | DATA AVAILABLE output polarity (H / L) |
| PAOE / PBOE | Output enable: X = peripheral pulls pin 5 / 6 low, E = always enabled |

Output connector: D0–D7, DATA ACCEPTED (3), DATA AVAILABLE (4), ENABLE (5, 6). Input connector:
D0–D7, INPUT EMPTY (3), DATA STROBE (4). All output buffers are non-inverting 74LS244s; a 74LS240
swapped in inverts a byte (§2.3).

## 8. Shipped software (p. 3-7, 3-8)

A CP/M list-device driver for a Diablo HyTyPe printer on **Port B**, with the ports at `02` (data)
and `03` (status / command):

```
        XRA A / OUT 03 ×3         ; ensure command mode
        MVI A,40H / OUT 03        ; internal reset
        MVI A,0CEH / OUT 03       ; mode:  2 stop bits, no parity, 8 bits, 16×
        MVI A,37H  / OUT 03       ; command: TxEN, DSR, RxEN, ER, CTS
LIST:   CALL LPSTAT / ORA A / JZ LIST / MOV A,C / OUT 02 / RET
LPSTAT: IN 03 / ANI 01H / RZ      ; TxRDY clear -> busy (zero)
        IN 03 / ANI 80H / XRI 80H / RET    ; bit 7 set -> busy
```

This is the ordinary 8251 reset-then-mode-then-command sequence. Handwritten notes on the listing
are partly illegible; the printed opcodes (`D3 03`, `D3 02`, `DB 03`) are the evidence used here.

## Quirks worth carrying

- ⚠ **Serial B register order: Table 1.3 vs Table 3.1.** Table 1.3 (p. 1-2, "assuming A1/A0 are not
  inverted") puts USART **data at A0=0** and command / status at A0=1. Table 3.1 and §2.1.3 put the
  data at **A0=1** with SBI = N. The shipped driver uses data at `02`, command / status at `03`,
  which is Table 1.3's order, so that board was strapped **SBI = I** by Table 3.1's rule. **Settled
  by:** the jumpers decide, and Table 3.1 is the rule. **Not settled:** the factory SBI default. Table
  1.2 (Serial A) agrees with Table 3.1.
- ⚠ **Address-jumper labels.** §1.1 and §2.1.1 say PAR ADDR 1–7 and SER ADDR 2–7. A handwritten note
  beside §2.1.1 says the labels are *reversed from 2.1 and the board*. **Unresolved.** The port
  counts are not in doubt: serial uses 4 ports (A2–A7), parallel uses 2 (A1–A7).
- ⚠ **Two 2400 baud codes.** `7` and `C` are both 2400; `B` (1200) carries a handwritten `?`. No
  code is given twice for any other rate. The generator is a 4702; its own table is not in this
  manual. **Unresolved** — do not build a rate table from this scan alone.
- ⚠ **`ERROR` reads backwards.** §3.2 says `ERROR = 1 indicates that no parity, overrun, or framing
  error has occurred`. That could be a misprint or an active-low line. **Unresolved**; the sample
  status wiring drops `ERROR`, so shipped software does not depend on it.
- ⚠ **DTR meaning.** §3.3 says status bit 7 "indicates that the peripheral is ready"; the shipped
  driver and a handwritten note ("Busy from Ptr" at pin 20) treat it as busy. **Settled by:** the
  Table B.1 note — the bit shows the **actual line level**, and the peripheral decides what it means.
- ⚠ **Manual cross-references are wrong.** §1.5 sends the reader to "Chapter 4" for ROM driver
  examples; there is no Chapter 4. The TOC lists §3.5 *Software Examples*; the page prints §3.8. The
  TOC names two "Appendix A"s (the second is B).
- **SYNDET bits must be 0.** The board does not use USART pin 16; mode-word bit 6 and status bit 6
  are always 0.
- **Reset.** `EXT CLR*` low resets both the UART and the USART. The USART then needs a full
  mode/command re-initialisation.
- **The baud register is write-only at `base+0`; the same address read is the status byte.**
- **No interrupts.** Nothing in the manual provides an interrupt output or a vector.
