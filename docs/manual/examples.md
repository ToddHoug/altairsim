# Worked examples

These are complete sessions. **Every transcript below was captured from the program.** None of
them was typed from memory.

This chapter goes through some of the machines in `examples/` in detail, because each one shows
something about how the machine works, not only how to start it. **It does not describe all the
examples.** Every folder in `examples/` has its own README, in Markdown and as a PDF, that says
what the machine is and what to type. To see what you have, look in the folder, not in this
chapter.

---

## 1. CP/M from a floppy disk

```
$ altairsim examples/cpm/cpm22-buffered.toml
```

```
startup> RUN FF00
[console -- ^E returns to the monitor]

56K CP/M 2.2b v2.3
For Altair 8" Floppy

A>
```

### What is on the disk

`DIR` lists the files:

```
A>DIR
A: L80      COM : LADDER   COM : ED       COM : ASM      COM
A: DUMP     COM : XSUB     COM : PCGET    COM : LS       COM
A: SUBMIT   COM : LOAD     COM : SURVEY   COM : VIEW     COM
A: LADDER   DAT : LUNAR    BAS : M80      COM : MAC      COM
A: MBASIC   COM : PIP      COM : STAT     COM : DDT      COM
A: MOVCPM8  COM : NSWP     COM : SYSGEN   COM : W        COM
A: ACOPY    COM : OTHELLO  COM : STARTRK  BAS : TICTAK   BAS
A: WM       COM : WM       HLP : CRC      COM : PCPUT    COM
A: AFORMAT  COM : STARINS  BAS : IOBYTE   TXT : R        COM
A: HDIR     COM
A>
```

There are 37 files: a macro assembler, a linker, Microsoft BASIC, `DDT`, `PIP`, a text editor, a
disk formatter and Star Trek. It is a complete development system of 1977. `STAT *.*` gives the
size of each file, and the space that is left:

```
A>STAT *.*

 Recs  Bytes  Ext Acc
   31     4k    1 R/W A:ACOPY.COM
   23     4k    1 R/W A:AFORMAT.COM
   64     8k    1 R/W A:ASM.COM
   17     4k    1 R/W A:CRC.COM
   38     6k    1 R/W A:DDT.COM
   ...
   83    12k    1 R/W A:WM.COM
   22     4k    1 R/W A:WM.HLP
    6     2k    1 R/W A:XSUB.COM
Bytes Remaining On A: 18k
```

### Out, and back in

`Ctrl-E` stops the machine and gives you the monitor. Nothing is lost. `RUN` with no address
continues at the instruction that the machine was about to execute.

```
A>
STOP -- the machine is still at CA9C. RUN resumes.
C0Z1M0E1I0 A=00 BC=007F DE=CA01 HL=BC0E SP=BC37 IE=1 PC=CA9C  CALL CA78
altairsim>
```

Look at the machine while it is stopped:

```
altairsim> BOARDS
altairsim> SHOW dsk0
altairsim> DUMP 100
```

To give the keyboard back to CP/M, type:

```
altairsim> RUN
```

Your `A>` prompt is where you left it.

### Before you write anything

The disk is mounted **read/write**, and there is no undo. Copy the folder first. The folder has
everything that the machine needs, and it boots from any place:

```
$ cp -R examples/cpm my-cpm
$ altairsim my-cpm/cpm22-buffered.toml
```

When you finish writing files, **go back to the `A>` prompt before you quit.** The BIOS keeps a
track in memory, and writes it to the disk only when it next reads the console. The disks
chapter tells you why.

---

## 2. Altair BASIC from a cassette

```
$ altairsim examples/basic/basic4k.toml
```

```
startup> MOUNT acr0:tape "4K BASIC Ver 3-1.tap"
acr0:tape: mounted 4K BASIC Ver 3-1.tap
startup> LOAD "LDR4K31.HEX"
loaded 20 bytes (1 page) from LDR4K31.HEX (0000-0013)
startup> RUN 0
[console -- ^E returns to the monitor]

MEMORY SIZE?
TERMINAL WIDTH?
WANT SIN? Y

742 BYTES FREE

ALTAIR BASIC VERSION 3.1
[FOUR-K VERSION]

OK
PRINT 6*7
 42

OK
```

**742 bytes free.** Altair BASIC was Microsoft's first product.

### What the three startup lines are

They are not a boot command. **There is no boot command.** They are the three things that an
operator did in 1975:

| | |
|---|---|
| `MOUNT acr0:tape "…"` | **Put the cassette in the recorder and press PLAY.** |
| `LOAD "LDR4K31.HEX"` | **Toggle in the bootstrap.** Twenty bytes, entered by hand on the front-panel switches. MITS printed them in the manual. |
| `RUN 0` | **Set the switches to zero, press EXAMINE, then press RUN.** EXAMINE puts the switches into the program counter. Without it, RUN continues from where the machine already was. |

A machine file can do what you can type at the prompt, and nothing more. For this reason, you
can do the same yourself:

```
altairsim> REWIND acr0:tape
altairsim> RUN 0
```

`REWIND` is a command that **the cassette board adds**. It exists only because the machine has
an ACR. You need it to load the same tape a second time, as you did in 1975.

### The tape is in a startup command, not in a key

The machine file declares a front panel, a processor, a serial board, a cassette interface and
some memory. **No key in it holds the tape.** A machine file's keys describe **hardware**. The
cassette in the recorder is not hardware, and the board cannot control the motor. The `startup`
list types the `MOUNT` for you, as you would type it. The tapes chapter tells you more.

### The sense switches

The front panel's `sense` switches are set to `80`. That is `A15` up, and nothing else. The
bootstrap's own printed header says why:

```
** Set A15 on (cassette load) **
** All other switches off **
```

`A15` up means *load from the cassette*. If the setting is wrong, BASIC talks to the wrong
device, or to nothing. The tapes chapter tells you more.

### It loaded in one second, and a real Altair took two minutes

The machine and the tape both run **as fast as possible** by default. A 300-baud cassette that
took a real Altair 110 seconds loads in about one second.

To load at the real speed:

```
altairsim> SET acr0:tape rate=real
altairsim> REWIND acr0:tape
altairsim> RUN 0
```

The load now takes 110 seconds. **The guest sees the same result.** The tape has its own clock,
separate from the processor's `clock_hz`, as on the real hardware. The tapes chapter describes
both clocks.

### The version before this one: BASIC 1.0

Beside the 4K file is `basic1.toml`. It boots **"8080 BASIC VER 1.0"**, the oldest Altair BASIC,
which Bill Gates and Paul Allen wrote in 1975. It works in the same way, with one difference:

```
$ altairsim examples/basic/basic1.toml
```

**It needs two steps, not one.** The bootstrap of 4K BASIC jumps into BASIC by itself when the
tape ends. The bootstrap of BASIC 1.0 does not. It copies the tape into memory from `0000`, and
then it loops forever, because it does not know how long the tape is. The 1975 operator had the
same problem. For this reason:

1. Watch the tape load. The tape counter on the console counts up to `(100%)`.
2. Press **`Ctrl-E`** when the tape is at the end.
3. Type **`RUN 0`** to start BASIC. This is what a real operator did at the front panel.

```
altairsim> RUN 0

MEMSIZ? 
WANT SIN-COS-ATN? 

2000 BYTES FREE

8080 BASIC VER 1.0

READY
```

The prompt really is `MEMSIZ?`. Microsoft set the end-of-message bit on the `Z`, and did not use
a byte for a final `E`. At the default speed, the tape loads at once, so the counter shows the
tape at the end when you first see it. Type `SET acr0:tape rate=real` before you run, to play
the tape at the true 300-baud speed. The counter then counts through the two and a half minutes
that the load took in 1975.

---

## 3. CP/M 2.2 from an 88-HDSK hard disk

```
$ altairsim examples/hdsk/hdsk.toml
```

Example 1 boots CP/M from an 8" floppy disk. This example boots the *same operating system* from
a **hard disk**, a five-megabyte platter, where the floppy disk holds a third of a megabyte. The
88-HDSK works differently from the floppy disk boards. It is a separate controller with its own
processor, and it gives the Altair whole sectors through a handshake protocol.

```
startup> RUN FC00
[console -- ^E returns to the monitor]

HDBL 2.00
LOADING FROM 0

48K CP/M 2.2b v1.6
For MITS 88-HDSK

A0>
```

`RUN FC00` is the whole `startup` of the machine file, and it runs before you see the prompt. On
a real machine, this was EXAMINE `FC00` and RUN. `FC00` is **HDBL**, the hard-disk boot PROM. It
reads the Pack Descriptor Page of the platter, loads the boot pages that it names, and jumps
into CP/M. `LOADING FROM 0` tells you which platter HDBL used. Sense switch A11 selects it, and
the default is drive 0.

**The prompt is `A0>`, not `A>`.** This CP/M numbers the platter as well as the drive: `A0` is
drive A, platter 0. In other ways, it is the same CP/M as in example 1. `DIR`, `TYPE`, `STAT`
and the other commands work:

```
A0>DIR
A: BOOT     ASM : BIOS     ASM : MOVCPMH  COM : MAKEMOV  COM
A: SYSGEN   ASM : DUMP     COM : XSUB     COM : SUBMIT   COM
A: LOAD     COM : SYSGEN   COM : ACOPY    ASM : PIP      COM
A: STAT     COM : DDT      COM : ASM      COM : MAC      COM
A: AFORMAT  ASM : NSWP     COM : CATCHUM  COM : CRC      COM
A: L80      COM : LADDER   COM : M80      COM : MBASIC   COM
A: MITSCNVT COM : PCGET    COM : PCPUT    COM : SURVEY   COM
A: WM       COM : ZORK1    COM : ZORK2    COM : ZORK3    COM
A: IOBYTE   TXT : AFORMAT  COM : CATCHUM  DAT : LADDER   DAT
A: ZORK1    DAT : ZORK2    DAT : ZORK3    DAT : MITSCNVT TXT
A: ACOPY    COM : LS       COM
A0>
```

The disk has the source of its own bootstrap (`BOOT.ASM`) and BIOS (`BIOS.ASM`), the usual CP/M
tools, and games, among them the three Zork games. `Ctrl-E` (STOP) gives the keyboard to the
monitor at any time, and `RUN` continues. `Ctrl-C` is CP/M's warm boot, and CP/M gets it.

**There is no undo.** Drive 0 is mounted read/write, as on a real machine, and CP/M writes to
`A:` for everything that you create. The package has no second copy of the original image. Copy
the folder before you test writes, or add `readonly = true` to the drive in the machine file.

---

## 4. Altair Disk Extended BASIC 4.1 from a floppy disk

```
$ altairsim examples/diskbasic/diskbasic.toml
```

The BASIC in example 2 comes from a cassette, and it forgets everything when you turn it off.
This BASIC is on an 8" floppy disk, and it has files, a directory and a `SAVE` that takes a
name. It boots from the same DBL PROM at `FF00` as CP/M. The `startup` list runs it for you.

**It asks five questions first**, and in this example, the answers are important:

```
MEMORY SIZE? 
LINEPRINTER? C
HIGHEST DISK NUMBER? 0
HOW MANY FILES? 
HOW MANY RANDOM FILES? 

37033 BYTES FREE
ALTAIR BASIC REV. 4.1
[DISK EXTENDED VERSION]
COPYRIGHT 1977 BY MITS INC.
OK
```

Three of the five take only Return. `HIGHEST DISK NUMBER?` is `0`, because there is one drive,
and it is numbered from zero.

**`LINEPRINTER?` takes only `C`, `O` or `Q`, and asks again, with no message, for anything
else.** If you answer with Return or `N`, you get the same prompt again. It looks as if the
machine has stopped, but it has not. The tapes chapter tells you more.

---

## Where to go next

- **The examples that this chapter does not describe:** `examples/`, and the README in each
  folder.
- **Move a file between CP/M and your computer:** the file-transfer chapter (`R`, `W`, `HDIR`).
- **Use telnet to connect to the guest, or connect it to a real serial port:** the serial
  chapter.
- **Look at the bus while the machine runs:** *The Debugger*.
