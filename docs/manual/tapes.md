# Tapes

Before the floppy disk, there was the **cassette recorder**. It was not a special recorder. It
was an ordinary home audio cassette deck, with a microphone jack and an earphone jack. MITS sold
a board that changed bytes into a sound that the deck could record, and changed the sound back
into bytes. That board is the **88-ACR**, and it is in this simulator.

The **Sol-20** also has a cassette interface, but a different one. It is on the Sol's main
board, with two decks, its own modulation, and motor control that the guest can use. The Boards
chapter describes it with the rest of the Sol's onboard hardware, under `sol`. This chapter is
about the 88-ACR. Everything in it (mounting, rewinding, audio files and recording) works in the
same way on the Sol's decks, with a different unit name.

The chapter ends with the most important use of the ACR: loading Altair 4K BASIC 3.1 as people
loaded it in 1976, with nothing in ROM, nothing on a disk, a bootstrap that you enter by hand,
and a tape.

## The 88-ACR

`acr` is the **MITS 88-ACR**, an Audio Cassette Record and playback interface.

It is **an 88-SIO channel B with an FSK modem connected to it**. MITS built it this way. The
guest uses an ordinary serial port, and the modem on the other side changes the bits into tones.
The default port is **06**: `0x06` is status and control, and `0x07` is data. It runs at **300
baud**. This is the speed of the tape, and you cannot change it.

It has one unit, **`tape`**, because a cassette recorder has one place for a tape.

### You cannot `CONNECT` it

The ACR takes no endpoint. You cannot connect it to your terminal, to a socket or to a real
serial port, because **the line goes to the modem on the board**. There is no connector on the
back of an 88-ACR. The signal goes to a cassette deck and nowhere else. **It is a cassette
interface, not a serial port**, even though it is built from a serial port. The serial chapter
describes the boards that *do* take endpoints.

### You put the tape in by hand

**An 88-ACR cannot start, stop or rewind the tape.** It can only hear what goes past the head.
The machine does not know that a tape is there.

For this reason, **a machine file has no key for the tape.** The keys of a machine file describe
the **hardware**: the boards in the backplane, what they decode, and how much memory is on the
bus. The cassette in the recorder is not hardware. You put it in by hand, and you can change it
at any time. A `startup` command can type the `MOUNT` for you, as `examples/basic/basic4k.toml`
does.

**You put the tape in, and you press PLAY.** That is `MOUNT`, and you type it.

## Using a tape

This section describes the one `tape` unit of the 88-ACR. On the Sol's decks, it is the same
with a different unit name (`sol0:tape1`, `sol0:tape2`). Where the two boards are different, the
`sol` section of the Boards chapter says so.

### Putting a tape in

```
altairsim> MOUNT acr0:tape "examples/basic/4K BASIC Ver 3-1.tap"
```

The ACR has one unit, so you do not need to give it:

```
altairsim> MOUNT ACR tape.bin
```

A Sol has two decks, so you must name the deck.

Names are not case-sensitive. The disks chapter gives the rules for names, and they apply here
too.

### `mode = play | record`

```
altairsim> SET acr0:tape mode=record
```

**`play` loads from the file, and `record` saves to it.** The setting gives the direction of the
bytes.

A tape has one head, and it moves in one direction at a time. For this reason, `mode` has two
values only.

### Where the head is: the tape counter

`SHOW MOUNTS` and `SHOW <id>` give the position of the head on the tape:

```
altairsim> SHOW MOUNTS
  acr0:tape  tape  BASIC.WAV  00:15 / 01:28 (17%)  301/2048 bytes
```

The counter is a **time and a percentage**: minutes and seconds into the tape, and how far along
the tape the head is. For a `.WAV` file, the time is the recording's *own* time. It includes the
leader and any silent gaps between programs, as a real cassette counter does. For this reason, a
program that a manual finds at "so many seconds from the start of the tape" is at that second
here. A byte `.TAP` file has no audio, so the program calculates its time from the baud rate.
The byte count is also shown.

### `WIND`: move the head to a time

A tape unit has a command of its own:

```
WIND <id>:<unit> <mm:ss | START | END>
```

You can type `WI`. It moves the head to a time on the tape. On a cassette with several programs,
you can **reach each program**. Read the counter or a manual for the start of the next program,
and wind to that time.

```
altairsim> WIND acr0:tape 2:05
acr0:tape: wound to 02:05 / 08:40 (24%) -- BASIC.WAV (...)
```

The position is a time (`mm:ss`, or a number of seconds), or the word `START` or `END`. A time
after the end goes to the end. On the Sol, you must name the deck, because there are two.

### `REWIND`: wind to the start

`REWIND <id>:<unit>` (`REW`) is the same as `WIND … START`. It is common, so it has its own
command.

**You need it to load the same tape a second time.** After the guest reads a tape, the head is
at the end of the tape, and a second read gets nothing.

```
altairsim> REW acr0:tape
```

### The live counter

When a tape plays in real time (`rate = real`, below), the counter **counts up on the console
while the tape loads**, so that you can see the progress of a long tape. It is on by default.
Turn it off for a machine whose guest writes to the same terminal:

```
altairsim> MOUNT acr0:tape "tape.wav" counter=off
```

You can also type `SET acr0:tape counter=off` at any time. When the counter is off, `SHOW` still
gives the position.

### `stop`: stop the tape at a time

On a tape with several programs, one program runs straight into the next. The `stop` mark is
like pressing the **STOP button of the recorder at a counter mark**. Set it to a time, and the
tape stops there. Set it with `MOUNT`, or with `SET` at any time after that:

```
altairsim> MOUNT acr0:tape "tape.wav" stop=2:05
altairsim> SET  acr0:tape stop=2:05
```

Load program 1, and the line goes silent at 2:05, as at the end of the tape. The loader stops
there, and does not read into program 2. To continue, move the mark forward or clear it, and
play again:

```
altairsim> SET acr0:tape stop=5:30      (the next boundary)
altairsim> SET acr0:tape stop=off       (play to the physical end)
```

The mark is `off` (play to the end) until you set it. `SHOW` shows a set mark as `stop @ 02:05`.
It stops **playback only**. A recording writes past it. It is also separate from `WIND`. If you
wind the head past a set mark, the tape stays stopped there, and `SHOW` says why. Move or clear
the mark to continue.

### `rate = full | real`

**The cassette has its own clock, and by default it runs as fast as possible.** With
`rate = full`, the default, the guest gets each byte as soon as it is ready to read the next
one. A tape loads in about a second, at any processor speed. This is almost always what you
want, because you do not need to wait for the recorder.

```
altairsim> SET acr0:tape rate=real
```

`rate = real` plays the tape in **real time** at the tape's baud rate, 1200 or 300. A load takes
as long as it took on the real machine. Use it when the wait is what you want to show, for
example in a demonstration or a screen recording. It applies to playback only. A recording takes
as long as the guest takes, with either setting.

**`rate` is not the processor clock.** The tape and the processor had separate crystals on the
real hardware, and they are separate here. See *Speed* below. The processor's speed does not
change the tape's speed. Only `rate` sets how fast a tape plays.

## Audio tapes: `.WAV`

Most surviving Altair and Sol cassettes are not files of bytes. They are **audio**. Somebody
played a cassette into a sound card, and saved a `.WAV` file. You can mount one on either board.

**The package has two**, in `examples/basic/`: `4K BASIC Ver 3-1.wav` and `BASIC Ver 1-0.wav`.
They are the two cassette BASICs of that folder, as 88-ACR audio, not as decoded bytes. Each one
has a machine file beside it, `basic4k-wav.toml` and `basic1-wav.toml`, that mounts it and
boots. `4K BASIC Ver 3-1.wav` is a 300-baud FSK cassette of 4K BASIC. In the `basic` example
folder, you can type this:

```
altairsim> MOUNT acr0:tape "4K BASIC Ver 3-1.wav"
acr0:tape: mounted 4K BASIC Ver 3-1.wav
4K BASIC Ver 3-1.wav: fsk300, 4439 bytes, 0 framing errors (100.0% of frames intact)
```

Everything else is the same as for a `.TAP` file. The program decodes the recording **one time,
when you mount it**, and never while the machine runs. After that, `SHOW`'s byte count, `WIND`
and `REWIND` work as they do for a `.TAP`. The audio adds one thing, a *real* clock. The tape
counter gives the recording's own minutes and seconds, with the gaps and the leader. A byte tape
can only estimate these. The guest cannot see any difference.

**Read the second line of the mount output.** A mount always says what it found, and the number
of framing errors is important. A tape that decodes at 60% is noise, not a program. The mount
tells you this before the loader fails. The program refuses a decode below 90%.

**The percentage counts framing only.** It counts the frames whose start and stop bits were in
the correct place. It cannot tell you that the eight bits between them are the bits that were
recorded. A worn or badly copied recording can keep its framing and still give the loader wrong
bytes. A high percentage means only that the decoder stayed in step. If a tape mounts well and
the program still does not run, the recording is probably worn.

**The program decides the format from the start of the file, never from its name.** A `.TAP`
file that somebody renamed `.WAV` is still read as bytes, and a recording renamed `.TAP` is
still decoded as audio.

### `extract`: split a WAV into one `.TAP` file for each program

A cassette WAV often holds several programs, one after another, with a few seconds of silence
between them. **`extract` writes each program to its own `.TAP` file.** You can then keep, mount
or load one program at a time, and you do not have to wind through the whole tape. Ask for it
with `MOUNT`:

```
altairsim> MOUNT acr0:tape "games.wav" extract
acr0:tape: mounted games.wav
  games-1.tap  2048 bytes
  games-2.tap  3120 bytes
2 programs extracted
```

The files go **beside the WAV**, with names from it: `games.wav` gives `games-1.tap`,
`games-2.tap` and so on, numbered from 1. A tape with one program gives `games.tap`, with no
number. Each line gives the name and size of a file. `extract=<base>` sets the names yourself
(`extract=disk1` gives `disk1-1.tap` and so on). `extract` only **reads** the tape and
**writes** the files. Nothing in the machine changes.

The same thing is also a command, so that you can split a WAV that is already in the deck
without mounting it again:

```
altairsim> EXTRACT acr0:tape          (on the Sol, name the deck: EXTRACT sol0:tape1)
```

You can extract only a `.WAV`. A `.TAP` is already bytes, and has no gaps to split on. The
program splits at one second or more of silence. That is much longer than any gap *inside* a
program, so the programs separate cleanly, and no program is cut in half.

### A board refuses audio that it could not really hear

The package has the same 4K BASIC in a modulation that the 88-ACR *cannot* read:
`4K BASIC (Kansas City).wav`, beside the FSK file. With it, you can see the board refuse a tape
that the real board could not hear:

```
altairsim> MOUNT acr0:tape "4K BASIC (Kansas City).wav"
acr0: 4K BASIC (Kansas City).wav: this board's modem cannot hear that tape -- it carries 2400 Hz / 1200 Hz, and this board reads fsk300
```

Not all published Altair cassette audio uses the modulation of the 88-ACR:

- The ACR uses **2400/1850 Hz FSK**.
- Many archive tapes are **Kansas City**, **2400/1200 Hz**. The two formats have the same 2400
  Hz mark tone, but different space tones, 1850 Hz and 1200 Hz.
- The Sol's own CUTS tapes are one octave lower, **1200/600 Hz**, at 1200 baud.

The decoder here measures the tones on the tape, so it *could* read all of them. A real 88-ACR
could not. Its decoder is a PLL centered at 2125 Hz, with a range of about ±100 Hz, and a 1200
Hz space tone is far outside that range. A real board does not read that tape badly. It reads
**nothing**.

If the program decoded the tape anyway, it would give your guest data that no 88-ACR could
produce. For this reason, the board tells you what the tape is, and you use a machine that can
read it.

**The frequencies in that message are a measurement, not the specification of the tape.** The
decoder uses only the points where the signal crosses zero. On a clean copy, the two readings
are at the nominal tones, here 2400 and 1200 Hz. On a worn or badly shaped recording, one
interval can read as a whole cycle or as half of one, and the value can be one octave wrong. The
message means *"this is not my format, and this is about what is there"*.

### Recording back out to a `.WAV`

Put the recorder in `record`. When the tape stops, the program changes the recording back into
audio and writes it over the file, in the format and at the sample rate that it had when you
mounted it:

```
altairsim> SET acr0:tape mode=record
altairsim> RUN 0                        (the guest records)
altairsim> SET acr0:tape mode=play      (...and the WAV is written here)
```

**You can also make a blank tape with `MOUNT … CREATE`.** A blank file has no recording to
examine, so its *name* decides what it becomes. A `.wav` name makes a blank **audio** tape that
records a real WAV that you can play. Any other name makes a blank **byte** tape.

```
altairsim> MOUNT acr0:tape new.wav CREATE mode=record    (a fresh FSK-300 audio cassette)
altairsim> MOUNT acr0:tape new.tap CREATE mode=record    (a fresh byte cassette)
```

The mount line tells you which one you got: `new.wav: blank fsk300 tape, ready to record`, or
`new.tap: blank raw byte tape`. You need `mode=record`, because the deck starts in PLAY, and a
tape moves in one direction at a time. The mode lets the tape *record*. It does not make the
tape audio. The name does that. The guest can then write to the tape, for example with `CSAVE`
in BASIC. When the tape stops, the program writes the recording. `UNMOUNT` and `QUIT` also stop
the tape. Without `CREATE`, `MOUNT` needs a file that already exists, so a name with a typing
mistake is an error, not a blank tape.

**You can also record over a WAV that you mounted.** The program then uses the format and the
sample rate that it found when it decoded the file. The recording writes over the file, so use a
*copy* unless you want to lose the original.

**For a file that already exists, the start of the file decides, never the name.** A `.wav` file
that is not really a WAV (not RIFF/WAVE) is read as bytes, and a recording then puts *bytes* in
it, not audio. It looks as if it worked, but nothing can play it. This applies only to a file
that already exists. A blank file from `CREATE` has nothing to examine, so its name decides, as
above. If you wanted audio and the mount line says `raw`, this is what happened.

A stop writes the recording. `UNMOUNT` and every `WIND` (with `REWIND`) are also stops. The
Sol's decks have one more stop, which the ACR does not have: the guest turns the motor off. The
`sol` section of the Boards chapter describes it.

The program writes the whole file again each time. It cannot change only a part, because where
the audio of a byte starts depends on every byte before it.

**The timing is the one thing that a round trip loses.** A byte image holds no times, so the
program that writes the audio must add the leader that a real tape needs. Two properties do
this, in seconds:

| Property | 88-ACR | Where the number comes from |
|---|---|---|
| `leader` | `15` | the MITS manual's *at least ~15 s of steady tone* |
| `trailer` | `5` | §8's *at least 5 s between batches* |

The Sol's decks have a shorter leader and trailer by default, measured from a real tape. The
`sol` section of the Boards chapter gives the numbers.

Set either one to `0` to cut the file down to its data. The published archive `.wav` files are
cut in this way, and for this reason they do not load on real hardware. Even at `0`, the program
puts sixteen bit times of tone at each end. The loader finds a start bit by its **edge**, so a
tape that started with the start bit would lose its first byte.

**You can shape the tone as the real hardware did, or smooth it.** The `waveform` property sets
the shape of the tone when the program writes audio:

| Property | Default | Choices | What it does |
|---|---|---|---|
| `waveform` | `square` | `square` \| `sine` | `square` is what the real modem lays down — a fuller, louder tone that sounds like a genuine cassette dub. `sine` is a smoother, quieter tone. |

`waveform` changes only how the recording **sounds**. A tape written either way decodes to the
same bytes. `square` is the default, because it is nearer to a real recorder.

**The recording level decides whether the tape reads back cleanly.**

| Property | 88-ACR | What it does |
|---|---|---|
| `level` | `36` | Recording level, in percent of full scale. This is the level of a real tape. A much higher level overloads the input of a real deck, and the tape then fails after its header |

`level` is not only a question of sound. A tape written at the level of a real tape is made to
load on the hardware, not only to read back here. The Sol's CUTS modem needs one more property,
`rc`, to shape the edges of its lower tone. The `sol` section of the Boards chapter describes
it.

**A tape with several files comes back as one continuous recording.** The decoded bytes do not
mark where one file ends, so the gaps that an operator left between programs are not written
back.

Recording to a `.TAP` file works as before, and none of this applies to it.

### `format`: when you must choose the format yourself

Each tape unit has a `format` property. `auto` is the default, and it is almost always correct.

```
altairsim> SET acr0:tape format=raw
altairsim> SHOW acr0
```

`SET` names the unit. `SHOW` names the **board**, and lists every unit of the board with its
properties. There is no `SHOW <id>:<unit>`.

| Value | What it does |
|---|---|
| `auto` | Sniff for RIFF magic; demodulate a recording, read anything else as bytes |
| `raw` | Read the file's own bytes **even if it is a WAV** — how you inspect a tape that decodes badly |
| a modulation | Force one: `fsk300` on the ACR, `cuts1200` or `kcs300` on the Sol |

`format` selects how the program *reads* the file. It never changes the hardware. If you tell an
88-ACR to decode `cuts1200`, it refuses, as it does when it detects that format itself. The
board has the modem that it has. The read-only `detected` property gives the format of the
mounted tape.

`format` also selects the modulation of a **blank** tape from `MOUNT … CREATE`. There is no
other setting for it. A `.wav` name records with the board's own modem (`fsk300` on the ACR).
`format=fsk300`, or on the Sol `format=cuts1200` or `kcs300`, selects one by name. An empty file
has nothing to examine, so `format` or the `.wav` name is the only choice.

`format` takes effect at the **next** `MOUNT`, because the program decodes a tape one time, when
you put it in.

## Loading Altair 4K BASIC 3.1: the steps

This is what an Altair owner did every time that they wanted to use BASIC, because there was no
place to keep it.

**1. Start the machine.** From the package folder, start the built-in `basic4k` machine. It has
the 88-ACR, and its sense switches are set for a cassette load:

```
$ altairsim basic4k
```

**2. Put the cassette in.**

```
altairsim> MOUNT acr0:tape "examples/basic/4K BASIC Ver 3-1.tap"
```

**3. Enter the bootstrap.**

```
altairsim> LOAD "examples/basic/LDR4K31.HEX"
```

The bootstrap is about twenty bytes. **On a real Altair, you entered it by hand on the
front-panel switches.** You set eight switches for each byte, pressed DEPOSIT, and did it again,
twenty times. If a bit was wrong, the tape did not load. MITS printed the listing in the manual,
and you entered it with your fingers. `LOAD` does that job, and nothing more. It puts the same
twenty bytes in memory.

**4. Run it from address zero.**

```
altairsim> RUN 0
```

The bootstrap starts the ACR. The tones come off the tape, BASIC loads into memory, and it
starts itself.

BASIC then asks three questions:

```
MEMORY SIZE?
TERMINAL WIDTH?
WANT SIN? Y

742 BYTES FREE

ALTAIR BASIC VERSION 3.1
[FOUR-K VERSION]

OK
```

An empty answer to the first two means "all of it" and "the default". `WANT SIN?` asks whether
you want to use some of your 4K for trigonometry. Type `Y`. You then have 742 bytes free and a
working `SIN`.

You are in Altair BASIC. This was the first product that Microsoft sold.

To do all of this with one command, use the machine file in the package:

```
$ altairsim examples/basic/basic4k.toml
```

That machine file types the commands for you. It has no special powers. You could type every
line in it.

### The sense switches are important

The `basic4k` machine sets **`sense = 0x80`** on the front panel, and the setting is needed.

The bootstrap reads the sense switches to find **the device to load from**. Its own printed
header says: *"Set A15 on (cassette load), all other switches off."* A15 on, and no other
switch, is `0x80`. If the setting is wrong, BASIC loads from **the wrong device**. It then waits
for bytes that never come, from a teletype that is not there.

If a tape load stops, and you are sure that the tape is mounted, **check the sense switches
first.** The `fp` section of the Boards chapter describes them.

### Speed: the tape and the processor have separate clocks

**The machine runs as fast as possible by default, and so does the tape.** A cassette that took
a real Altair about **110 seconds** loads in **about one second**. The two have separate clocks,
as on the hardware. The processor's speed is `clock_hz` on the processor board. The tape's speed
is `rate` on the deck (above). Neither one changes the other.

`SET cpu0 clock_hz=2000000` gives the *processor* the speed of the period, so that a game plays
at its real speed. `SET acr0:tape rate=real` is a separate setting that makes the *load* take
its real time. With `rate = full`, the guest cannot see a difference at any processor speed,
because a polled loader only asks for the next byte, and the byte is always ready.

## Altair Disk Extended BASIC 4.1

The other BASIC in the package does not use tape. Altair Disk Extended BASIC 4.1 is on an 8"
floppy disk. It has what a cassette cannot give you: files, a directory, and a `SAVE` that takes
a name.

```
$ cd examples/diskbasic
$ altairsim diskbasic.toml
```

It asks five questions before it starts, and **the second question is the one that stops
people**:

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

For `MEMORY SIZE?`, press Return to use all of it. `HIGHEST DISK NUMBER?` is `0`, which means
one drive, numbered from zero. For the last two, press Return.

**`LINEPRINTER?` accepts only `C`, `O` or `Q`.** If you give a blank line or `N`, it asks again.
It gives no error and no hint. A machine that asks the same question again after every answer
looks as if it has stopped, but it has not. The program expects you to have its manual open. `C`
is the 88-C700 line printer. It is a correct answer here even though this machine has no printer
board. The answer only tells BASIC where `LPRINT` goes, and nothing is sent until you use
`LPRINT`.

After that, it is BASIC:

```
OK
PRINT 2+2
 4 
```

The disk is mounted read/write, as on a real machine, so `SAVE` writes to it.
