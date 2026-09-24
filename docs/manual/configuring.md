# The machine file

A machine file is **TOML**. It lists the boards in the backplane, the settings of each board,
and the commands to run after the power comes on. A machine file does nothing else.

This chapter is the full description of the format.

## What TOML is

**TOML** is a plain configuration format. You write it by hand, and you can read it again later
without a manual. These rules are enough to read every example in this chapter:

- **`key = value`** gives one setting on each line, for example `clock_hz = 2000000`.
- **Quotes are for text.** A string has quotes (`name = "cpm22"`). A number, `true` or `false`
  has no quotes (`size = 256`, `idle = true`).
- **A `[table]` appears one time.** `[machine]` is the machine, and `[console]` is your
  terminal.
- **A `[[table]]`, with double brackets, repeats.** Each `[[board]]` starts one more board.
- **A nested table such as `[board.unit.x]` belongs to the block above it**, by its name. The
  indentation in these examples only makes them easier to read.
- **`#` starts a comment** that continues to the end of the line. A comment that starts with
  **`#>`** is a *note*, which the program prints when it loads the file. See below.

That is all the syntax. The rest of this chapter tells you which keys go where.

## The first thing to know

**A machine file can do what you can type, and nothing more.** It cannot boot a disk, because
there is no `BOOT` command. It can *type `RUN FF00` for you*, which is what the operator did.

Every board setting in a machine file is a setting that you can make with `SET` at the prompt.
Every `SET` that you make at the prompt is a key that you can write in the file. **The
properties of a board are its TOML keys.** The board reference at the back of this manual lists
every property of every board.

## Nothing is ignored

**An unknown table or key is an error, and the program tells you which one.** The machine does
not load.

```
mine.toml: unknown [machine] key 'widget'
mine.toml: [[board]] cpu0: cpu0 has no property 'frobnicate'. Known: clock_hz idle achieved_hz
```

A setting with a typing mistake never looks as if it worked. You find the mistake when you load
the file, not later while you look for a fault in the machine.

## Notes for the operator: `#>`

An ordinary `#` comment is for the person who *reads the file*. A comment that starts with
**`#>`** is for the person who *loads it*. The program prints its text when the machine loads,
after the `loaded …` line and before the `startup` commands run. Use it to tell the operator how
to use the machine.

```toml
#> Boots CP/M 2.2 from drive A.
#> Type DIR at the A> prompt, and DIR B: for the blank second disk.

[machine]
name = "cpm22"
base = "default"
startup = ["RUN FF00"]
```

- **Each `#>` line prints as one line.** You can write as many as you want, together or in
  different places in the file. They print in the order of the file.
- **A `#>` with no text prints a blank line**, so that you can make short paragraphs.
- **A `#>` can follow a setting** on the same line: `name = "cpm22"  #> the buffered variant`.
- **Under `--mcp`, the notes go to stderr.** Stdout carries only the MCP messages there, but you
  still see the notes in the terminal.
- A note is still a comment. It **sets nothing**, and `CONFIG SAVE` does not write it. To keep a
  note, keep it in the file that you wrote by hand.

## The tables

| Table | What it is |
|---|---|
| `[machine]` | the name of the machine, its base and its startup commands. Three keys only |
| `[[board]]` | a board. One entry for each board |
| `[board.unit.<name>]` | one unit *on* the board above it, for example a serial channel or a tape deck |
| `[[board.region]]` | a memory region on a `memory` board |
| `[[board.drive]]` | a drive on a disk controller |
| `[console]` | **your terminal.** It is not a board. See below |
| `[display]` | **your video window.** It is not a board. See below |
| `[terminal]` | the built-in terminal window. The serial chapter describes it |

## `[machine]`: three keys

```toml
[machine]
name    = "cpm22"
base    = "default"
startup = ["RUN FF00"]
```

### `name`

The name of the machine. `SHOW MACHINE` prints it, and the title bar of the video window shows
it. `SET MACHINE name=` changes it at the prompt.

### `base`: start from a machine, and write what is different

```toml
base = "default"          # a built-in
base = "../cpm22/cpm22.toml"   # or a file
```

The program reads the value with **the same rule as the command line**. A value that contains a
`/` or ends in `.toml` is a file. Any other value is a built-in name. The machines chapter gives
the rule. A file path is relative to *this* file.

Two rules tell you where to put `base`:

- **The program reads `base` before every other key**, in any order in the file.
- **`base` must come before the first `[[board]]`.** A file cannot change a backplane and then
  start from a different one.

A `base` can itself have a `base`, **up to 8 levels deep**.

With `base`, a machine file contains **only what is different** from its base. You do not copy
every board, so a change to the base reaches every file that uses it.

### `startup`: the operator's commands

```toml
startup = ["RUN FF00"]
```

`startup` is a list of **ordinary monitor commands**. The program runs them in order after it
builds the machine, and it shows each one. This is the `startup>` line in the quick start.

**A path in a `startup` command is relative to the machine file**, the same as every other
relative path. A `startup` line and the same command that you type find the same file.

### What `[machine]` does not accept

```toml
[machine]
clock_hz = 2000000        # ERROR
sense    = 0x80           # ERROR
```

The program **rejects both, and it tells you where each one goes**:

```
mine.toml: clock_hz belongs to the CPU BOARD, not to [machine] --
  the crystal is on the board. Put it in the CPU's [[board]]:
      [[board]]
      type     = "8080"
      id       = "cpu0"
      clock_hz = 2000000
```

The crystal is on the processor board, and the sense switches are on the front panel. Neither
one is a property of the machine.

## `[[board]]`: four forms

**What a `[[board]]` entry does depends on two things.** Does it have a `type`? Did the base
already use its `id`?

| Write | What it does |
|---|---|
| `type` + a **new** `id` | **ADD** the board |
| `type` + an `id` **from the base** | **REPLACE** the board |
| **no** `type` + an `id` | **MODIFY** the board |
| `remove = true` + an `id` | **REMOVE** the board |

**Every `[[board]]` needs an `id`.** You use the `id` to name the board at the prompt, and a
later file uses it to name the board here.

### ADD: `type` + a new id

```toml
[[board]]
type = "virtc"
id   = "vi0"
```

The machine now has this board. **In a file with no `base`, this is the only form**, because
there is no board to modify, replace or remove.

### REPLACE: `type` + an id that the base already used

```toml
[[board]]
type = "2sio"
id   = "sio0"
port = 0x20
```

If the base had a board called `sio0`, the program removes it and adds a new `2sio` in its
place. **All the settings that the base made on that board are lost**, also the ones that you
did not write. You get the defaults of the type, and the settings that you write here.

Usually you do not want this. You want to modify the board:

### MODIFY: no `type`

```toml
[[board]]
id   = "cpu0"
clock_hz = 2000000
```

**When you leave out the `type`, you change the board that is already there.** All the settings
that the base made on `cpu0` stay. Only the clock changes.

### REMOVE: `remove = true`

```toml
[[board]]
id     = "acr0"
remove = true
```

The program removes the board from the backplane. Its ports are no longer decoded. No other
entry in the file can name it.

### The error that finds a copied block

**`type` + an id that *the same file* already declared is an error.** It is not a replace. In
one file, two boards with the same id are almost always a block that you copied and did not
rename. The file does not load, and the error tells you the id.

Across files, the rule is different. An id from your *base* is a board that you got from the
base, and you can replace it.

## Every other key on a `[[board]]` is a property

`type`, `id` and `remove` are the only keys that the file loader reads. **The loader gives every
other key to the board.**

```toml
[[board]]
type = "2sio"
id   = "sio0"
port = 0x10          # the 2sio knows what a port is. The config layer does not.
```

The loader knows nothing about ports, baud rates, sense switches or drives. It gives the key to
the board, and the board accepts the key or rejects it by name:

```
mine.toml: [[board]] cpu0: cpu0 has no property 'frobnicate'. Known: clock_hz idle achieved_hz
```

**The board reference at the back of this manual lists the keys of every board.** The boards
chapter tells you what each board is.

## `[board.unit.<name>]`: settings for one unit

Some boards have more than one independent part. An 88-2SIO is **two 6850 ACIAs**. Unit `a` and
unit `b` each have their own baud rate, their own interrupt setting and their own connection, so
each one has its own table:

```toml
[[board]]
type = "2sio"
id   = "sio0"
port = 0x10                    # the BOARD's property -- both chips live at this base

  [board.unit.a]
  baud    = 9600               # channel A's property
  connect = "console"

  [board.unit.b]
  baud    = 1200               # channel B is a different chip. It does not care.
  connect = "socket:2323"
```

A key in `[board.unit.a]` is the same key that `SET sio0:a baud=9600` sets at the prompt.

The board reference tells you which boards have units, and the keys of each unit.

## `[[board.region]]`: memory

A `memory` board is **a list of regions**. For this reason, one memory board can have 56K of RAM
and a boot PROM at the top of memory.

```toml
[[board]]
type = "memory"
id   = "mem0"

  [[board.region]]
  type = "ram"
  at   = 0x0000            # HEX -- it is an address
  size = "56K"             # DECIMAL -- it is a count

  [[board.region]]
  type  = "rom"
  at    = 0xFF00
  size  = 256
  mount = "turnmon.bin"    # relative to THIS FILE
```

| Key | |
|---|---|
| `type` | **required.** `ram` or `rom` |
| `at` | the address where the region starts. **Hex** |
| `size` | the size. **Decimal**. You can use the `K` and `M` suffixes |
| `mount` | a ROM image: a file path, or `builtin:<name>` |

A size with a suffix needs quotes, as in `size = "56K"`, because TOML does not accept `56K` as a
number. A plain number needs no quotes: `size = 256`.

### An empty socket

**A `rom` region with no `mount` is an empty socket.** It decodes nothing, so a read there gets
`FF`. That is what an S-100 bus gives when no board drives it. It is not zero, and it is not an
error. A real board with an empty PROM socket gives the same result.

## `[[board.drive]]`: disks

A disk controller has drives, and a drive holds a disk image.

```toml
[[board]]
id = "dsk0"

  [[board.drive]]
  unit     = 0             # DECIMAL -- it is a drive number
  mount    = "cpm.dsk"     # relative to THIS FILE
  readonly = false
```

| Key | |
|---|---|
| `unit` | the drive number. **Decimal** |
| `mount` | the image file |
| `readonly` | refuse every write at the controller, so that the file cannot change. The disks chapter tells you more. `writeprotect` is the same key. You can write either one |
| `media` | set the disk format, and do not detect it from the image |
| `create` | make an empty file if the file is not there, and then mount it. This is `MOUNT … CREATE` in the file |

The controller usually detects the format from the image. Use `media` when it cannot, for
example for an image with no header or with an unusual layout. `media` also sets the size of a
**blank** disk, because a blank disk matches no format. The disks chapter describes the formats
and `create`.

Without `create`, a `mount` that names a missing file is an **error, and the machine does not
load**.

## Numbers: hex on the bus, decimal for counts

The machine file uses the same number rule as the monitor. *The Monitor* gives the full rule.
**A value that the processor sees on the bus is hex. A value that never goes on the bus is
decimal.**

```toml
port  = 10        # 0x10 -- a port is on the wire, so it is HEX. This is SIXTEEN.
at    = 0xFF00    # an address
sense = 80        # 0x80
baud  = 9600      # a rate  -- DECIMAL
size  = 56        # a count -- DECIMAL
```

**`port = 10` is port sixteen.** This rule surprises people. A port is on the bus, so it is hex,
and every listing from 1976 put the 2SIO at 10. In your own files, you can make the base clear
with a marker:

- `0x10`, `$10` or `10h` for hex
- `0o20` or `20q` for octal
- `0b10000` for binary
- `#16` for decimal
- a `K` or `M` suffix, which is always a decimal count

To read and print in octal, set `[console] base = octal` (see below). This changes the base, not
the rule. When you `SET` a port, the program shows it in hex, for example `port=0x20`, whatever
form you typed.

## `[console]`: your terminal, which is not a board

```toml
[console]
stop      = 0x05      # the STOP key: back to the monitor. Ctrl-E (attn= also works)
base      = hex       # hex | octal -- how you read/write addresses, ports, bytes
upper     = false
strip7in  = false
strip7out = false
crlf      = false
echo      = false
bell      = true
bsdel     = "off"
```

`[console]` is **not a `[[board]]`**, and it is not in the backplane. It describes *the terminal
where you type*, which is on your desk, at the other end of the connection. The Altair knows
nothing about it.

| Key | |
|---|---|
| `stop` | the byte of the STOP key. **Hex.** The default is `05`, which is `Ctrl-E`. `attn` is the same key |
| `base` | `hex` or `octal`: how the monitor reads and prints addresses, ports and bytes. `octal` is split octal, as on the MITS front panel |
| `history` | how many lines the monitor's command history file keeps. The default is 50. `0` stops saving it |
| `log` | a file that gets a copy of the session. `off` stops it |
| `upper` | change input to upper case |
| `strip7in` | clear bit 7 of every byte that the guest receives |
| `strip7out` | clear bit 7 of every byte that the guest sends |
| `crlf` | change line endings |
| `echo` | show typed characters locally |
| `bell` | let the guest ring the bell of your terminal |
| `bsdel` | `off`, `bs` or `del`: what your Backspace key sends |

**These settings are the only part of the program that changes a byte.** They belong to the
console, because a person reads text there. **Every serial line is 8-bit clean.** No board has a
setting that clears bit 7, because a line can carry XMODEM, and a line that clears bit 7 cannot
carry a file. For example, to fix the garbled `MEMORY SIZ?` prompt of MITS BASIC, set
`strip7out` on the console. Do not set `data_bits = 7` on the board.

When you connect a board's unit to something other than the console, these settings no longer
apply to it. Examples are `CONNECT sio0:a socket:2323` and a real serial port. The other end
gets the bytes as the guest wrote them, with all eight bits. The serial chapter tells you why,
and what to set instead.

## `[display]`: your video window, which is not a board

```toml
[display]
focus = true
crt   = true
```

`[display]` is like `[console]`. It describes *the window on your screen*, not the board that
draws into it. A machine with two video boards still has one operator with one keyboard, so
these settings are in one place, not on each board. The settings apply to every video window of
the machine.

The size of a window is different. Each video board opens its own window, so its size is the
`width` property of that board. The boards chapter describes `width`.

| Key | |
|---|---|
| `focus` | whether the video window comes to the front and gets the keyboard when it opens. Default `false` |
| `keyboard` | whether the keys that you type in the window go to the machine's console: `console` (default) or `none` (display only). See below |
| `crt` | show the picture as on the original monitor, with a soft glow and a 4:3 shape, and not as sharp square pixels. Default `false`. See below |

With `focus = false`, the default, the terminal keeps the keyboard. The window opens behind your
other windows, and when the machine stops you can type at `altairsim>` at once. You can also
click in the window and type there. Your keys then go to the same console as the terminal's
keys.

With `focus = true`, the window comes to the front when it opens, and it keeps the keyboard when
the machine stops. Use this for a **Sol-20**, where the window *is* the console.

`keyboard` decides whether a video window is a keyboard at all. This is different from focus.
With `console`, the default, the keys that you type in the window go to the console. This is
correct for a **Sol-20**. Set `none` for a board that only shows a picture, such as a
**Dazzler**. The window still shows the picture and still comes to the front, but its keys do
**not** go to the console. They control a joystick, if the machine has a `d7a`. Only `Ctrl-E`
works in the window, to stop the machine and give you the monitor. For this reason, the keys
that you type in a Dazzler game window do not go to the CP/M prompt.

`crt` changes how the window shows the picture. It does not change what the machine draws. With
`crt = false`, the default, each pixel is a sharp square, scaled by a whole number. With `crt =
true`, the window looks like a monitor of the period. The picture is stretched to a 4:3 shape,
and the rows are softened into each other. A VDM-1 draws 512×208 and a VDB draws 640×240.
Neither picture was square, because the tube stretched it to 4:3. Type `SET DISPLAY crt=on` or
`crt=off` at the monitor to change it, and the open window changes at once. With `crt = true`, a
window with a `width` is that many pixels wide.

## A complete machine file

This small file has every part that a machine needs, and it loads. It has no `base`, so every
board is an ADD:

```toml
[machine]
name    = "tiny"

[[board]]
type = "fp"                # the front panel: the sense switches at port FF
id   = "fp0"
sense = 0x00

[[board]]
type     = "8080"          # the CPU is a board. The crystal is on it.
id       = "cpu0"
clock_hz = 0               # 0 = flat out. This is the default.

[[board]]
type = "2sio"              # the console board
id   = "sio0"
port = 10                  # HEX. Port SIXTEEN.

  [board.unit.a]
  baud    = 9600           # DECIMAL. Nine thousand six hundred.
  connect = "console"

[[board]]
type = "memory"
id   = "mem0"

  [[board.region]]
  type = "ram"
  at   = 0x0000
  size = "16K"

[console]
strip7out = true
```

## A file with a `base`: the kind that you will write

This is `examples/cpm/cpm22-buffered.toml` without its comments:

```toml
[machine]
name = "cpm22-buffered"
base = "default"
startup = ["RUN FF00"]

[console]
bsdel = "bs"

[[board]]
id = "dsk0"

  [[board.drive]]
  unit  = 0
  mount = "cpm22b23-56k.dsk"
```

The `default` machine already has everything that a 56K CP/M machine needs. It has a front
panel, an 8080, a 2SIO console, a floppy disk controller, 56K of RAM and the boot PROM at
`FF00`. This file adds only three things: the disk in drive 0, the `RUN FF00` command, and the
Backspace setting that CP/M expects.

The `[[board]]` has no `type`, because `dsk0` is already there, and the file modifies it. With
`type = "dcdd"`, the file would remove the base's controller and add a new one with the default
settings. The machine would still work, and you would not see the difference. Leave the `type`
out.

## Saving and loading at the prompt

```
altairsim> SET MACHINE name=mine
altairsim> CONFIG SAVE mine.toml
altairsim> CONFIG LOAD mine.toml
```

**`CONFIG SAVE` writes the machine that you are running now.** It writes every board and every
property, with every change that you made with `SET`. When you load the file, you get the same
machine. Give the machine a name with `SET MACHINE name=` before you save it. The machines
chapter tells you more about the name.

**`CONFIG LOAD` replaces the machine that you have.** It does the same as a machine file on the
command line. You cannot undo it, except with a file that you saved. It also **loads all of the
file or nothing**. The program builds the new machine first. If the file does not load, you keep
the machine that you had.

This is the fastest way to write a machine file. Build the machine at the prompt with `BOARDS
ADD` and `SET`, and save it. After that, edit the file down to the parts that you need, or give
it a `base` and delete the rest. The machines chapter shows these steps under "From an empty
backplane to a machine file".

The `startup` list is the one part of the file that is not a board. It holds the commands that
the machine runs when it loads, for example `MOUNT` a disk, `LOAD` a loader, and `RUN`. You can
build it at the prompt with `STARTUP`:

```
altairsim> STARTUP ADD MOUNT dsk0:drive0 "CP-M 2.2.dsk"
altairsim> STARTUP ADD RUN FF00
altairsim> STARTUP
  1  MOUNT dsk0:drive0 "CP-M 2.2.dsk"
  2  RUN FF00
```

`STARTUP ADD` adds a line as you typed it, with its quotes and spaces, because a startup entry
is a command line. `STARTUP REMOVE <n>` removes one line, and `STARTUP CLEAR` removes them all.
`CONFIG SAVE` writes the list as `startup = [...]`, so the file boots the machine in the way
that you tested at the prompt.
