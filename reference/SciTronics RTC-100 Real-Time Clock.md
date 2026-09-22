# SciTronics RTC-100 Real-Time Clock

Source: [RTC-100_Real-Time_Clock.pdf](#)

SciTronics Inc. (Bethlehem, PA), *RTC-100 Real-Time Clock User's Manual*, 47 pp, software
© 1980, schematic dated 9-7-80. An S-100 battery-backed calendar clock: an **OKI MSM5832**
clock/calendar chip (crystal 32.768 kHz, 3 V lithium cell, quoted .002% accuracy) presented to
the bus through a **6821 PIA** at four consecutive I/O ports, plus an optional once-a-second
interrupt that jams an `RST` onto the data bus. This document extracts what an implementer
needs; assembly, warranty and the BASIC listings are summarized, not transcribed.

**Not emulated.** Reference only.

---

## 1. Quick reference for emulation

| Item | Value |
|------|-------|
| Ports | **four consecutive**, `base+0 … base+3` (p2) |
| Base address | DIP switch **PORT** (SW1), six poles decoding **A2–A7**; A0/A1 select the PIA register (p2) |
| Valid bases | **multiples of 4**, 0 … 252 — Table I, p4 |
| PIA | **6821** (U5; the schematic also annotates 6820/6521) — port A = digit address + data, port B = write strobe (pp. 40–42) |
| Clock chip | **OKI MSM5832** (U2), 13 BCD digits, 32.768 kHz crystal (X1) (p42) |
| Data width | one **BCD digit per access**, carried in the **high** nibble of port A on read (p35) |
| Interrupt | one per **second**, vectored by jamming `11 xxx 111` = `RST xxx` during T3 of M1 (p8) |
| Interrupt switch | **INT** (SW2), 3 poles, **negative logic — ON = 0, OFF = 1** — Table II, p9 |
| Backup | 3 V lithium cell; the chip keeps time with system power off (p2) |

Port A0/A1 decode, as used by every routine in the manual:

| Port | Role |
|------|------|
| `base+0` | PIA **A** data / direction register — digit address (b0–b3) and digit data (b4–b7) |
| `base+1` | PIA **A** control register — `CA2` = clock **Hold** (stop) pulse |
| `base+2` | PIA **B** data / direction register — clock **Write** strobe |
| `base+3` | PIA **B** control register — `CB2` = clock **Read** pulse |

## 2. Base port address (pp. 2–4)

The general form is `A0 A1 A2 A3 A4 A5 A6 A7`, of which **A2 through A7 are user-defined** by
the six-pole PORT switch; **A0 and A1 are used internally to select among the clock
functions**, which is why only multiples of 4 are legal. A switch **ON** makes that address bit
valid when **high**, OFF when low. The manual's worked example sets base = 144 decimal
(220 octal / 90 hex).

> ⚠ *"Do not use the switch markings as a guide to the identification of the address bits, as
> they may vary from one production run to another"* (p3) — go by the board's silkscreen and
> Figure 1.

Table I (p4) lists all 64 legal bases against their switch patterns; the binary digits are
printed **in switch order with the S-100 connector towards you**, i.e. A2 first, A7 last — not
in the usual MSB-first order. The rule ("multiples of 4") is what matters; the table is a
lookup convenience.

## 3. Digit map

Digit address (port A bits 0–3) is the MSM5832 register number. From `READ.ASM`'s equates
(p31), which match the MSM5832 register order exactly:

| Code | Digit | Code | Digit | Code | Digit |
|---|---|---|---|---|---|
| 0 | seconds units | 5 | hours tens ⚠ | 10 | month tens |
| 1 | seconds tens | 6 | day of week (0 = Sunday) | 11 | year units |
| 2 | minutes units | 7 | day units | 12 | year tens |
| 3 | minutes tens | 8 | day tens ⚠ | | |
| 4 | hours units | 9 | month units | | |

⚠ Two digits carry a flag in their upper bits and must be masked with **`AND 3`**:
**hours tens** holds the **24-hour / AM-PM** flag, **day tens** holds the **leap-year** flag.
The manual's prose on p5 says "AND 3 to *month* tens digit" but writes *days* in the margin
correction, and both `READ.ASM` (`MVI A,HRST` / `ANI 3`, and `MVI A,MONT` / `ANI 3`) and the
p9 prose place the leap-year flag with the **day tens** digit. Mask day-tens and hours-tens;
masking month-tens as well is harmless (a month tens digit is 0 or 1).

The manual's 13-byte "clock data block" (p9) runs in the **opposite** order — year tens first,
seconds units last — because the BASIC read loop counts the digit code down (`FOR X=1 TO 13:
OUT P,13-X`).

## 4. Reading the clock (pp. 5, 37)

**Enable read** (interrupts not active):

| Out to | Value | Effect |
|---|---|---|
| `base+1` | 240 = `F0H` | PIA A control: `CA2` low = Hold high = start of stop-clock pulse |
| `base+0` | 15 = `0FH` | PIA A direction: **b0–b3 output** (digit address), **b4–b7 input** (data) |
| *delay* | ~150 µs | |
| `base+3` | **252 = `FCH`** | PIA B control: `CB2` high = start of read pulse |
| `base+1` | 244 = `F4H` | PIA A control: `CA2` low and select the data register; clock stopped |

> ⚠ **The p5 prose is wrong here, and the manual's own software proves it.** p5 prints
> `port 3 = 248 = F8H` for the read pulse and puts the two writes in the other order. The
> scan's previous owner struck the passage out and wrote **252** in the margin; both shipped
> programs agree with the margin — `READ.ASM`'s `TSTART` does `F0H→PORT1, 0FH→PORT, delay,
> 0FCH→PORT3, 0F4H→PORT1` (p34) and the North Star `RTCREAD` does
> `OUT P1,240 \ OUT P,15 \ OUT P3,252 \ OUT P1,244` (p37). **Use 252, in that order.**

**Read one digit** (repeat per digit):

```
    OUT  base+0      ; digit code 0-12
    ...              ; ~6 us delay  (READ.ASM burns a DELAY loop preset to 0FAH)
    IN   base+0      ; read the digit
    AND  0F0H        ; drop the code part
    RRC / RRC / RRC / RRC   ; the BCD digit is in the HIGH nibble
```

**Return to run mode** — again per the artifact, not the p5 prose (`TIMED`, p34; `RTCREAD`
line 280, p37):

```
    248 = F8H -> base+1     ; CA2 high = end of stop pulse
     15 = 0FH -> base+0
    248 = F8H -> base+3     ; CB2 low = read pulse off
    252 = FCH -> base+1
     15 = 0FH -> base+0
```

## 5. Setting the clock (pp. 6–7)

**Enable set** (interrupts not active): `255 = FFH` to `base+0` **and** `base+2` (preset both
PIA ports to output), then `244 = F4H` to `base+1` (`CA2` low = Hold high = start of
stop-clock pulse) and `244 = F4H` to `base+3` (`CB2` low = read off), then a **150 µs** delay.

**Write one digit** — port A carries address and data in one byte:

```
 b0-b3 = a0-a3   digit select address
 b4-b7 = d0-d3   data to clock
```

then strobe: `0` to `base+2` (write pulse on), `1` to `base+2` (write pulse off). Repeat per
digit.

**Return to run:** `248 = F8H` to `base+1` (`CA2` high = end of stop pulse = run).

Setting always zeroes seconds, so the shipped `RTCSET` asks for the *next* whole minute and
waits for the operator's carriage return on the zero second (p6, p37).

## 6. Interrupt mode (pp. 8–9)

The clock can request an interrupt **once per second**. On acknowledge (INTA status, T3 of M1)
the board jams an `RST` onto the data bus:

```
 B0        B7
 |         |
 1 1 1 x x x 1 1      ; xxx = the interrupt number, LSB-first as printed
```

so control goes to **8 × the interrupt number**. The example given: `RST 2` places `11010111`
on the bus and vectors to 16.

Table II — valid interrupt addresses (**switch is negative logic: ON = 0, OFF = 1**):

| Code (B0→B7) | INT switch | Vector |
|---|---|---|
| `111 000 11` | ON ON ON | 0 |
| `111 100 11` | OFF ON ON | 8 |
| `111 010 11` | ON OFF ON | 16 |
| `111 110 11` | OFF OFF ON | 24 |
| `111 001 11` | ON ON OFF | 32 |
| `111 101 11` | OFF ON OFF | 40 |
| `111 011 11` | ON OFF OFF | 48 |
| `111 111 11` | OFF OFF OFF | 56 |

> The manual advises against **all-on (RST 0)** and **all-off (RST 7)** — both are commonly
> taken by other devices.

Interrupts are enabled by programming the PIA to pass the seconds tick through; the shipped
`STARTS` routine does that after planting a vector, and `STOPS` reverses it. **Stopping the
interrupt does not stop timekeeping** (p11). The chip keeps time while the interrupt is off,
and while the machine is off.

## 7. Hardware (pp. 39–42)

Parts list and layout, for identification:

| Ref | Part |
|---|---|
| U2 | **MSM5832 OKI clock chip** |
| U5 | **6821 Peripheral Interface Adaptor** (schematic annotates `6521`/`6820`) |
| U0 | 7474 dual D flip-flop |
| U1, U3 | 7408 quad 2-in AND |
| U4, U6 | 74LS04 hex inverter |
| U7, U8 | 74LS136 quad XOR, open collector (the address comparator) |
| U9, U10, U11 | 74LS241 octal bus/line driver |
| X1 | 32.768 kHz crystal (C7/C8 15 pF, C9 5–30 pF trimmer) |
| B1 | 3 V lithium battery; CR1 = 1N6263 |
| SW1 | 8-position DIP — **PORT** (six poles used) |
| SW2 | 4-position DIP — **INT** (three poles used) |
| J1 | jumper, "seconds"; board also silkscreens **MSEC / SEC / HR** straps beside U5 |

> ⚠ **Note on the board itself** (p1 of the body): the lithium cell is fully charged on
> arrival — *"serious damage to the battery may result if the circuit board is placed on a
> conducting surface."*

## 8. Shipped software (Appendices, pp. 12–37)

| Program | What it is |
|---|---|
| `RTCREAD` (App. I) | BASIC — reads and displays the clock continuously, non-interrupt. Contains the reusable read subroutine. |
| `RTCSET` (App. II) | BASIC — prompts for base port, date, day-of-week and 24-hour time, then sets on the operator's carriage return. Handles leap year in hardware but not in the shipped program. |
| `RTC` (App. III) | Interrupt-driven demonstration: `SETTER`, `INTR`, `STARTS`, `STOPS` — the routines meant to be lifted into interrupt-driven software. `INTR` strips the leap-year and 24-hour flags before storing. |
| `TIME.BAS` (App. IV) | The same clock under MBASIC 5.0. |
| `READ.ASM` (App. V) | 8080 driver for a boot PROM or BIOS — §9. |
| App. VI | North Star BASIC versions of `RTCSET` and `RTCREAD` (© 1980, converted by Rice Communications). These are the tie-breaker for §4. |

The RTC-100 is also stated to be compatible with the **SciTronics Remote Controller** for
real-time control of a-c appliances (p2) — the same 13-byte clock data block format.

## 9. `READ.ASM` — the driver, as printed (pp. 31–36)

*Note in the manual: interrupts not used. `;PGM by Harry Kaemmerer 810220`.*

```asm
PROM    EQU     xxxxH           ;ADD START OF PROM ADDRESS
CONOUT  EQU     xxxxH           ;ADD YOUR CONOUT CALL ADDRESS
PORT    EQU     xxH             ;ADD BASE PORT ADDRESS FOR CLOCK

PORT1   EQU     PORT+1
PORT2   EQU     PORT+2
PORT3   EQU     PORT+3

SECU    EQU     0               ;SECONDS UNITS
SECT    EQU     SECU+1          ;SECONDS TENS
MINU    EQU     SECT+1          ;MINUTES UNITS
MINT    EQU     MINU+1          ;MINUTES TENS
HRSU    EQU     MINT+1          ;HR'S    UNITS
HRST    EQU     HRSU+1          ;HR'S    TENS
DAYW    EQU     HRST+1          ;DAY OF THE WEEK
DAYU    EQU     DAYW+1          ;DAY     UNITS
DAYT    EQU     DAYU+1          ;DAY     TENS
MONU    EQU     DAYT+1          ;MONTH   UNITS
MONT    EQU     MONU+1          ;MONTH   TENS
YEAU    EQU     MONT+1          ;YEAR    UNITS
YEAT    EQU     YEAU+1          ;YEAR    TENS

ASCII   EQU     30H             ;OFFSET TO MAKE CHAR. ASCII

        ORG     PROM

TIME:   CALL    TSTART          ;DO CLOCK INIT SUB
        CALL    DAY0            ;DISPLAY TIME
        CALL    TIMED           ;RESET CLOCK BOARD
        RET

;-------------------------------;
;      SUB ROUTINES START HERE  ;
;-------------------------------;
DAY0    LXI     H,STR3          ;POINTS TO LOOKUP TABLE FOR DAY
        LXI     B,00H           ;CLEAR B&C REGISTERS
        MVI     A,DAYW          ;DAY READ INSTRUCTION
        CALL    GETDIG1         ;GET IT
        MOV     C,A             ;MOVE DAY OF WEEK POINTER IN C.
        DAD     B               ;ADD B&C TO H&L REGISTERS
        MOV     C,M             ;GET OFFSET ADDRESS
        LXI     H,STR4          ;GET DAY NAME
        DAD     B               ;POINT TO NAME
        CALL    MESAG           ;SEND IT TO TERMINAL

HRS1    MVI     A,HRST          ;SELECT HR'S TENS DIGIT
        CALL    GETDIG1         ;GET DIGIT
        ANI     3               ;ELIMINATE EXTRA BIT
        CALL    SENDCH          ;SEND IT TO TERMINAL

HRS2    MVI     A,HRSU          ;SELECT HR'S UNITS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

HRS3    MVI     A,':'           ;LOAD ":" SEPERATOR
        CALL    SENDIT          ;SEND IT TO TERMINAL

MIN1    MVI     A,MINT          ;SELECT MINUTES TENS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

MIN2    MVI     A,MINU          ;SELECT MINUTES UNITS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

MIN3    MVI     A,':'           ;LOAD ":" SEPERATOR
        CALL    SENDIT          ;SEND IT TO TERMINAL

SEC1    MVI     A,SECT          ;SELECT SECONDS TENS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

SEC2    MVI     A,SECU          ;SELECT SECONDS UNITS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

MON1    MVI     A,MONT          ;SELECT MONTH TENS DIGIT
        CALL    GETDIG1         ;GET DIGIT
        ANI     3               ;ELIMINATE EXTRA BIT
        LXI     H,STR5          ;POINT TO OFFSET LOOKUP TABLE
        CPI     1               ;SEE IF IT IS A MONTH AFTER SEPT.
        CZ      LAB1            ;IF IT IS A ONE THEN CALL

MON2    MVI     A,MONU          ;SELECT MONTH UNITS DIGIT
        CALL    GETDIG1
        DCR     A               ;DECR. MONTH VALUE BY ONE JAN.=0 THEN
        LXI     B,0             ;CLEAR B&C REGISTERS
        MOV     C,A             ;PUT CLOCK DIGIT IN C REGISTER
        DAD     B               ;ADD B&C TO H&L REGISTERS
        MOV     C,M             ;PUT OFFSET NUMBER IN C REGISTER
        LXI     H,STR6          ;LOAD MONTH NAME POINTER
        DAD     B               ;ADD OFFSET TO H&L REGISTERS
        CALL    MESAG           ;SEND MESSAGE

DAY1    MVI     A,DAYT          ;SELECT DAY TENS DIGIT
        CALL    GETDIG1
        CPI     0               ;TEST FOR ZERO
        JZ      DAY2            ;JUMP TO DAY IF ZERO
        PUSH    PSW             ;SAVE RESULT ON STACK
        ADI     ASCII           ;MAKE IT ASCII
        MOV     C,A             ;PUT IN C REGISTER
        CALL    CONOUT          ;SEND IT
        POP     PSW             ;RESTORE A REGISTER

DAY2    MVI     A,DAYU          ;SELECT DAY UNITS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

YEA0    LXI     H,STR7          ;LOAD FIRST TWO DIGITS OF YEAR
        CALL    MESAG           ;SEND IT TO TERMINAL

YEA1    MVI     A,YEAT          ;SELECT YEAR TENS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL

YEA2    MVI     A,YEAU          ;SELECT YEAR UNITS DIGIT
        CALL    GETDIG          ;SEND IT TO TERMINAL
        RET

TSTART  MVI     A,0F0H          ;SEQUENCE TO ENABLE THE CLOCK
        OUT     PORT1
        MVI     A,0FH
        OUT     PORT

        MVI     A,0F0H          ;LOAD DELAY CONST.
        CALL    DELAY           ;DO SOME DELAY

        MVI     A,0FCH
        OUT     PORT3
        MVI     A,0F4H
        OUT     PORT1
        RET

TIMED   MVI     A,0F8H          ;RETURN CLOCK TO RUN MODE
        OUT     PORT1
        MVI     A,0FH
        OUT     PORT
        MVI     A,0F8H
        OUT     PORT3
        MVI     A,0FCH
        OUT     PORT1
        MVI     A,0FH
        OUT     PORT
        RET

MESAG   MOV     A,M             ;GET CHARACTER FROM MEMORY
        CPI     0               ;TEST FOR ZERO
        JZ      MSS1            ;IF ZERO EXIT
        CALL    CONOUT          ;SEND CHARACTER
        INX     H               ;MOVE POINTER TO NEXT CHARACTER
        JMP     MESAG           ;DO IT AGAIN UNTILL DONE
MSS1    RET                     ;RETURN

GETDIG  CALL    GETDIG1         ;READ CLOCK
SENDCH  PUSH    PSW             ;SAVE FLAGS AND ACC.
        ADI     ASCII           ;MAKE IT ASCII
        MOV     C,A             ;PUT IN ACC
        CALL    CONOUT          ;SEND IT TO TERMINAL
        POP     PSW             ;RESTORE ACC. AND FLAGS
        RET                     ;RETURN TO CALLER

SENDIT  PUSH    PSW             ;SAVE FLAGS AND ACC.
        MOV     C,A             ;PUT IN ACC
        CALL    CONOUT          ;SEND IT TO TERMINAL
        POP     PSW             ;RESTORE ACC. AND FLAGS
        RET                     ;RETURN TO CALLER

GETDIG1 OUT     PORT            ;CODE DIGIT TO SELECT CLOCK ELEMENT
        MVI     A,0FAH          ;DELAY FACTOR
        CALL    DELAY           ;DELAY SOME TIME HERE
        IN      PORT            ;READ CLOCK ELEMENT
        ANI     0F0H            ;DROP CODE PART OF DIGIT
        RRC                     ;MOVE DATA TO LOW NIBBLE
        RRC
        RRC
        RRC                     ;NUMBER RETURNED IN ACC.
        RET

LAB1    LXI     D,0             ;CLEAR D&E REGISTERS
        MVI     E,0AH           ;IF THE RESULT WAS OCTOBER OR LATER
        DAD     D               ;ADD X10 OFFSET
        RET                     ;LOAD POINTER OFFSET AND RETURN

DELAY:  INR     A
        JNZ     DELAY           ;NOT DONE DELAY SOME MORE
        RET

STR3    DB      00H,08H,10H,19H ;OFFSET LOOKUP TABLE
        DB      24H,2EH,36H     ;FOR DAY OF THE WEEK

STR4    DB      'Sunday ',0
        DB      'Monday ',0
        DB      'Tuesday ',0
        DB      'Wednesday ',0
        DB      'Thursday ',0
        DB      'Friday ',0
        DB      'Saturday ',0

STR5    DB      00H,0AH,15H,1DH ;OFFSET LOOKUP TABLE
        DB      25H,2BH,32H,39H ;FOR MONTH NAME
        DB      42H,4FH,59H,64H

STR6    DB      ' January ',0
        DB      ' February ',0
        DB      ' March ',0
        DB      ' April ',0
        DB      ' May ',0
        DB      ' June ',0
        DB      ' July ',0
        DB      ' August ',0
        DB      ' September ',0
        DB      ' October ',0
        DB      ' November ',0
        DB      ' December ',0

STR7    DB      ' 19',0         ;YEAR LEADIN MESS.
        END     PROM
```

⚠ `STR7` hard-codes the century as `19`, and every shipped program prints `"19"+year` — a
1980 board, dated accordingly.

## 10. Quirks worth carrying

- **⚠ The p5 read sequence contradicts the shipped software** on the `base+3` value and the
  order of the last two writes. `READ.ASM` and the North Star `RTCREAD` agree with each other
  and with the previous owner's margin note: **252, then 244**. See §4.
- **The digit arrives in the high nibble** on a read, and the low nibble still holds the code
  you wrote — hence `ANI 0F0H` + four `RRC`. On a *write*, address is low and data is high.
- **`AND 3` two digits**: hours-tens carries the 24-hour/AM-PM flag, day-tens the leap year
  flag (§3).
- **The 150 µs enable delay and the ~6 µs per-digit delay** are the manual's own numbers; the
  driver implements them with a plain `INR A` / `JNZ` spin, so they are loose.
- **The INT switch is negative logic** — ON = 0. Easy to invert.
- **Timekeeping never stops.** `STOPS` disables the interrupt only; the clock also keeps
  running with the machine powered off, on the lithium cell.
- The board answers **four ports and no memory**; there is no status register and nothing to
  poll — a read of `base+0` outside the hold sequence returns whatever the last digit code
  selected.
