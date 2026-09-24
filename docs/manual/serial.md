# Serial ports, sockets and telnet

The Altair had no screen. It had a serial board, and you connected a device to it: a Teletype, a
video terminal, a modem or a paper-tape reader. The board did not know which. It moved
characters.

`altairsim` works in the same way. **Every board that moves characters has one or more UNITS,
and you can CONNECT each unit to an ENDPOINT.** The unit is the connector on the back of the
board. The endpoint is what you connected to it.

```
altairsim> CONNECT sio0:b socket:2323
altairsim> DISCONNECT sio0:b
```

That is all the interface. The endpoints are the important part, and the list is short enough to
give in full.

## The endpoints

This is the complete list. There are no others.

| Endpoint | Is |
|---|---|
| `console` | the host terminal — your keyboard and your screen. |
| `null` | nowhere. Writes vanish. Reads never come. |
| `loopback` | itself. What the guest writes comes straight back as a read. |
| `socket:PORT` | **LISTENS** on that TCP port, as a raw pipe. Add `?banner` to greet each caller (see *The connect banner*, below). |
| `socket:HOST:PORT` | **CALLS OUT** to that host and that port, as a raw pipe. |
| `telnet:PORT` | **LISTENS** like `socket:PORT`, but speaks the **Telnet protocol**, so that a `telnet` client works well: no double echo, one key at a time. Use it when a **person** connects. It greets each caller. `?banner=off` stops that. |
| `telnet:HOST:PORT` | **CALLS OUT** like `socket:HOST:PORT`, as a telnet client. |
| `serial:DEVICE` | a real serial port on this host. |
| `in:PATH` | a host file, read-only — a **paper-tape reader**. The file's bytes feed the board. |
| `out:PATH` | a host file — a **paper-tape punch**. Whatever the board sends is written to it. |
| `in:PATH,out:PATH` | both at once on one line: a reader and a punch, two files, two positions. |
| `terminal` | a terminal in a window that the simulator draws itself: a built-in VT100, ADM-3A, VT52 or H19. See *A terminal in its own window*, below. |
| `printer:QUEUE` | a real print queue on this host, write-only. Buffers the bytes into a job and prints it. Present only where the build found a host print system. |
| `scripted` | a terminal with a program in place of a person. No tty needs to exist. The MCP tools and the test suite type into it. You will probably not use it yourself. |

You can **tap** any of these. Add `|FILE` to log the line to a hex file as it runs, or add
`|socket:PORT` to **mirror** it live. With a mirror, a second person can use `telnet` to watch
the session, and can type on the line to take over. Both are changes to an endpoint, not
endpoints of their own. See *Tapping a line to a log file* and *Mirroring a line so a person can
watch and take over*, below.

### `null` is not an error

A unit that is not connected is `null`, and **that is a correct state, not a fault.** A 6850
with no cable has an empty transmit register, and it is always ready. A program that writes to
it runs correctly, and nobody receives the output. Reads never complete, because nothing sends.

A real board with no cable does the same. A machine with a second serial port that has nothing
connected to it is not broken. `null` is the missing cable, and the guest sees what it saw in
1977.

### The colon makes the difference

`socket:2323` **listens.** `socket:localhost:2323` **calls out.** The only difference is the
colon. Terminal programs use the same rule: a port alone is a port that you own, and a host with
a port is a place that you go to. Nothing else about the endpoint changes.

## Using telnet to connect to the guest

Connect a unit to a listening `telnet:` port, and the guest has a serial port with a terminal on
it. The guest cannot tell that the terminal is your telnet client, in another program.

```
altairsim> CONNECT sio0:b telnet:2323
altairsim> RUN
```

In another terminal on your computer, type:

```
$ telnet localhost 2323
Connected to AltairSim X.Y.Z (sio0:b) on port 2323
```

The guest now talks to that window. Your first terminal still has the monitor and `Ctrl-E`. In
this way, you can give a machine two terminals, or run a program that needs a console other than
the one that you use.

### `socket:` or `telnet:`

Both listen on a port, or call out to `HOST:PORT`. Both raise carrier when a caller connects,
and drop it when the caller leaves. The guest cannot tell them apart. They differ in what they
send to the far end:

| | `socket:` | `telnet:` |
|---|---|---|
| Who is at the far end | another program or another machine | a **person** with `telnet` or `nc` |
| On the wire | the guest's bytes and nothing else | Telnet: it negotiates echo and one-key-at-a-time with the client |
| What the person sees | each key twice (their terminal echoes it, then the guest does), and Enter sent as a whole line | one echo, from the guest, and each key sent as it is pressed |
| Greeting | none, unless you add `?banner` | one line naming the machine, the board line and the port; `?banner=off` stops it |

Use `telnet:` when a person types to the guest. Use `socket:` when a program is at the far end:
one `altairsim` connected to another, a file transfer, a tool that uses raw bytes, or a line
that you mirror.

A raw `socket:` shows its problem as soon as a person uses it. A standard `telnet` client, with
its default settings, shows each key locally, and the guest also echoes it, so each character
shows two times. Enter arrives as a whole line, with its carriage return changed to a line feed.
You cannot fix this from the keyboard. The two ends must agree on the settings, and the Telnet
protocol does that.

### The connect banner

A `telnet:PORT` line greets each person who connects, before the guest sends anything. This is
the `Connected to ...` line above. It gives the build, the board line that you reached and the
port, so that you know that you have the correct machine and line. Only the caller sees it. The
guest does not, and nothing goes to the board. To turn it off, add `?banner=off`:

```
altairsim> CONNECT sio0:b telnet:2323?banner=off
```

A `socket:PORT` line has no banner, because the far end is often another machine, and it would
read the banner as data. If a person calls a raw socket and you want the greeting, add
`?banner`: `CONNECT sio0:b socket:2323?banner`. The banner is only for a port that listens. A
line that calls out (`socket:HOST:PORT`, `telnet:HOST:PORT`) is the caller, so the program
refuses `?banner` there.

## A terminal in its own window

The telnet method works, but it needs things that you do not control. A telnet client must be
installed, and you must point it at the correct port. It also changes any byte that it thinks is
a telnet command. That breaks cursor sequences and file transfers, because on a serial line
every byte is data. Also, a modern terminal emulates a modern terminal, not the ADM-3A or VT52
that the 1970s software on the disk expects.

`terminal` avoids all of that. It is a terminal that the simulator draws itself, in its own
window:

```
altairsim> CONNECT sio0:a terminal
altairsim> RUN
```

The guest's console is now that window. The monitor and `Ctrl-E` stay in the terminal that you
started from. There is nothing to install and nothing to connect. The window opens when you
connect the line, on every platform. The terminal is part of the simulator, so its emulation is
the one that you select, and it answers the reports that a period program asks for, such as
`ESC[6n`.

`terminal` alone is a VT100 in an 80×24 window. Give options after a `?`, joined by `&`, as for
any other endpoint:

```
altairsim> CONNECT sio0:a terminal?emulation=adm3a
altairsim> CONNECT sio0:a terminal?emulation=vt52&size=80x24
altairsim> CONNECT sio0:a "terminal?emulation=h19&size=132x24"
```

- `emulation` is one of `vt100` (the default, and `ansi` is the same), `adm3a` (the Lear Siegler
  ADM-3A, the classic CP/M terminal), `vt52`, or `h19` (the Heath/Zenith H19, a VT52 with an
  ANSI mode added).
- `size` is *columns*×*rows*, and it is `80x24` by default.
- `phosphor` is the color of the tube: `green` (the default) or `amber`.
- `width` is the width of the window when it opens, in pixels, for example `width=1100`. Without
  it, the window is about half as wide as the screen.

The text uses the real **DEC VT220** character set. It goes well with the period look,
`[display] crt = true` (see the configuring chapter). With that look, a `width` opens the window
at that exact size.

```
altairsim> CONNECT sio0:a terminal?emulation=vt100&phosphor=amber&width=1100
```

The built-in terminal has the same settings as the console for changing bytes: `strip7out`,
`upper`, a CR/LF option and the others, under `[terminal]`. They are important for a period
monitor that sets bit 7. See *The built-in terminal has these too*, later in this chapter.

## Calling out

```
altairsim> CONNECT sio0:b socket:bbs.example.com:23
```

The guest dials. The software in the machine sees a connected modem, and it can run a period
terminal program over it.

## A real serial port

```
altairsim> CONNECT sio0:b serial:/dev/tty.usbserial-A600K1XY
altairsim> CONNECT sio0:b serial:COM3
```

The second form is for Windows. The bytes go out of a real UART, down a real cable, to the
device at the other end.

**If you give a wrong device name, the program lists the ports that are on your computer.** It
does not only say "cannot open". A cable can appear under a name that is one character different
from what you expected, and the list shows you the correct name.

### What the board does to the line

The program opens the host port at 9600 8N1, and then **the board sets it up again at once**,
because only the board knows what frame it carries. Where the board gets the settings depends on
the board, as on the real hardware:

- On an **88-SIO** or an **88-ACR**, the word format is set with **jumpers**. The board's
  `baud`, `data_bits`, `stop_bits` and `parity` properties become the frame on the line.
- On an **88-2SIO**, the board in the example above, the baud rate is a jumper (`baud`), but the
  **word format is a register that the guest writes**. The 6850 has no data-bits, stop-bits or
  parity property, and it must not have one. A property would be a second place to set the same
  thing, and the two would disagree as soon as the software wrote to the chip. For this reason,
  the frame on the line is what the **guest** last set. A guest that selects 7E1 sets the cable
  to 7E1.

To send 300 baud, 7 bits and even parity out of that connector, you do not set it on the
connector. You set it on the **board**, where an operator in 1975 set it, with a jumper. The
modem control lines (DCD, CTS and RTS) are connected through.

### Give the machine its real crystal before you transfer a file

```
altairsim> SET cpu0 clock_hz=2000000
```

When the line leaves the machine, the guest talks to something that keeps real time. The guest
does not. It counts instructions, so at full speed, its "three-second" timeout passes in a few
milliseconds, and it decides that your sender is dead. Full speed is correct for a machine that
talks only to itself. **A machine that talks to you needs the crystal.** The troubleshooting
chapter tells you more.

## A paper-tape reader and punch: `in:` and `out:`

A **paper-tape station** was connected to a serial or parallel line. `altairsim` makes one from
two host files. The keyword gives the direction:

```
altairsim> CONNECT lpt0:prn out:printout.txt          # a punch: capture what the board sends
altairsim> CONNECT 4pio0:ja in:reader.tap             # a reader: feed a file to the board
altairsim> CONNECT 4pio0:ja in:reader.tap,out:punch.tap   # both, on one bidirectional line
```

`in:` is a **reader**, a *source* of bytes. It reads the file from the start, and gives the
bytes to the board one at a time. When the file ends, the line goes **quiet**, with no error and
no end-of-file byte, as a reader with no more tape stops. If the file does not exist, `CONNECT`
refuses, and names the path.

`out:` is a **punch**, a *sink* for bytes. The board's output is written to the file. The punch
does **not** truncate the file. It writes from the start and continues past the old end. For
this reason, a short run into a longer old file leaves the end of the old file there, as new
tape wound onto a reel that still had some tape on it. If the file does not exist, the punch
makes it.

`in:` and `out:` are **separate files, each with its own position**. The combined form is two
independent heads on one line. A read of the tape cannot change what the punch wrote, and the
other way around. Both are **8-bit clean**: the bytes on the line are the bytes in the file,
with the control codes. Nothing changes them. A relative path starts from the machine's folder,
in a machine file or typed at the prompt.

### The 88-HSR: a reader with a speed

A real paper-tape reader has a speed, and a program that times its input needs it. To pace an
`in:` reader, add `?cps=N` (characters per second) or `?baud=N` (a line rate, at 10 bits per
character):

```
altairsim> CONNECT 4pio0:ja in:tape.tap?cps=300       # the 88-HSR high-speed reader
altairsim> CONNECT 4pio0:ja in:tape.tap?cps=30        # the slow reader
```

`in:tape.tap?cps=300` **is** the MITS 88-HSR. Give only one of `cps` or `baud`, with a positive
value. With neither, the reader runs as fast as possible. The punch takes no options. It writes
at the speed of the line.

## Printing to a real printer

When your build has host printing, a line can go to a real print queue instead of a file:

```
altairsim> CONNECT lpt0:prn printer:linewriter
```

The macOS and Linux builds have host printing. The **Windows** builds do not have it yet. On
Windows, `printer:` is not available, and printing goes to a file (`out:`) or a socket
(`socket:`) instead. To see what your build has, connect to `printer:` with no name. It lists
the queues that it can reach, or tells you that this build has no host printing.

`printer:` is a **write-only** sink like `out:`, and like `out:` it is not only for printers.
Any line can use it, not only the 88-C700 (see the boards chapter). The difference is what
happens to the bytes. They go into a buffer, and then to the host print system as one **job**.

**When does a job end?** A printer has no "done" signal. A program prints and then stops. For
this reason, `altairsim` decides where a job ends, and you can change this in the endpoint:

| Option | Means | Default |
|---|---|---|
| `?idle=N` | end the job after **N seconds** with nothing more printed. `0` = never. | `5` |
| `?onff` | also end the job on a **form feed** (the page-eject character). | off |
| `?max=N` | end the job at **N bytes**, so a runaway program cannot fill memory. | a large number |

```
altairsim> CONNECT lpt0:prn printer:linewriter?idle=15
altairsim> CONNECT lpt0:prn printer:linewriter?onff
altairsim> CONNECT lpt0:prn "printer:Generic / Text Only?idle=0&onff"
```

Write an option with no value (`?onff`) to turn it on. You do not need `=1`. The options
combine, and the first one that applies ends the job. **An empty buffer never prints**, so a
form feed and then silence does not print a blank page. A job also prints when you `DISCONNECT`
the line, load another machine or quit, so nothing that you printed is lost.

The queue must be one that the host passes through **unchanged** (a *raw* queue). A printer
control language is not text, and a normal queue would try to change it. You make that queue one
time, in the printer settings of your operating system, not in `altairsim`. If a queue name
contains spaces, put quotes around the whole endpoint, as above.

Like `out:`, a printer line is **8-bit clean**. The printer gets the bytes that the program
sent.

## Tapping a line to a log file

To see what a guest and the far end send to each other, you need to *see the bytes*. Add
**`|FILE`** to any endpoint, and `altairsim` writes every byte that crosses the line, in both
directions, to a text file as it runs. It writes hex and ASCII, with times. The guest cannot
tell that the tap is there, and the tap never changes a byte. For this reason, you can use it on
a binary transfer as well as on a terminal.

```
altairsim> CONNECT sio0:b socket:2323|bbs.hex
```

Use telnet to connect, use the guest, and `bbs.hex` fills with the conversation:

```
# altairsim capture  socket:2323  2026-07-29 14:03:11  fmt=dump
+0.001000  TX 0000  41 54 5A 0D                                       ATZ.
+0.048213  RX 0000  0D 0A 4F 4B 0D 0A                                 ..OK..
+9.100000  [DCD^]
+45.30000  [DTR_]
```

- `TX` is what the guest sent, and `RX` is what came back.
- The offset counts the bytes in a burst.
- The right-hand column is the ASCII, with a `.` for a character that cannot be printed.
- A line in `[...]` is a change on a **modem control line**, such as carrier, DTR or RTS. `^` is
  a rising edge, and `_` is a falling edge, so you can see the far end answer and hang up.

The program empties the file each time that you connect. Each capture is a new trace. The
program keeps the whole tap: `SHOW` prints it, and `CONFIG SAVE` writes it. For this reason, a
machine file can have a line that is always traced.

### Three layouts, and some options

The tap takes options with the same `?key=value` form as the other endpoints, after the file
name:

```
altairsim> CONNECT sio0:b socket:2323|bbs.hex?fmt=cols
altairsim> CONNECT sio0:b in:reader.tap?cps=300|trace.log?fmt=jsonl
```

- **`fmt=dump`** (the default) is the layout above. It has one hex row on each line, in time
  order. It is easy to read, and easy to search with `grep`.
- **`fmt=cols`** puts what the guest sent on the **left**, and what it received on the
  **right**. A request and its reply read down the page like a transcript.
- **`fmt=jsonl`** writes one JSON record for each transfer. Use it to give the trace to another
  program, to compare two runs, or to import it into a spreadsheet.

The other options are all optional:

- `ts=elapsed` (the default, seconds since the first byte), `ts=wall` (the host clock), or
  `ts=none`
- `width=N`: the bytes in each hex row (default 16)
- `gap=MS`: how long a quiet line waits before it writes a partial row (default 200 ms)
- `pins=off`: leave out the changes on the modem control lines

The second example taps a **paced paper-tape reader**. The tap works with any endpoint, and with
that endpoint's options.

## Mirroring a line so a person can watch and take over

A tap writes to a file. A **mirror** writes to a *socket*, in both directions. Add
`|socket:PORT` to any endpoint, and a second person can type `telnet localhost PORT` to watch
the session of the machine, with every character that the guest prints. That person can also
**type on the line**, and share it.

```
altairsim> CONNECT sio0:a console|socket:2323
```

The guest talks to your terminal as before. Anyone who connects to port 2323 with telnet sees
the same output, and can type too. A mirror is like a tap: it works with any endpoint. A tap
only listens, but a mirror also speaks. The common use is a console that a program drives. A
person connects, watches the program work, and takes the keyboard when they want to.

The mirror adds no echo. The watcher sees what the guest sends, and what the watcher types goes
to the guest. If the guest echoes (a monitor or CP/M, for example), the typed characters come
back in the usual way. A password that the guest does *not* echo also stays hidden at the
mirror. One watcher can connect at a time, as a serial line is one wire.

Add `?ro` to make the mirror **watch-only**. The watcher then cannot type:

```
altairsim> CONNECT sio0:a console|socket:2323?ro
```

The watcher never sets the speed. If a watcher's connection is slow, or they pause their
terminal, the guest continues. A slow watcher loses a little output, and the guest loses no
bytes. Like the tap, the program keeps the mirror: `SHOW` prints it, and `CONFIG SAVE` writes
it.

## An endpoint that `CONNECT` does not understand is an error

If `altairsim` cannot read your endpoint, it **refuses, and lists the forms that it accepts.**
It never uses `null` instead without telling you.

If it did, you would have a machine that boots, runs and prints nothing. You would look for the
fault in the guest, the board and the disk before you looked at what you typed. An error message
shows the mistake at once.

## Only one unit can hold the console

The console is **your keyboard**, and you have one.

```
altairsim> CONNECT sio1:a console
console taken from sio0:a
sio1:a: connected to console
```

When you connect a second unit to `console`, **it takes the console, and says which unit it took
it from.** This is not an error, and the console is not shared. If two boards read one keyboard,
each one would get about half the characters, in an order that neither could predict.

To see which unit has it:

```
altairsim> SHOW CONSOLE
```

This also shows the transforms, which the rest of this chapter describes.

## The transforms belong to the console

This is the most important rule in the chapter:

**The `[console]` settings are the only part of the simulator that changes a byte. Every serial
LINE is 8-bit clean. No board has a setting that masks a bit.**

The settings belong to the console, so they stop where the console stops. See *Where the
transforms stop*, below.

| Setting | Does |
|---|---|
| `upper` | changes what you type to upper case |
| `strip7in` | clears bit 7 of every character you type |
| `strip7out` | clears bit 7 of every character the guest prints |
| `crlf` | translates line endings |
| `echo` | shows your keys locally |
| `bell` | rings the terminal bell on `Ctrl-G` |
| `bsdel` | makes Backspace and Delete send the same code: `off` (default), `bs` (send BS for both), or `del` (send DEL for both) |
| `stop` | the control character that is the STOP key (default `Ctrl-E`; `attn` is the same setting) |
| `base` | `hex` or `octal`: how the **monitor** prints numbers. This is not a transform. *The Monitor* describes it |

Set them with `CONSOLE k=v`. `SET CONSOLE k=v` does the same thing.

```
altairsim> CONSOLE strip7out=on
altairsim> CONSOLE upper=on crlf=off
altairsim> CONSOLE stop=1D
```

`stop=1D` moves the STOP key from `Ctrl-E` to `Ctrl-]`. **It must be a control character.** A
STOP key that you can type by accident in the middle of a sentence would stop the machine when
you do not want it to.

### Why it works this way: `MEMORY SIZ?`

Boot MITS BASIC with the transforms off, and it asks:

```
MEMORY SIZ?
```

The `E` is missing, and there is a wrong character in its place. BASIC is not broken. **MITS
BASIC sets bit 7 of the last character of every message**, to mark the end of the string, and it
sends that character. `E` is `45`. With bit 7 set, it is `C5`, and your terminal prints whatever
`C5` means to it.

The fix is `CONSOLE strip7out=on`, and the reason that the fix is *there* is the whole argument:

**On the real machine, the board sent all eight bits.** Nothing masked anything. The board put
`C5` on the line, because BASIC gave it that byte. The Teletype at the other end **did not read
bit 7**. On a Model 33, bit 7 is the parity position, and the printer does not decode it. It
printed `E`, and ignored the eighth bit.

Nothing was masked. The device at the far end did not read the bit. **`strip7out` is the
terminal that does not read bit 7.** It is a property of the device that you use, and it belongs
on that device.

### Where the transforms stop

This argument also explains something that can surprise you the first time. If `strip7out` is
your terminal not reading bit 7, then when your terminal is **not** at the far end, nothing
ignores the bit:

```
altairsim> CONSOLE strip7out=on
altairsim> CONNECT sio0:a socket:2323
sio0:a: connected to socket:2323
```

Connect with telnet, and `MEMORY SIZ?` is back, with the wrong character, as if you had never
set `strip7out`. The same is true of `upper`, `crlf`, `echo`, `bell` and `bsdel`, and of a
`serial:` port as well as a socket.

**Nothing was undone.** The console settings are still on, and they still work. The byte no
longer goes through the console. It goes from the 2SIO to the socket to your telnet client, and
every step on that path is 8-bit clean. That is the rule above, not an exception to it.

Set the same thing where it now belongs, on the terminal that shows the text. Every terminal
emulator and telnet client has these settings, under its own names: strip parity or 7-bit
display for `strip7out`, local echo for `echo`, and newline or CR/LF handling for `crlf`. It is
the same fix in the same kind of place, one device further out.

### The built-in terminal has these too, in `[terminal]`

One terminal further out *is* part of the simulator: the built-in `terminal` window (see *A
terminal in its own window*, above). It draws the text itself, so the simulator must ignore the
bit. It has the same settings as the console, under `[terminal]` instead of `[console]`:

| Setting | Does |
|---|---|
| `upper` | changes what you type to upper case |
| `strip7in` | clears bit 7 of every character you type |
| `strip7out` | clears bit 7 of every character the guest prints |
| `cr` | `cr` (default, pass the guest's CR through) or `crlf` (add an LF after every CR) |
| `echo` | shows your keys locally, for half-duplex software |
| `bell` | passes `Ctrl-G` through to the terminal (default on) |
| `bsdel` | makes Backspace and Delete send the same code: `off` (default), `bs`, or `del` |

Set them with `SET TERMINAL k=v`, read them with `SHOW TERMINAL`, or put a `[terminal]` block in
a machine file. Like `[console]`, it is one section for the machine. It applies to whichever
`terminal` line is open.

```
altairsim> CONNECT sio0:a terminal
altairsim> SET TERMINAL strip7out=on
```

`strip7out` is useful here for an **even-parity monitor**. MITS Programming System II puts
parity *into* bit 7 of every character, so it sends a carriage return as `8D`. The console masks
this. In the terminal window, `8D` is not `0D`, so it shows as a character, and the cursor never
goes back to the left. Every line feeds without a carriage return. `strip7out=on` fixes this, as
it fixes BASIC's prompt. `cr=crlf` is a related setting, for a guest that sends a CR alone and
expects the terminal to add the LF.

The STOP key works in the same way. The program reads it at **your keyboard**, before any board
gets the byte, and never looks for it on a socket or a serial line. A `05` that comes down a
cable is part of somebody's protocol. If the program looked for a STOP key on a modem line, it
would change data.

What *does* go down the line is the board's own line coding: baud, data bits, parity and stop
bits. This belongs to the board, not to you. The section after next describes it.

### Why not strap the board to 7 bits

It would work, and then it would change your data without telling you.

Set a 7-bit mask on the board, or a filter on the line, and BASIC's prompt is correct. It also
**damages every XMODEM transfer through that port**, because XMODEM sends binary data. Every
byte is important, and bit 7 is a real bit in half of them. The file arrives, and the checksum
sometimes passes on a bad packet. The fault is in the connection, which is the last place that
anyone looks.

**A line can carry binary. A terminal is not a line.** The transform belongs to the terminal,
because only the terminal knows that it shows text.

### `data_bits` and `parity` are real hardware, and they are different

The 88-SIO and the 88-ACR have `data_bits`, `stop_bits` and `parity` properties, because those
boards set them with jumpers. They are **a FRAME**. They describe what goes down the line, bit
by bit. On a real serial port, the far end must use the same frame, or it reads wrong data.

They are never a mask. `data_bits=7` does not mean "AND the byte with `7F`". It means "put seven
data bits in the frame", which describes the line, not the byte that the guest wrote. Do not use
it to fix a prompt.
