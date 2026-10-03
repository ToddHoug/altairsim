# Altair BASIC 1.0 off a cassette

```
altairsim basic1.toml
```

The machine loads the tape and then waits. Press `Ctrl-E`, then type `RUN 0`:

```
altairsim> RUN 0

MEMSIZ? <return>
WANT SIN-COS-ATN? <return>

2000 BYTES FREE

8080 BASIC VER 1.0

READY
```

**"8080 BASIC VER 1.0"** is the first Altair BASIC. Bill Gates and Paul Allen wrote it for the
Altair in 1975. This folder reads it off a period cassette image with the bootstrap that MITS
shipped. Both are unmodified. The machine is the bare 1975 Altair: 8K of RAM, an 88-SIO for the
Teletype and an 88-ACR for the recorder. BASIC 1.0 loads to `0000-117F`, so it does not fit in
4K.

## Why the boot has two steps

The bootstrap (`LOAD10`) copies the tape into memory from `0000`, and then loops forever. It does
not know the length of the tape, so it never jumps to the program that it loaded. In 1975, the
operator watched the tape end, stopped the machine and started BASIC from 0 by hand. You do the
same:

1. Type `altairsim basic1.toml`. The machine puts the tape in the recorder, enters the bootstrap
   and runs it (`RUN 1800`).
2. Press `Ctrl-E`. This is the STOP switch of the front panel. It gives you the monitor, and
   memory keeps what the bootstrap loaded.
3. Type `RUN 0` to start BASIC.

The 4K BASIC in `examples/basic4k` boots in one step, because its bootstrap jumps into BASIC when
the tape ends.

`MEMSIZ?` is the real spelling. MITS BASIC marks the end of a message by setting bit 7 of its
last character. Microsoft set that bit on the `Z`, and did not spend a byte on an `E`. Press
Return at `MEMSIZ?` and at `WANT SIN-COS-ATN?`.

`basic1.ini` builds the same machine with monitor commands: `altairsim -s basic1.ini`.

## Delete a character with `_`

The Backspace key does not work in BASIC 1.0. To delete the last character that you typed, type
`_` (underscore). This is how a Teletype did it. For example, `PRINT 5Q_` prints `5`.

## Watch the tape load

The tape loads in less than one second, because the default clock runs as fast as possible. The
real 300-baud cassette took two and a half minutes. To play the tape at its real speed, press
`Ctrl-E` and type:

```
altairsim> SET acr0:tape rate=real
altairsim> REWIND acr0:tape
altairsim> RUN 1800
```

The tape counter counts up on the console while the tape loads. When it gets to `100%`, press
`Ctrl-E` and type `RUN 0`. BASIC gets the same result at either speed.

## The same tape in audio form

`BASIC Ver 1-0.wav` holds the program of the `.tap`, encoded in the audio format of the 88-ACR.
`basic1-wav.toml` mounts it. The board decodes the audio in the same way that the real board did,
and the boot has the same two steps:

```
altairsim basic1-wav.toml
```

## The files

| File | What it is |
|---|---|
| `basic1.toml` | The machine file. It describes the whole machine, puts the tape in the recorder and starts the bootstrap. |
| `basic1.ini` | The same machine, set up by monitor commands. |
| `basic1-wav.toml` | The same machine, with the audio tape in the recorder. |
| `basic1-wav.ini` | The same, set up by monitor commands. |
| `BASIC Ver 1-0.tap` | The cassette. |
| `BASIC Ver 1-0.wav` | The program, encoded in the audio format of the 88-ACR. |
| `LOAD10.HEX` | The bootstrap, assembled. |

The bootstrap source, `LOAD10.ASM` and `.PRN`, is in `tapes/MSBasic10/` in the repository. It is
source, not product, so it is not in the package.
