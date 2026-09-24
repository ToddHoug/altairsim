# Quick start: CP/M in one command

```
$ ./altairsim examples/cpm/cpm22-buffered.toml
```

On **Windows**, the program is `altairsim.exe`, and the path uses backslashes:

```
> altairsim.exe examples\cpm\cpm22-buffered.toml
```

The machine boots CP/M:

```
startup> RUN FF00
[console -- ^E returns to the monitor]

56K CP/M 2.2b v2.3
For Altair 8" Floppy

A>
```

You are in CP/M. Type `DIR`:

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

The disk has an assembler, a debugger, Microsoft BASIC, a text editor and Star Trek. Run one:

```
A>MBASIC
```

## What happened

The machine file describes a **56K Altair with an 8″ floppy disk controller and a boot PROM at
`FF00`**. It puts the disk image in drive 0, and it types `RUN FF00` for you. The `startup>`
line shows this command.

**There is no `BOOT` command.** To boot a disk on a real Altair, you set the address switches
to `FF00`, pressed EXAMINE, and then pressed RUN. `RUN FF00` does the same thing. *The Monitor*
tells you more about it. A machine file can do only what you can type.

The rest of the boot is real software. The PROM loads a boot loader from the disk. The boot
loader loads CP/M into memory. The BIOS of CP/M then prints its banner.

## Where it found the disk

The machine file names its disk as `cpm22b23-56k.dsk`, with no folder. The program looks for
the disk in the folder of the machine file, `examples/cpm`. For this reason, you can copy the
folder to a different place, and the machine still boots.

A path that you type at the `altairsim>` prompt also starts from the folder of the machine file.
`SHOW PATHS` shows you this folder. The machines chapter gives the full rule.

## Getting back out: `Ctrl-E`

Press **`Ctrl-E`**. This key does what the STOP switch on the front panel of the Altair does.
It stops the machine and gives you the monitor:

```
A>
STOP -- the machine is still at CA9C. RUN resumes.
C0Z1M0E1I0 A=00 BC=007F DE=CA01 HL=BC0E SP=BC37 IE=1 PC=CA9C  CALL CA78
altairsim>
```

**The machine stops at the instruction where it was, and it loses nothing.** STOP is not RESET,
and it is not POWER. The registers, the memory, the disk in the drive and the program counter do
not change. The processor does not run while you are at the monitor. While the machine is
stopped, you can read memory, step one instruction at a time and set breakpoints.

`Ctrl-E` is the stop key because **`Ctrl-C` belongs to the software on the machine**. CP/M does
a warm boot when you press `Ctrl-C`, and BASIC stops the program. The program gets `Ctrl-E`
before the machine can read it. Every other key, `Ctrl-C` too, goes to the machine. If you need
`Ctrl-E` for something else, type `SET CONSOLE stop=1D`. The stop key is then `Ctrl-]`.

## Going back in: `RUN`

```
altairsim> RUN
```

The machine continues from the instruction where it stopped. Your `A>` prompt is where you left
it.

## Leaving: `QUIT`

```
altairsim> QUIT
```

There is no `EXIT` command. Type `QUIT`, or only `Q`.

## The three things to remember

| | |
|---|---|
| **`Ctrl-E`** | Stop the machine, and go to the monitor. The machine loses nothing. |
| **`RUN`** | Start the machine again. |
| **`QUIT`** | Leave the program. |

## Careful: the disk is real, and there is no undo

The machine mounts the disk image **read/write**. Everything that you do in CP/M changes the
file on your computer, and the program keeps no copy. You can protect the file in two ways:

- **Write-protect the disk.** Type `MOUNT dsk0:drive0 cpm22b23-56k.dsk WP`. The controller then
  refuses every write, and the file cannot change. Use this to look at the disk. CP/M does not
  know that the disk is write-protected. A program that tries to write can fail, so do not use
  `WP` when you want to save your work.
- **Copy the folder** when you want to write to the disk. The folder has all that the machine
  needs, and it boots from any place:

  ```
  $ cp -R examples/cpm my-cpm
  $ ./altairsim my-cpm/cpm22-buffered.toml
  ```

**Go back to the `A>` prompt before you quit or copy the image.** This BIOS does not write to
the disk when CP/M closes a file. It keeps a full track in memory, and it writes the track the
next time it reads the console. If you quit before that, the last write is lost. The disks
chapter tells you more about all of these.

## No disk? Start with a tape

To see a machine boot with no disk and no PROM, go to the tapes chapter and load Altair 4K BASIC
3.1. You enter the bootstrap by hand, from the listing that MITS printed in its manual.
