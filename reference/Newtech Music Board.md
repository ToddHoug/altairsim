# Newtech Model 6 Music Board — 6-bit DAC, amplifier and speaker

Sources: [Newtech Music Board.pdf](#) and [Computer Music part 1 (0976).pdf](#)

Newtech Computer Systems, Inc. (131 Joralemon Street, Brooklyn, N.Y.), *Model 6 Music Board
Users Manual*, © 1977, Rev. A, June 1977 — 22 scanned pages. The Model 6 is an S-100 board
with one write-only output port: a 6-bit latch, an R/2R digital-to-analog converter, an LM380
audio amplifier, a 2" speaker, a volume control and an RCA phono jack. It was sold assembled
and tested for $59.95. The manual gives two programs, MICROSCORE (BASIC) and MICROPLAY
(8080), and two test routines.

The second source is the article the manual names as a source of more software: Hal
Chamberlin, "Computer Bits — Computer Music", *Popular Electronics*, September 1976,
pp. 116–119 (4 scanned pages). It is part 1 of 2; the October part is not in the scan.

The manual has no printed page numbers, so the page numbers here are **PDF pages**. The
article is cited by its printed pages. This file omits the marketing sheet, the warranty and
the speaker-safety advice.

**Not emulated.**

---

## 1. Quick reference for emulation

| Item | Value |
|------|-------|
| Ports | **One output port.** No input port, no status, no interrupt. |
| Default address | **`24H`** (044 octal). Newtech's software uses it. |
| Ports answered | **Four adjacent ports**: A1 and A0 are not decoded. Default `24H`–`27H`. |
| Decode | SOUT high · pWR low · A7–A4 as jumpered · **A3 = 0** · **A2 = 1** |
| Address choices | `x4H`–`x7H` for x = 0…F (16 blocks, A7–A4 by jumper) |
| Data latched | **DO7–DO2** (6 bits) in a 74LS174. **DO1 and DO0 are ignored.** |
| DAC | 6-bit R/2R ladder, DO7 = MSB. Output from 0 V to almost +5 V, unsigned. |
| Step | 5 V / 64 ≈ 78 mV at the ladder (derived) |
| Reset | **None.** The latch clear pin is tied to +5 V through R13. |
| Audio | AC-coupled to an LM380 (gain about 50), then to the speaker and to J1 |
| Bus power | +18 V (pin 2) → +12 V; +8 V (pin 51) → +5 V; ground pins 50 and 100 |

The board holds the last value written. Sound comes only from the program changing the value
at an audio rate; the board has no oscillator and no timer.

---

## 2. Address decode (pp. 4–6)

IC1 (74LS30, 8-input NAND) goes low only when all eight inputs are high. The latch clock
(IC3 pin 9) is the IC1 output (pin 8).

| IC1 pin | Signal | S-100 pin | Path |
|---------|--------|-----------|------|
| 1 | A7 | 83 | direct or through inverter 2A, by jumper |
| 12 | A6 | 82 | direct or through inverter 2B, by jumper |
| 11 | A5 | 29 | direct or through inverter 2C, by jumper |
| 2 | A4 | 30 | direct or through inverter 2D, by jumper |
| 3 | A3 | 31 | through inverter 2F — must be **0** |
| 6 | A2 | 81 | direct — must be **1** |
| 4 | pWR (active low) | 77 | through inverter 2E |
| 5 | SOUT | 45 | direct |

A1 and A0 do not go to the board.

```
 A7  A6  A5  A4  A3  A2  A1  A0
  0   0   1   0   0   1   X   X      = 24H (the address as supplied)
 \--selectable--/ \-fixed-/ \don't care/
```

### Table 1 — output port address selection (p. 5)

The jumpers go in a 16-pin position: J1 is pins 1–16, J2 is 2–15, J3 is 3–14, J4 is 4–13,
J5 is 5–12, J6 is 6–11, J7 is 7–10, J8 is 8–9. One jumper of each pair is fitted:

| Bit | Jumper for 1 | Jumper for 0 |
|-----|--------------|--------------|
| A7 | J1 | J2 |
| A6 | J3 | J4 |
| A5 | J5 | J6 |
| A4 | J7 | J8 |

| High digit | Ports selected (hex) | Jumpers fitted |
|------------|----------------------|----------------|
| 0 | 04, 05, 06, 07 | J2 J4 J6 J8 |
| 1 | 14, 15, 16, 17 | J2 J4 J6 J7 |
| **2** | **24, 25, 26, 27** (standard) | **J2 J4 J5 J8** |
| 3 | 34, 35, 36, 37 | J2 J4 J5 J7 |
| 4 | 44, 45, 46, 47 | J2 J3 J6 J8 |
| 5 | 54, 55, 56, 57 | J2 J3 J6 J7 |
| 6 | 64, 65, 66, 67 | J2 J3 J5 J8 |
| 7 | 74, 75, 76, 77 | J2 J3 J5 J7 |
| 8 | 84, 85, 86, 87 | J1 J4 J6 J8 |
| 9 | 94, 95, 96, 97 | J1 J4 J6 J7 |
| A | A4, A5, A6, A7 | J1 J4 J5 J8 |
| B | B4, B5, B6, B7 | J1 J4 J5 J7 |
| C | C4, C5, C6, C7 | J1 J3 J6 J8 |
| D | D4, D5, D6, D7 | J1 J3 J6 J7 |
| E | E4, E5, E6, E7 | J1 J3 J5 J8 |
| F | F4, F5, F6, F7 | J1 J3 J5 J7 |

Fitting both jumpers of a pair shorts an inverter output to its input and can destroy IC2.

---

## 3. Latch and DAC (pp. 6, 8)

| Data bit | S-100 pin | IC3 (74LS174) in → out | IC4 (CD4050) buffer | Series resistor |
|----------|-----------|------------------------|---------------------|-----------------|
| DO7 (MSB) | 90 | 14 → 15 | 4F | R1 |
| DO6 | 40 | 6 → 7 | 4A | R2 |
| DO5 | 39 | 4 → 5 | 4E | R3 |
| DO4 | 38 | 3 → 2 | 4D | R4 |
| DO3 | 89 | 13 → 12 | 4B | R5 |
| DO2 (LSB) | 88 | 11 → 10 | 4C | R6 |

- R1–R6 are 20 kΩ 1% (the "2R" legs). R7–R12 are 10 kΩ 1% (the "R" chain, R12 to ground).
- The CMOS buffers swing very close to 0 V and +5 V, so the ladder output goes from 0 to 5 V.
- The ladder output is taken at the R1 end and goes to C7.
- IC3 pin 1 (clear) goes to +5 V through R13 (4.7 kΩ). No bus signal clears the latch.

---

## 4. Audio path and power (pp. 7–9)

| Stage | Parts | Value |
|-------|-------|-------|
| Coupling and divider | C7 (0.1 µF), R14 (2 MΩ), R15 (25 kΩ volume pot) | 5 V × 25K / (2M + 25K) ≈ **0.062 V** p-p at IC5 |
| Amplifier | IC5, LM380 | gain about **50** → about **3.08 V** p-p |
| Maximum square wave | | 3.08 / 2 ≈ 1.54 V RMS |
| Maximum power into 8 Ω | | (1.54)² / 8 ≈ **0.3 W**; peak current 1.54 / 8 = 0.19 A |
| Output filter | R16 (1 Ω), C10 (0.1 µF) | for amplifier stability |
| Output coupling | C6 (220 µF) | |
| On-board speaker | SPKR, 8 Ω 0.2 W, through **JPR1** | remove or cut JPR1 to use an external speaker |
| Jack | J1 (RCA phono), through **JPR2** | external 8 Ω speaker, or the auxiliary input of an audio system |

Power: IC6 (7812 or 340T-12) makes +12 V for IC5 from +18 V (pin 2). IC7 (78L05) makes +5 V
for the logic and the DAC from +8 V (pin 51).

The manual warns that a program can make frequencies below and above the audible range,
which a speaker system still receives.

---

## 5. Parts list (p. 19; p. 20 is the same page again)

| Ref | Part |
|-----|------|
| IC1 | 74LS30 |
| IC2 | 74LS04 |
| IC3 | 74LS174 |
| IC4 | CD4050 |
| IC5 | LM380 |
| IC6 | 7812 or 340T-12 |
| IC7 | 78L05 |
| R1–R6 | 20 kΩ 1% |
| R7–R12 | 10 kΩ 1% |
| R13 | 4.7 kΩ 10% |
| R14 | 2 MΩ 10% |
| R15 | potentiometer, 25 kΩ |
| R16 | 1 Ω 10% ½ W |
| C1, C2, C3 | 1 µF 35 V tantalum |
| C4, C5, C7, C9, C10 | 0.1 µF 12 V disc |
| C6 | 220 µF 25 V |
| C8 | 4.7 µF 10 V tantalum |
| SPKR | 8 Ω 0.2 W |
| J1 | phono jack |

---

## 6. Shipped software

Both programs assume an **8080 at 2 MHz with no wait states**.

### 6.1 Test routines (p. 18)

Ramp. The manual calls it a triangular ramp of 312.5 Hz: 25 clock cycles a step, 256 steps.

```
0000 3C          START  INR  A
0001 D3 24              OUT  24H
0003 C3 00 00           JMP  START
```

Maximum-amplitude square wave, the reference for setting the volume controls. The manual
gives 1005 Hz.

```
0000 97          START  SUB  A
0001 06 40       LOOP1  MVI  B,64
0003 05                 DCR  B
0004 C2 03 00           JNZ  $-4
0007 2F                 CMA
0008 D3 24              OUT  24H
000A C3 01 00           JMP  LOOP1
```

### 6.2 MICROPLAY Rev. A (pp. 13, 16–17)

Origin `0000H`. The score starts at `SCORE` = `0100H`. The port is `MODL6` = `24H`.

**Score format — 3 bytes a note:**

| Byte | Content |
|------|---------|
| 0 | pitch constant (a delay count). **0 = end of score.** |
| 1 | duration, low byte |
| 2 | duration, high byte **plus 1** |

The main loop copies the three bytes into the operands of four instructions in `PLAY`
(`XFER1`…`XFER4`). The program changes its own code, so it must run in RAM. At the end of the
score it loops at `HERE` (`JZ HERE`).

```
0000 31 7F 00    BEGIN  LXI  SP,STACK
0003 21 00 01    INIT   LXI  H,SCORE
0006 22 6D 00           SHLD PLACE
0009 2A 6D 00    NEXT   LHLD PLACE
000C 3E 00              MVI  A,0
000E BE                 CMP  M
000F CA 0F 00    HERE   JZ   HERE        ;end of score: loop here
0012 7E                 MOV  A,M         ;pitch
0013 32 41 00           STA  XFER2+1
0016 32 60 00           STA  XFER4+1
0019 23                 INX  H
001A 7E                 MOV  A,M         ;duration LSB
001B 32 3A 00           STA  XFER1+1
001E 32 5D 00           STA  XFER3+1
0021 23                 INX  H
0022 7E                 MOV  A,M         ;duration MSB
0023 32 3B 00           STA  XFER1+2
0026 32 5E 00           STA  XFER3+2
0029 23                 INX  H
002A 22 6D 00           SHLD PLACE
002D CD 33 00           CALL PLAY
0030 C3 09 00           JMP  NEXT

0033 21 65 00    PLAY   LXI  H,TBL1      ;envelope pointer
0036 1E 08              MVI  E,8         ;segment count
0038 7E                 MOV  A,M         ;starting amplitude
0039 01 00 00    XFER1  LXI  B,LNGTH     ;duration count (patched)
003C 40          LOOP2  MOV  B,B         ;waste time
003D C3 40 00           JMP  XFER2
0040 16 00       XFER2  MVI  D,PITCH     ;pitch constant (patched)
0042 D3 24              OUT  MODL6       ;output half wave
0044 34                 INR  M           ;waste much time
0045 35                 DCR  M
0046 34                 INR  M
0047 35                 DCR  M
0048 34                 INR  M
0049 35                 DCR  M
004A 15          LOOP3  DCR  D           ;delay according to pitch
004B C2 4A 00           JNZ  LOOP3
004E AE                 XRA  M           ;"COMPLEMENT A." — see Quirks
004F 0D                 DCR  C           ;count down duration
0050 C2 3C 00           JNZ  LOOP2
0053 05                 DCR  B
0054 C2 40 00           JNZ  XFER2
0057 D3 24              OUT  MODL6
0059 23                 INX  H           ;next segment
005A 1D                 DCR  E
005B C8                 RZ               ;all segments done
005C 01 00 00    XFER3  LXI  B,LNGTH     ;(patched)
005F 16 00       XFER4  MVI  D,PITCH     ;(patched)
0061 7E                 MOV  A,M         ;new amplitude
0062 C3 4A 00           JMP  LOOP3

0065 95 30 D0 FE FF FF 30 85   TBL1      ;envelope, 8 segments
006D 00 00       PLACE  DW   0           ;score pointer
                 SCORE  EQU  0100H
                 MODL6  EQU  24H
                 STACK  EQU  $+10H       ;= 007FH
```

- A note is **8 envelope segments**. Each segment plays the duration count in half waves.
- The six `INR M` / `DCR M` write to the envelope table and leave it unchanged.
- The envelope comment (lines 1140–1147): maximum amplitude is output when the accumulator
  is complemented from `0FFH` to `000H` and back; minimum when it is complemented from
  `080H` to `07FH`. The 8 values in `TBL1` set the envelope for each note.

Timing of one half wave on the usual path (derived from the listing, standard 8080 cycle
counts): `MVI D` 7 + `OUT` 10 + six `INR/DCR M` 60 + N × (`DCR D` 5 + `JNZ` 10) + `XRA M` 7
+ `DCR C` 5 + `JNZ` 10 + `MOV B,B` 5 + `JMP` 10 = **114 + 15N** clock cycles =
**57.0 + 7.5N µs**, where N is the pitch constant.

### 6.3 MICROSCORE Rev. A (pp. 11–12, 14–15)

North Star BASIC (Rev. 6). It reads note strings from `DATA` statements and writes the
MICROPLAY score into memory with `FILL`, starting at `U` = 256 (`100H`). The sample score is
"The Entertainer" by Scott Joplin: 119 notes and the end mark.

**Note string — 4 or 5 characters:**

| Character | Values | Meaning |
|-----------|--------|---------|
| 1 | `A` `B` `C` `D` `E` `F` `G` | pitch in the octave (N = 1, 3, 4, 6, 8, 9, 11) |
| 2 | `#`, `!`, space | sharp (N+1), flat (N−1), natural |
| 3 | `1` `2` `3` | octave: `1` starts at A = 220 Hz, `2` at A = 440 Hz, `3` at A = 880 Hz |
| 4 | `S` `E` `Q` `H` `W` | sixteenth, eighth, quarter, half, whole (T = 16, 8, 4, 2, 1) |
| 5 (optional) | `.` | dotted: half as long again (T = 2T/3) |

The one-character string `X` ends the score (it writes a zero pitch byte). An illegal
character stops the run with `ERROR IN NOTE #n`, `DATA STRING`, `CHARACTER #c`.

**The calculation (lines 160–170, 400–420, 560–680):**

```
K1 = 2^(1/12)
K6 = 1.2                      tempo; speed is proportional to K6
P  = M, M+12 or M+24          for octave 1, 2 or 3 (M = N, N-1 or N+1)
F1 = 220 * K1^(P-1)           frequency, Hz
T1 = 10^6 / (2*F1)            half period, microseconds
K3 = (T1 - 56.5) / 7.5        pitch constant
K4 = F1 / (K6*T)              half waves in one envelope segment
D3 = INT(K4)
D4 = 2*D3 - 2*INT(D3/2)       "make duration even"
D5 = INT(D4/256)
D6 = D5 + 1                   MSB as stored
D7 = D4 - D5*256              LSB
score byte 0 = INT(K3 + .5)
score byte 1 = D7
score byte 2 = D6
```

An octave runs from A up to G#, so `C 1` is above `A 1`.

Derived from these formulas: a note lasts 8 × K4 half waves = 4 / (K6 × T) seconds, whatever
its pitch. At K6 = 1.2 a sixteenth is about 0.21 s and a whole note about 3.3 s.

The manual says each note takes roughly 8 to 10 bytes in the BASIC program and 3 bytes in the
score.

---

## 7. The Popular Electronics article (September 1976, pp. 116–119)

The manual (p. 3) says the programs in Hal Chamberlin's "Computer Bits" columns of September
and October 1976 "can easily be implemented on the Model 6". This scan is the September
column. The frequency table and the touch-tone tables that the manual mentions for the
October column are not in it. The listings are in split octal, at `000:100` and `000:000`,
with the port address left as `XXX`.

### 7.1 Fig. 1 — tone subroutine (p. 116)

Enter with the frequency parameter in C and the duration parameter in D and E. The speaker
goes on any bit of the output port.

```
000:100 076 000      TONE    MVI  A,0        (7)
000:102 323 XXX              OUT  port       (10)   printed "232" — see Quirks
000:104 101                  MOV  B,C        (5)
000:105 005          DELAY1  DCR  B          (5)
000:106 302 105 000          JNZ  DELAY1     (10)
000:111 033                  DCX  D          (5)
000:112 172                  MOV  A,D        (5)
000:113 263                  ORA  E          (4)
000:114 312 136 000          JZ   RETURN     (10)
000:117 076 377              MVI  A,377Q     (7)
000:121 323 XXX              OUT  port       (10)
000:123 101                  MOV  B,C        (5)
000:124 005          DELAY2  DCR  B          (5)
000:125 302 124 000          JNZ  DELAY2     (10)
000:130 033                  DCX  D          (5)
000:131 172                  MOV  A,D        (5)
000:132 263                  ORA  E          (4)
000:133 302 100 000          JNZ  TONE       (10)
000:136 311          RETURN  RET
```

| Fact | Value (p. 117–118) |
|------|-------|
| Overhead a half cycle | 46 clock cycles |
| Wait loop | 15 clock cycles × N |
| Half cycle at 0.5 µs a clock cycle | **23 + 7.5N µs** |
| N | 1…255; 0 counts as 256 |
| Lowest frequency | 500,000 / (23 + 7.5 × 256) = **257.33 Hz** |
| Duration parameter | M = 2TF half cycles (T seconds, F Hz), 16 bits in D and E |
| Step between notes | 7.5 µs |

### 7.2 Fig. 2 — table of musical notes (p. 117)

| Note | Hz | Period µs | Note | Hz | Period µs |
|------|--------|--------|------|--------|--------|
| C (middle) | 261.62 | 3822.3 | C | 523.25 | 1911.2 |
| C# | 277.18 | 3607.8 | C# | 554.37 | 1803.9 |
| D | 293.66 | 3405.3 | D | 587.33 | 1702.7 |
| D# | 311.13 | 3214.2 | D# | 622.25 | 1607.1 |
| E | 329.63 | 3033.8 | E | 659.26 | 1516.9 |
| F | 349.23 | 2863.5 | F | 698.46 | 1431.8 |
| F# | 369.99 | 2702.8 | F# | 739.99 | 1351.4 |
| G | 391.99 | 2551.1 | G | 783.99 | 1275.6 |
| G# | 415.30 | 2407.9 | G# | 830.61 | 1204.0 |
| A | 440.00 | 2272.8 | A | 880.00 | 1136.4 |
| A# | 466.16 | 2145.2 | A# | 932.33 | 1072.6 |
| B | 493.88 | 2024.8 | B | 987.77 | 1012.4 |
| | | | C | 1046.5 | 955.58 |

### 7.3 Fig. 4 — the article's 8-bit DAC (pp. 118–119)

This is a circuit to build on any output port. It is **not** the Model 6 circuit. Two CD4050
packages buffer the eight bits; a 4.5 V battery powers them to keep system noise out.

| Bit | Resistance to the summing point |
|-----|---------------------------------|
| 7 | four buffers, each with 47 kΩ, in parallel |
| 6 | two buffers, each with 47 kΩ, in parallel |
| 5 | 47 kΩ |
| 4 | 47 kΩ + 47 kΩ in series |
| 3 | four 47 kΩ in series |
| 2 | 390 kΩ |
| 1 | 820 kΩ |
| 0 | 1.5 MΩ |

Output is **0.0176 × N volts**, N being the last byte written. Sending 0 and 255 alternately
gives 0 to +4.5 V; sending 0 and 128 gives 0 to about 2.26 V, about half the amplitude. The
article notes that an amplitude envelope during a tone is hard to do without upsetting the
loop timing.

### 7.4 Fig. 5 — white noise (p. 119)

A 16-stage shift register in H and L with feedback from bits 15, 14, 12 and 3. The upper 8
bits go to the port.

```
000:000 021 000 000  WHITEN  LXI  D,0
000:003 041 001 000          LXI  H,1        ;shift register, non-zero
000:006 174          LOOP    MOV  A,H
000:007 323 XXX              OUT  port
000:011 017                  RRC
000:012 254                  XRA  H
000:013 017                  RRC
000:014 017                  RRC
000:015 254                  XRA  H
000:016 017                  RRC
000:017 255                  XRA  L
000:020 017                  RRC
000:021 017                  RRC
000:022 017                  RRC
000:023 346 001              ANI  1
000:025 051                  DAD  H          ;shift left 1
000:026 137                  MOV  E,A
000:027 031                  DAD  D          ;new bit 0
000:030 303 006 000          JMP  LOOP
```

### 7.5 Fig. 6 — "weird sound" routine (p. 119)

```
000:000 021 001 000  WEIRD   LXI  D,1
000:003 041 000 000          LXI  H,0
000:006 175          LOOP    MOV  A,L        ;MOV A,H for 8 more sounds
000:007 323 XXX              OUT  port
000:011 031                  DAD  D
000:012 322 006 000          JNC  LOOP
000:015 023                  INX  D
000:016 172                  MOV  A,D
000:017 263                  ORA  E
000:020 302 006 000          JNZ  LOOP
000:023 023                  INX  D
000:024 303 006 000          JMP  LOOP
```

A different sound is heard on each bit of the port.

### 7.6 Fig. 3 — NOTRAN (p. 118)

A sketch of a music language, with no program given: `TEMPO 1/4=500` (a quarter note is
500 ms), a note statement such as `1C#4,1/8` (voice, note name, `#` sharp or `@` flat, octave
with C4 = middle C, length), and a rest statement such as `R,1/2`.

### 7.7 On a Model 6

- Fig. 1 writes `000` and `377`. On the Model 6 that is the full 6-bit swing.
- Figs. 5 and 6 write a whole byte. The Model 6 converts the upper 6 bits.
- The article's amplitude rule holds: 0 and 128 give about half the amplitude of 0 and 255.

---

## Quirks worth carrying

- ⚠ **MICROPLAY: the comments say "complement", the code is `XRA M`.** The flow chart
  (p. 13: "COMPLEMENT OUTPUT VALUE IN ACCUMULATOR"), the comment at `004E` and the envelope
  comment all describe a wave that goes between a value and its complement. The listing at
  `004E` is byte `AE`, mnemonic `XRA M`, and the two agree; HL points at the envelope byte
  that A was loaded from, so A goes between **that byte and `00`**. The two readings give
  different swings for `TBL1` = 95 30 D0 FE FF FF 30 85:

  | Reading | Swing (byte − other level) for the 8 segments |
  |---------|-----------------------------------------------|
  | value ↔ complement (the comments) | 2B 9F A1 FD FF FF 9F 0B — rises, holds, falls |
  | value ↔ 00 (the code) | 95 30 D0 FE FF FF 30 85 |

  **The listing wins**: it is the shipped program, and that is what an emulated board will be
  sent. The table looks as if it was made for the complement reading. Not resolved; there is
  no second copy of the program to compare.
- ⚠ **Four ports, not one.** A program that also uses `25H`, `26H` or `27H` for another
  board collides with the Model 6.
- ⚠ **The low two data bits do nothing.** A program that puts a speaker "on any bit of the
  output port" (article Figs. 1, 5, 6) is silent on bits 0 and 1. A ramp of 256 steps is 64
  levels, each held for four writes.
- ⚠ **No reset.** The latch keeps its value through a bus reset, and its power-on value is
  not defined. The output is AC-coupled, so a held value is silence.
- ⚠ **Write-only.** A read of the port address is not decoded by this board (SOUT and pWR
  are both in the decode).
- ⚠ **MICROSCORE's lowest notes do not fit in the pitch byte** (derived, not in the manual).
  K3 must be 255 or less, so T1 ≤ 56.5 + 7.5 × 255 = 1969 µs, about 254 Hz. `A 1` (220 Hz,
  K3 = 296), `A#1` and `B 1` are over; `C 1` (261.6 Hz, K3 = 247) is the lowest note that
  fits. "The Entertainer" uses no octave-1 note below C.
- ⚠ **56.5 or 57.0 µs.** MICROSCORE uses 56.5 µs for the fixed part of a half wave. The
  MICROPLAY listing counts to 114 clock cycles, 57.0 µs (derived). The manual's number is
  transcribed as printed.
- ⚠ **1005 Hz or about 1009 Hz.** The square-wave test routine is quoted as 1005 Hz. Its
  listing counts to 991 clock cycles a half wave (64 × 15 + 4 + 10 + 10 + 7), about 1009 Hz
  at 2 MHz (derived).
- **`JNZ $-4` is `C2 03 00`.** In the test routine the target is `0003` (the `DCR B`), so
  this assembler's `$` is the address of the next instruction.
- **Article Fig. 1 prints the first `OUT` as `232 XXX`.** The second `OUT`, and Figs. 5 and
  6, print `323`. `323` octal (`D3H`) is `OUT`; `232` is a misprint.
- **The duration high byte is stored plus 1**, because MICROPLAY tests it with `DCR B` /
  `JNZ` after the low byte reaches zero. MICROSCORE does this at line 630.
- **MICROPLAY must run in RAM**, and its envelope table too: it patches four operands and
  writes to `TBL1` as a delay.
- **The author's name.** The article byline and the manual both print "Hal Chamberlain".
  The usual spelling of his name is Chamberlin.
- **The sample run's last line reads `STOP IN LINE 760`** after `SCORE COMPILATION
  COMPLETE!`; the `STOP` that follows that message is at line 750. Printed so in the scan.
