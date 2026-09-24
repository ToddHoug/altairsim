# Disks

A disk on an Altair is three things:

- a **controller** board on the bus
- some **drives** on that controller
- a **disk image** in one of those drives

The three are separate, and this manual keeps them separate, because the machine did.

This chapter is about the **image** and the **drive that it goes in**, and about `MOUNT`, which
puts one in the other. `MOUNT` works in the same way for every controller. The controllers
themselves are boards, and the Boards chapter has a section for each one. This chapter uses the
MITS hard-sector controllers, `dcdd` and `mds`, for its examples, because the machines in the
package boot from them.

The controller is a board, and it goes in the machine file. The drives are part of the
controller. A MITS 88-DCDD addresses up to sixteen drives, whether or not you have sixteen. The
disk goes in a drive. You can name it in the machine file, or `MOUNT` it at the prompt. Both do
the same thing.

> **A disk is not a cassette.** A machine file *can* name the floppy in drive 0, and the CP/M
> example does. A drive has a `mount` key, but a tape recorder has no such key. A floppy drive
> is connected to its controller, and the guest can tell what is in it. **A cassette recorder
> has no motor control on the board.** Nothing in the machine can start the tape, sense it, or
> know that it is there. For this reason, a person puts the tape in and presses PLAY, with
> `MOUNT`. See the tapes chapter.

## The controllers in this chapter

| Board | What it is | Drives | Ports |
|---|---|---|---|
| `dcdd` | **MITS 88-DCDD**: the 8″ hard-sector floppy controller. CP/M booted from it | up to 16 | 08, 09, 0A |
| `mds` | **88-MDS**: the 5¼″ minidisk. Smaller and slower, with four drives | 4 | 08, 09, 0A |

**The two boards use the same ports**, so you cannot put both in one machine. The Boards chapter
tells you more.

The drives are units on the board: `drive0`, `drive1` and so on, up to the limit of the board.

## Putting a disk in: `MOUNT`

```
MOUNT <id>[:<unit>] <file> [WP] [CREATE]
```

```
altairsim> MOUNT dsk0:drive1 games.dsk
```

This puts a floppy in drive 1 of the controller called `dsk0`. The drive was empty before, and
now it has a disk.

**The file must already exist.** A name that does not exist is a typing mistake, not a new disk.
The program tells you so, and does not make a disk that you did not want:

```
altairsim> MOUNT dsk0:drive1 my-scratch.dsk
dsk0: 'my-scratch.dsk': no such file
dsk0: to make a blank one, add CREATE: MOUNT dsk0:drive1 my-scratch.dsk CREATE
```

`CREATE` makes a disk on purpose. "Making a scratch disk" below tells you what to do with it.

### An `.imd` file is converted when you mount it

An `.imd` file is an **ImageDisk** file. Much archived 8″ and 5¼″ software is in this format.
When you `MOUNT` a file whose name ends in `.imd`, the program does not mount the `.imd` itself.
It **reads the file, converts it to a raw sector image, writes that image beside it as
`foo.dsk`, and mounts the `.dsk`**. A controller reads raw sectors, not the track records of
ImageDisk. The program reports the conversion, so that you can check that it is the disk you
expected:

```
altairsim> MOUNT dsk0:drive0 cpm.imd
dsk0: converted cpm.imd -> cpm.dsk
dsk0:   IMD: CP/M 2.2 system disk
dsk0:   337568 bytes
dsk0:drive0: mounted cpm.dsk
```

**It never overwrites a `.dsk` that is already there**, as with `CREATE`. If `cpm.dsk` already
exists, the program tells you to remove it or to mount it directly. For this reason, a second
mount cannot lose the changes that you made to the converted disk. After the conversion, the
`.dsk` is the disk, and the `.imd` is only where it came from.

### Write protection: `WP`

`WP` **write-protects the disk**, as the write-protect notch of a real diskette did. The guest
can read the disk. The controller refuses every write, and no write reaches the file on your
computer.

```
altairsim> MOUNT dsk0:drive2 golden-master.dsk WP
```

`RO` is also accepted, and it means the same thing. `RO` is the word for a ROM socket. For a
floppy disk, `WP` is the correct word.

**The guest is not told, and it cannot be.** See "Write protection is not an error message"
below. Use `WP` for a disk that you only want to read.

A write-protected disk shows this in every list, so that you do not have to remember which disk
it was:

```
altairsim> SHOW MOUNTS
  UNIT         KIND  HOLDS
  dsk0:drive2  disk  golden-master.dsk  (write-protected)
```

If **your computer** does not let the program write the file, because of its permissions, the
disk still mounts. It is write-protected, and the list says that you did not ask for this:

```
  dsk0:drive2  disk  master.dsk  (write-protected -- THE HOST WON'T LET US WRITE IT; you did not ask for this)
```

Look for this message. You did not ask for the protection, so CP/M will fail every write to a
disk that you think is writable.

### Taking a disk out: `UNMOUNT`

```
altairsim> UNMOUNT dsk0:drive1
```

The drive is empty again. The guest sees a drive with no disk in it, which a real drive could
also be.

### Names, and what you can leave out

Board names are **not case-sensitive**. `dsk0`, `DSK0` and `Dsk0` are the same board.

You can also leave out a part of the name when it adds no information:

- the **number at the end**, when the machine has only one board of that type. With one floppy
  controller, `dsk` finds `dsk0`.
- the **unit**, when the board has only one thing that you can mount into. `MOUNT ACR tape.bin`
  needs no unit, because a cassette recorder has one place for a tape.

A floppy controller has several drives, four by default and up to sixteen on an 88-DCDD. The
drive that you mean is real information, so you must give it. **When there is more than one, you
must name the one that you mean.**

## Putting a disk in the machine file

`MOUNT` at the prompt is for the disk that you are using *now*. A disk that belongs to a machine
goes in the machine file. The examples in the package do this:

```toml
[[board]]
id = "dsk0"                    # no `type`: the controller is already there

  [[board.drive]]
  unit  = 0
  mount = "cpm22b23-56k.dsk"   # relative to THIS FILE
  # readonly = true            # refuse every write; your file cannot change
  # create   = true            # make it blank if it isn't there -- CREATE, in a file
  # media    = "8in"           # what a blank one is; see "The geometry comes from the file size"
```

`writeprotect = true` means the same as `readonly = true`. It is there because the rest of the
program says "write-protect": `MOUNT` takes `WP`, and `SHOW MOUNTS` prints `(write-protected)`.
`CONFIG SAVE` writes `readonly`.

The file form and the prompt form do the same thing. Only the time is different. The file gives
the drive as the machine starts, and the prompt is you, changing it later.

The path names the file **beside the machine file**, with no folder. For this reason, you can
copy the whole folder to another place, and it still boots. A relative path, in a machine file
or typed at the prompt, starts from the machine's folder. The machines chapter gives the full
rule.

## Getting a disk over the network: TNFS

A disk image does not have to be a file on your computer. You can point `MOUNT` at a **TNFS
server**, the network file system of the FujiNet project. The program then gets the disk over
the network:

```
altairsim> MOUNT dsk0:drive0 tnfs://fileserver/cpm/games.dsk
altairsim> MOUNT dsk0:drive1 tnfs://fileserver:16384/scratch.dsk WP
```

The form is `tnfs://<host>[:<port>]/<path>`. The port is **16384** by default, which is the TNFS
port. The path is where the image is on the server. A machine file can name one in the same way,
with `mount = "tnfs://fileserver/cpm/games.dsk"` on the drive. A `tnfs://` name includes its
server, so it does **not** start from the machine's folder as a plain file name does. It means
the same thing wherever you write it.

After that, it is like any other disk. The program gets the whole image **one time, when you
mount it**. From then on, it works like a local disk. The guest reads and writes it, the program
finds the format from its size (below), and `SHOW MOUNTS` lists it. The program writes your
changes back to the server when you `UNMOUNT` it, or when the guest flushes. `WP` write-protects
it as it does a local disk. If the **server** does not let the program write, because the file
is read-only there, the disk still mounts, write-protected, and says that you did not ask for
this. This is the same as for a file on your computer.

A network can go away in the middle of a session. If the server stops accepting writes while you
use the disk, **the program tells you**. It prints a line that says it can no longer save
changes to that disk, and that it keeps them in memory only. The guest continues to run, and the
program continues to try. When the server comes back, the program says so, and saves the
changes. Until then, the changes are not safe. If you quit, or the server never comes back, the
writes after the message are lost.

TNFS carries only **single-file images**: a floppy, a minidisk, an 8 MB `fdc8mb` disk and a
cassette tape. These are small enough to keep in memory. The larger card images of the
CompactFlash and SD boards keep their size in a second file (`.img` with `.geo`), and you cannot
mount them over TNFS. Keep them on your computer.

## The geometry comes from the file size

You do not tell `altairsim` what kind of disk you mounted. It **looks at the number of bytes in
the file** and finds the format:

| Format | Tracks | Sectors | Bytes/sector | File size |
|---|---|---|---|---|
| `8in` | 77 | 32 | 137 | **337,568** |
| `minidisk` | 35 | 16 | 137 | **76,720** |
| `fdc8mb` | 2048 | 32 | 137 | **8,978,432** |

A file of 337,568 bytes is an 8″ floppy. It cannot be anything else. For this reason, the quick
start does not name a format.

The `8in` and `fdc8mb` formats belong to the `dcdd`. `minidisk` is the only format of the `mds`.
The program chooses only from the formats that the board can take.

**The size in the table is the size of the disk. Your file can be a little bigger.** Almost
every image in circulation was sent by XMODEM at some time, and XMODEM adds bytes to fill a
128-byte block. The CP/M disk in this package is 337,664 bytes, not 337,568, and the minidisk
images that you find are 76,800 bytes, not 76,720. The program allows for this, so those files
are 8″ floppies and minidisks like any other. Do not look for the exact number on your own disk.
It is the size of the format, not a checksum.

**The package has 8″ floppy images and one Datakeeper hard-disk image.** You supply any
`minidisk` or `fdc8mb` image. The `mds` board and the 8 MB format both work, but the package has
no disk for them.

### A file that matches no format

A blank disk is a file of zero bytes, and zero is not in the table. It mounts anyway, because
otherwise you could not make a disk:

**A file whose size matches no format is mounted UNFORMATTED, at the largest format that the
board can use**: `fdc8mb` on a `dcdd`, and `minidisk` on an `mds`. The guest's own format
program fills it in. The file **grows as the guest writes it**, up to as far as the head can
step. For this reason, `CREATE` and a period format program together make a disk.

A blank disk starts as big as the controller can use. If you want a blank 8″ floppy and not a
blank 8 MB disk, for example because you will format it with a program that expects 77 tracks,
set `media`:

```toml
  [[board.drive]]
  unit   = 1
  mount  = "scratch.dsk"
  create = true
  media  = "8in"        # not "as far as a dcdd can step"
```

`media` is a machine-file key on the drive. You cannot set it at the prompt. Also use it for an
image that is cut short, or for a format that you make up, where the size cannot decide.

## How an 8 MB disk works

`fdc8mb` is a disk with 2048 tracks on a controller that MITS designed for 77 tracks. It works
with **the stock board and the stock PROM**, because the controller cannot tell the difference.
The controller steps the head and moves bytes. It never asks how big the disk is. The BIOS,
which is software, knows *where track 1500 is*.

For this reason, **the format and the spindle belong to each drive, not to the controller**. One
board can have drives of different sizes, and that is correct. The period 8 MB CP/M BIOSes
expect an 8 MB disk on A: and B:, and ordinary 77-track floppies on C: and D:, so that you can
`PIP` between them.

**You supply both images.** The package has neither, so the lines below show the form of the
command, not files that you have:

```
altairsim> MOUNT dsk0:drive0 big.dsk
altairsim> MOUNT dsk0:drive2 floppy.dsk
altairsim> SHOW dsk0
```

## THE TRACK BUFFER TRAP

If you do not know this, you can lose work. It is not caused by the simulator. **A BIOS can work
this way**, and the CP/M in this package does.

**A track-buffering BIOS does not write to the controller when CP/M closes a file.**

`BIOS WRITE` copies your data into a **track buffer in memory**, which holds 32 sectors of 137
bytes, one whole track. It marks the buffer as changed, and returns. Nothing has gone to the
disk, and nothing goes to the disk until something calls the flush routine. **`CONIN`**, the
BIOS routine that reads the console, calls the flush. **A read of the console writes the buffer
to the disk.**

The BIOS does this for speed. A track write needs one seek and one turn of the disk, not
thirty-two. When the machine waits for a person to type, it has time to spare.

### Not every CP/M does this

Buffering is not part of CP/M, and it is not part of the controller. The author of the **BIOS**
decided it, and the BIOS is the part of CP/M that every owner rewrote. Period Altair BIOSes work
in one of two ways:

- **Track-buffered.** Writes stay in memory, and go to the disk when the BIOS reads the console.
  The CP/M in this package works this way.
- **Write-through.** Every `BIOS WRITE` puts a sector on the disk before it returns. When you
  get your prompt back, your file is already on the disk. Some of these BIOSes also use `CONIN`,
  but only to *unload the head*. This protects the drive, and does not change any data.

You cannot tell which kind you have from the `A>` prompt. A disk image from another person has
the BIOS that its author wrote. For this reason, always obey the rule below. On a write-through
BIOS, it costs you nothing. On a buffered BIOS, it decides whether your file is on the disk.

> **Do not `UNMOUNT`, copy or stop the program right after a file operation.**
> **Go back to the `A>` prompt first.**

At the `A>` prompt, CP/M reads the console. The console read writes the buffer to the disk, if
there is a buffer, and the directory update goes to the disk. When you see `A>`, the disk on
your computer is correct, with either kind of BIOS. Save the file, wait for the prompt, and
*then* do the next thing.

`Ctrl-E` to the monitor from the `A>` prompt is safe. `Ctrl-E` to the monitor right after `ED`
says it wrote your source file is **not** safe. On the CP/M in this package, the file is then
not on the disk.

## The disk is real, and there is no undo

The program mounts a disk **read/write**, and every write goes to the file on your computer at
once. There is no journal and no way back. `SNAPSHOT` does not help. It saves the state of the
machine, not the files on your computer, so the disk image is not in it. CP/M cannot save your
work to a disk that it cannot write, so read/write is the only useful default. The cost is that
one wrong `ERA` can destroy the example disk.

**Copy the folder before you experiment.** The folder has everything the machine needs, and it
boots from any place.

Use `WP` when you want the guest to read the disk and not change it.

## Write protection is not an error message

`WP` protects **your file**. It is not a message to the guest. It is easy to think otherwise, so
this section explains it.

**The controller cannot report "write protected".** The status byte of the 88-DCDD has seven
bits: write circuit wants a byte, head movement OK, head loaded, drive enabled, interrupts
enabled, on track 0, and new read data. None of them means *the notch is covered*. A program
that reads the byte cannot tell a protected disk from an ordinary one.

**CP/M also has no way to report it.** A CP/M 2.2 BIOS write returns `0` for success and `1` for
an error that it cannot recover from. There are no other values, and no "protected" code. For
this reason, a real hardware write failure shows as `Bdos Err On A: Bad Sector`. That is the
only thing that CP/M can hear from the BIOS.

The message that people remember, `Bdos Err On A: R/O`, **comes from a different mechanism**. It
is CP/M's own software read-only flag, which `STAT A:=R/O` sets and a warm boot clears. The flag
is in CP/M's memory, not on the disk and not in the controller. A write-protected image never
causes it.

For this reason, CP/M cannot handle a write to a `WP` disk in a controlled way. **Do not mount a
disk `WP` and expect CP/M to handle it well.** Use `WP` for a disk that you are going to read.
For a disk that the guest can write and that you can throw away, copy the folder.

## Making a scratch disk

There are two steps, and the guest does the second one. **`CREATE` gives you the blank medium,
and a format program makes it a disk.**

**Step 1, at the monitor.** `CREATE` makes the file, and puts it in the drive:

```
altairsim> MOUNT dsk0:drive1 my-scratch.dsk CREATE
dsk0:drive1: created my-scratch.dsk (empty)
dsk0:drive1: mounted my-scratch.dsk
```

The file has zero bytes, and it is mounted. It is an unformatted disk, and CP/M will refuse to
read it. That is correct for a disk that nobody has formatted.

**Step 2, in CP/M.** Boot, and format the disk as people did then. The CP/M disk in this package
has **`AFORMAT`**, the Altair Disk Format Utility by Martin Eberhard. It formats an image as it
formats a disk:

```
A>AFORMAT B:

Put disk in B
Ready (Y/N)?Y
Formatting an 8" Disk............................................................................
Format another (Y/N)?
```

Each dot is a track. When the dots stop, the file on your computer is a formatted 8″ floppy, and
`B:` is a disk that CP/M can `DIR`, `PIP` to and put files on.

If you want a blank **8″ floppy**, not a blank disk as large as the controller can step, set
`media = "8in"` on the drive. See "The geometry comes from the file size" above. `AFORMAT`
formats either one, but a period program that expects 77 tracks works better with the 77-track
disk.

**`CREATE` works only on a controller that can grow its disk.** The `dcdd` and `mds` mount a
blank disk at the largest format that they can use, and the guest's format program fills it in.
A **fixed-geometry** controller cannot do this. The 88-HDSK "Datakeeper" hard disk knows the
exact size of its platter, so it cannot make a blank one. It refuses `CREATE` with this message:

```
h0: 88-HDSK has a fixed geometry and cannot make a blank platter -- supply an existing
    4988928-byte image (406 cyl x 2 sides x 24 sectors x 256)
```

The program removes the empty file. For this disk, you supply a full-size image, and there is no
scratch-disk step.

With either controller, go back to `A>` before you look at the file.

## Looking at what is in the machine

`SHOW` on the controller lists its drives and the disk in each one:

```
altairsim> SHOW dsk0
```

`BOARDS` shows the backplane: every board and the addresses that it decodes:

```
altairsim> BOARDS
```

These two commands answer most questions about why a machine does not boot. The most common
answer is that the disk is in the wrong drive.
