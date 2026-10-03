# SD Systems SBC-100 & SBC-200 Single-Board Computers

Source: [SBC-100 Manual.pdf](#), [SDS_SBC-200.pdf](#)

SD Sales / SD Systems, Dallas TX. Two revisions of one S-100 **Z80 single-board computer** —
CPU, serial port, parallel port, counter/timer, RAM and boot-PROM sockets on a single S-100
board that acts as the system's bus master. The **SBC-100** (Rev A, 1980) runs at 2.4576 MHz;
the **SBC-200** (Rev C, 1981) is the same architecture clocked at 4 MHz with a couple of added
features. They share their entire I/O port map, register layout and memory-mapping scheme, so
they are documented together with the differences called out in §8. This is a distilled
emulation reference: kit-assembly steps, parts lists, PCB layouts, alignment procedures,
marketing and prices from the original manuals are intentionally omitted; only what is needed
to emulate the board in software is kept.

The boot PROM that ships in these boards is the SD/MS monitor — see
[`SD Systems Monitor.md`](SD%20Systems%20Monitor.md). Together with a
[`VersaFloppy`](SD%20Systems%20VersaFloppy.md) controller they run
[`SDOS`](SD%20Systems%20SDOS.md) or CP/M.

---

## 1. What the board is

- **CPU: Zilog/Mostek Z80** (MK3880 on the SBC-100, MK3880-4 / Z80A on the SBC-200). Both are
  strictly Z80 — neither manual offers an 8080/8085 option.
- **Serial: one Intel 8251 USART** (8251 on SBC-100, 8251A on SBC-200), asynchronous or
  synchronous.
- **Counter/timer: one Mostek MK3882 Z80-CTC**, four 16-bit channels. Channel 0 is normally the
  16× (or 64×) baud-rate clock for the USART; the four channels can also serve as Z80 mode-2
  vectored-interrupt inputs.
- **RAM: 1 KB static scratchpad** (2114/4114), strappable to any address.
- **PROM: four sockets** (ROM 0–3), each holding a 1K/2K/4K/8K device, strappable anywhere in
  the map. The board's PROM normally holds the monitor.
- **Parallel: one 8-bit input port and one 8-bit output port**, each with two handshake lines.
- **Interrupts: four maskable vectored inputs (via the CTC) plus NMI.**
- **S-100 bus master.** The board *is* the CPU board; it releases the bus on DMA (address,
  data-out and status buffers tri-state while `BUSAK=1`). Onboard memory takes priority over
  off-board memory at the same address.

Connectors: **J1** = S-100 bus, **J2** = serial I/O (26-pin), **J3** = parallel I/O (26-pin).

---

## 2. I/O port map (identical on both boards)

Addresses are **hex only** (the manuals use no octal). Decoded on the low address bits.

| Port | Device / function |
|------|-------------------|
| **78H** | CTC channel 0 (baud-rate generator) |
| **79H** | CTC channel 1 |
| **7AH** | CTC channel 2 |
| **7BH** | CTC channel 3 |
| **7CH** | **8251 USART — data** (`IN` = RX data, `OUT` = TX data) |
| **7DH** | **8251 USART — status (`IN`) / control (`OUT`)** |
| **7EH** | Parallel port **data** latch — `OUT` writes the output latch, `IN` reads the input latch |
| **7FH** | Parallel port **handshake/control** register (§4); a **read of 7FH also releases the auto-start jam** (§5) |

This is the console the monitor and every SD OS uses: **console data = 7CH, console status =
7DH** (the "MS" monitor build's `CDATA`/`CSTAT` equates).

---

## 3. Serial section (8251 USART, ports 7CH/7DH)

Standard Intel 8251 register model. Status byte at `IN 7DH`, bits (both flags **active-high**):

| Bit | 8251 status | Meaning |
|-----|-------------|---------|
| D0 | **TxRDY** | transmitter ready — `OUT 7CH` will be accepted |
| D1 | **RxRDY** | a received byte is available at `IN 7CH` |
| D2 | TxEMPTY | transmitter shift register empty |
| D3 | PE | parity error |
| D4 | OE | overrun error |
| D5 | FE | framing error |
| D6 | SYNDET | sync detect |
| D7 | DSR | data-set-ready input |

The monitor's poll idioms (the emulation test vectors):

```
    IN   A,(7DH) / AND 1  / JP Z,txwait   ; wait for TxRDY (bit 0)
    IN   A,(7DH) / AND 2  / JP Z,rxwait   ; wait for RxRDY (bit 1)
    IN   A,(7CH) / AND 7FH                ; read byte, strip parity bit
```

**Baud generation.** The USART runs in 16× (or 64×) mode off CTC channel 0 at port 78H. The
monitor programs the 8251 mode/command, then loads the CTC time-constant:

```
    LD A,4EH / OUT (7DH),A     ; 8251 mode  (4FH on SBC-200 for 150/300 baud → ÷64)
    LD A,37H / OUT (7DH),A     ; 8251 command (RxE, TxEN, DTR, RTS)
    LD A,<ctc> / OUT (78H),A   ; CTC ch0 control (05H on SBC-100, 45H on SBC-200)
    LD A,<const> / OUT (78H),A ; CTC time constant (baud, table below)
```

**SBC-100** baud table (φ = 2.4576 MHz, USART ×16 throughout):

| Baud | 110 | 300 | 600 | 1200 | 2400 | 4800 | 9600 |
|------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| CTC const (hex) | 57 | 20 | 10 | 08 | 04 | 02 | 01 |

**SBC-200** baud table (φ = 4.00 MHz; USART ÷64 for 150/300, ÷16 for 600–9600):

| Baud | 150 | 300 | 600 | 1200 | 2400 | 4800 | 9600 |
|------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| CTC const (hex) | D0 | 68 | D0 | 68 | 34 | 1A | 0D |

The monitor **auto-detects baud** by timing the start bit of the first character the user types
(a CR), then loading the matching constant — which is why the serial jumpers loop RXD back to
DSR in "terminal" mode.

---

## 4. Parallel section (ports 7EH/7FH)

- **Output:** `OUT 7EH` latches 8 bits to J3 (tri-state, optionally gated by the `ORPLY` input).
  `OSTB` (output strobe) is a one-bit latch at **7FH bit 0**, jumperable as a positive or
  negative pulse and optionally auto-reset by `ORPLY`. `ORPLY` (device-ready) is read at
  **7FH bit 0**.
- **Input:** an external `ISTRB` positive edge sets a flip-flop; **`IN 7FH` bit 1 = 0 means a
  byte is available** (active-low "data ready"). The flop clears when the byte is read from
  `IN 7EH`; its complement drives `IRPLY` back to the sender.

J3 pinout: pin 1 GND; odd pins 3–17 = PDO0..PDO7; 19 = ORPLY; 21 = OSTB; 23 = +5 V; even pins
4–18 = PDI0..PDI7; 20 = IRPLY; 22 = ISTRB.

**⚠ SBC-200 only:** `IN 7FH`/`OUT 7FH` **bit 1 also switches the on-board memory in and out.**
`OUT 7FH` with `A=2` switches the onboard 1 KB RAM/PROM *out* of the map; `A=0` switches it back
*in*. The board always has onboard memory enabled after reset; when switched in, writes to the
1 KB RAM also write through to any off-board RAM at that address. Bit 0 remains the parallel
output handshake. The SBC-100 does not have this mechanism.

---

## 5. Memory mapping, boot PROM window and auto-start

The board holds **1024 bytes of static RAM** (U19, U20) and **four ROM/PROM sockets** (ROM 0–3 =
U36–U39). Jumpers set where each one sits in memory. "The memory on the SBC-200 takes priority
over any memory on another board which might occupy the same memory addresses."

**Memory-mapping headers** (identical scheme on both boards):

- **X1** — the ROM chip size (Table 2-3) and the memory bank (Table 2-4).
- **X2** — ROM *type*: routes A10/A11/A12 to the socket's address pins for the fitted device
  (2758/2716/2732 EPROMs; 2308/2316/2332 mask ROMs; Mostek 34000/32000/36000; 93451 PROM).
- **X3** — which slots of the bank are used (Table 2-5). Only the slots jumpered on X3 occupy
  the map, so the board can claim just its 1 KB alongside a 64 KB EXPANDORAM.

The etch default is **2K ROMs in bank 3**, the top of memory.

### Table 2-3 — ROM size selection (X1)

| Size per chip | Jumpers |
|---|---|
| 1K | X1-1 to X1-2, X1-3 to X1-4, X1-5 to X1-6 |
| 2K | X1-2 to X1-3, X1-4 to X1-5, X1-6 to X1-7, X1-8 to X1-10 |
| 4K | X1-2 to X1-5, X1-4 to X1-7, X1-6 to X1-9, X1-8 to X1-10, X1-10 to X1-12 |
| 8K | X1-2 to X1-7, X1-4 to X1-9, X1-6 to X1-11 |

### Table 2-4 — Memory bank selection (X1)

A bank is **eight slots of the chip size**: 8K for 1K chips, 16K for 2K, 32K for 4K, 64K for 8K.

| Chip size | Bank | Addresses | Jumpers |
|---|---|---|---|
| 1K | 0 | 0000–1FFF | X1-8 to X1-13, X1-10 to X1-15, X1-12 to X1-16 |
| 1K | 1 | 2000–3FFF | X1-8 to X1-7, X1-10 to X1-15, X1-12 to X1-16 |
| 1K | 2 | 4000–5FFF | X1-8 to X1-13, X1-10 to X1-9, X1-12 to X1-16 |
| 1K | 3 | 6000–7FFF | X1-8 to X1-7, X1-10 to X1-9, X1-12 to X1-16 |
| 1K | 4 | 8000–9FFF | X1-8 to X1-13, X1-10 to X1-15, X1-12 to X1-11 |
| 1K | 5 | A000–BFFF | X1-8 to X1-7, X1-10 to X1-15, X1-12 to X1-11 |
| 1K | 6 | C000–DFFF | X1-8 to X1-13, X1-10 to X1-9, X1-12 to X1-11 |
| 1K | 7 | E000–FFFF | X1-8 to X1-7, X1-10 to X1-9, X1-12 to X1-11 |
| 2K | 0 | 0000–3FFF | X1-10 to X1-15, X1-12 to X1-16 |
| 2K | 1 | 4000–7FFF | X1-10 to X1-9, X1-12 to X1-16 |
| 2K | 2 | 8000–BFFF | X1-10 to X1-15, X1-12 to X1-11 |
| 2K | 3 | C000–FFFF | X1-9 to X1-10, X1-11 to X1-12 |
| 4K | 0 | 0000–7FFF | X1-12 to X1-16 |
| 4K | 1 | 8000–FFFF | X1-12 to X1-11 |
| 8K | 0 | 0000–FFFF | X1-8 to X1-10, X1-10 to X1-12, X1-12 to X1-14 (SBC-100 manual: none) |

The SBC-200 manual prints `X-15` for `X1-15` in the 1K bank 1 row.

### Table 2-5 — ROM and RAM memory space (X3)

Each X3 jumper puts one device in one slot of the bank. ROM 0, 1 and 2 each have two possible
slots (low and high); ROM 3 has one. The RAM is the last slot.

| Slot | Device | Location | X3 jumper | Address in the bank |
|---|---|---|---|---|
| 0 | ROM 0 (low) | U36 | X3-1 to X3-2 | 0 × size |
| 1 | ROM 1 (low) | U37 | X3-3 to X3-4 | 1 × size |
| 2 | ROM 2 (low) | U38 | X3-5 to X3-6 | 2 × size |
| 3 | ROM 3 | U39 | X3-7 to X3-8 | 3 × size |
| 4 | ROM 0 (high) | U36 | X3-9 to X3-10 | 4 × size |
| 5 | ROM 1 (high) | U37 | X3-11 to X3-12 | 5 × size |
| 6 | ROM 2 (high) | U38 | X3-13 to X3-14 | 6 × size |
| 7 | RAM | U19, U20 | X3-15 to X3-16 | 7 × size |

**Slot address = bank × 8 × size + slot × size.** The manual prints the result for every size
and bank; the formula gives each printed row. For the etch default (2K, bank 3):

| Slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 (RAM) |
|---|---|---|---|---|---|---|---|---|
| Start | C000 | C800 | D000 | D800 | E000 | E800 | F000 | F800 |

So the monitor PROM at `E000` is ROM 0 in its high slot, the disk BIOS PROM at `F000` is ROM 2
in its high slot, and the RAM is `F800`–`FFFF`.

**The RAM takes a whole slot.** "When the on board 1K Static RAM is used, it occupies the same
amount of memory as each of the ROM/PROM sockets. For example, if 2K ROM/PROMS are used, the 1K
RAM occupies two contiguous 1K blocks, redundantly." With 2K chips the same 1K is at `F800` and
at `FC00`.

**Switching the onboard memory out (SBC-200 only, manual §2.4.4).** `OUT 7FH` with bit 1 set
(`LD A,2` or `3`) switches all onboard memory out of the map; with bit 1 clear (`LD A,0`) it
switches it back in. Bit 0 is the parallel port's handshake bit. While the onboard memory is
switched out, a memory board may occupy the same addresses. "While the on-board memory is
switched in, any memory writes to the 1K RAM also writes to the memory on the other board
containing memory at that address. The SBC-200 always enables the on-board memory upon reset."

### Auto-start (X16, X17, X18)

"Since many systems require RAM starting at address 0, the SBC-200 has the capability of
automatically causing control to begin on any 4K boundary upon resetting the board." The start
address is any multiple of `1000H`, `0000H`–`F000H`.

**Table 2-6 (SBC-200) / Table 3-1 (SBC-100)** — each jumper sets one bit of the start address:

| Address bit | Header | 0 | 1 |
|---|---|---|---|
| A15 | X17 | X17-2 to X17-3 | X17-1 to X17-2 |
| A14 | X18 (pins 4-5-6) | X18-5 to X18-6 | X18-4 to X18-5 |
| A13 | X16 | X16-2 to X16-3 | X16-1 to X16-2 |
| A12 | X18 (pins 1-2-3) | X18-2 to X18-3 | X18-1 to X18-2 |

The manuals print all sixteen rows (`0000`, `1000`, … `F000`); each row is the four jumpers for
its four bits. Two rows of the SBC-200 table are misprinted. The SBC-100 table has them right:

| Row | SBC-200 prints | Correct (SBC-100 table) |
|---|---|---|
| `5000` | X18-2 TO X18-2 | X18-1 to X18-2 |
| `9000` | X17-2 TO X17-2 | X17-1 to X17-2 |

The code at the start address must release the circuit with its first two instructions:

```
X000  JP  X003        ; C3 03 X0
X003  IN  A,(7FH)     ; DB 7F
```

"This resets the hardware which caused execution to occur at X000 instead of 0000. The only
case where these instructions are not needed is when X=0 i.e. when resetting to 0000."

- "The S.D. Monitor resides at E000 and requires that the jumpers be set to cause an auto start
  to that address. When resetting to the disk controller prom (BIOS), set the auto start for
  F000."
- The board is etch-jumpered for `E000` or `F000`; only the last jumper (**X18-2**) must be
  connected, and it selects between the two.
- **What the manuals do not say.** The SBC-200 manual's circuit analysis (§2.3) is blank, and
  neither manual describes the circuit. The `JP X003` is to an absolute address in the PROM, so
  after it the program counter is in the PROM and the read of port 7FH can release the circuit.
  That the circuit supplies all of A12–A15 until the release is read from the table (four
  jumpers, four bits); it is not confirmed from a schematic analysis.
- **What the PROMs do.** The MSMONR21 and SDMONV21 monitor PROMs open with `JP E00F`
  (`C3 0F E0`), and `E00F` is `IN A,(7FH)`. That is the manual's sequence with a different
  target: a jump to an absolute address in the PROM, then the read of port 7FH.
- There is **no separate "phantom" jumper** documented. (A `PHANTOM` net exists on the SBC-200
  schematic but is not user-documented.)

**SBC-100 manual.** It has the same tables under other numbers: bank selection is its Table 2-3,
the X3 slot table is its Table 2-4, and the auto-start table is its Table 3-1 (Section III).
The addresses and jumpers agree with the SBC-200 manual, except the 8K bank row above. It has
no memory switch-out (§8).

---

## 6. Interrupts

Four maskable vectored inputs through the CTC (channels at 78H–7BH) plus NMI. Header jumpers map
the CTC channels to the S-100 vectored-interrupt lines: Ch0←VI1, Ch1←VI2/SYNDET,
Ch2←VI3/serial-RxRDY, Ch3←VI4/serial-TxRDY. The board sits in the Z80 mode-2 daisy chain (IEI/
IEO). Reset always begins in the auto-start/monitor path (§5), not an interrupt.

---

## 7. Reset behavior summary

On power-on / reset: onboard memory enabled, Z80 begins at the X16/X17/X18 auto-start 4K
boundary (E000H for the monitor, F000H for a floppy boot), runs `JP`+`IN 7FH` to drop the
override, then the monitor initializes the 8251 (mode 4E/4F, command 37) and CTC ch0, auto-
detects baud from the first typed CR, and prints its `.` prompt (see the Monitor reference).

---

## 8. SBC-100 vs SBC-200 — differences

| Aspect | SBC-100 | SBC-200 |
|--------|---------|---------|
| CPU | Z80 (MK3880) | Z80A (MK3880-4) |
| Clock (φ) | **2.4576 MHz** (4.9152 MHz xtal ÷2) | **4.00 MHz** (16 MHz osc ÷) |
| Clock jumper | X8 (÷2 standard / no-÷2) | X8 (4 MHz standard / 2 MHz) |
| USART | 8251 | 8251A |
| CTC | MK3882 | MK3882-A-4 |
| Baud range | 110–9600, all USART ×16 | **150–9600**, USART ÷64 (150/300) or ÷16 |
| 8251 mode byte | `4EH` always | `4EH`, but **`4FH`** for 150/300 baud |
| CTC ch0 control byte | `05H` | `45H` |
| Serial interface | RS-232 **+ 20 mA current loop**; headers X4/X6/X9/X10/X11; fixed cable | RS-232 only; single header **X20** + X11 terminal/printer + X21 baud routing |
| On-board memory switch-out | — | **`OUT 7FH` bit 1** disables onboard memory (§4) |
| DMA driver-disable jumper | — | **X22** (J1-19 pDBDIS vs BUSAK disables the bus drivers) |
| Default etch | Rev B, 2716 top bank | Rev A, 2716 top bank |

**Identical on both:** the whole port map (USART 7C/7D, parallel 7E/7F, CTC 78–7B); 8251 status
polarity (TxRDY=D0, RxRDY=D1, active-high; parallel "data ready" = 7FH D1 active-low); 1 KB RAM;
four ROM sockets; the X1/X2/X3 memory-mapping headers; auto-start via X16/X17/X18 with the port-
7FH release; monitor at E000H / disk BIOS at F000H; J3 parallel pinout; CTC-as-interrupt mapping
to VI1–VI4.

---

## 9. Emulation checklist (summary of load-bearing facts)

- **Z80 CPU, S-100 bus master.** SBC-100 = 2.4576 MHz, SBC-200 = 4.00 MHz.
- **Console = 8251 at 7CH (data) / 7DH (status/control).** TxRDY = status D0, RxRDY = D1, both
  active-high; strip parity with `AND 7FH` after reading data.
- **CTC (MK3882) at 78H–7BH;** channel 0 is the baud generator (control byte 05H SBC-100 / 45H
  SBC-200, then the time constant). Baud is auto-detected from the first typed CR.
- **Parallel at 7EH (data) / 7FH (handshake).** Input "byte ready" = `IN 7FH` D1 **= 0**
  (active-low). **SBC-200: `OUT 7FH` bit 1 switches onboard memory out(1)/in(0).**
- **Reading port 7FH releases the reset auto-start jam.** Reset begins at a 4K boundary
  (X16/X17/X18) — monitor PROM at **E000H**, floppy-boot BIOS at **F000H** — not at 0000H.
- **1 KB RAM, four PROM sockets** (1K/2K/4K/8K each), all strappable; onboard memory has bus
  priority.
- **Interrupts:** four maskable vectored via the CTC (→ VI1–VI4) + NMI; Z80 mode-2 daisy chain.
