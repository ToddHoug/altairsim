# Altair 4K BASIC off a cassette

```
altairsim basic4k.toml

MEMORY SIZE? <return>
TERMINAL WIDTH? <return>
WANT SIN? Y

742 BYTES FREE

ALTAIR BASIC VERSION 3.1
[FOUR-K VERSION]

OK
```

**Altair 4K BASIC 3.1**, read off a period cassette image by the bootstrap that MITS shipped.
Both are unmodified. The machine is the bare 1975 Altair: 4K of RAM, an 88-SIO for the
Teletype, an 88-ACR for the recorder, and the sense switches at `0x80`.

The three lines in the `startup` list of `basic4k.toml` are the three things that an operator did
in 1975:

1. Put the tape in the recorder and press PLAY (`MOUNT`).
2. Toggle in the bootstrap on the front panel (`LOAD`).
3. Run it from zero (`RUN 0`).

The bootstrap (`LDR4K31`) jumps into BASIC when the tape ends, so one command gets you to the
prompt.

The tape loads in about one second. The real 300-baud cassette took 110 seconds. The difference
is the default clock, which runs as fast as possible. Type `SET acr0:tape rate=real` before you run
to play the tape at its real speed. BASIC gets the same result at either speed.

`basic4k.ini` builds the same machine with monitor commands: `altairsim -s basic4k.ini`.

## The same tape in audio form

Most surviving cassettes are audio recordings, so this folder has the tape in audio form too.
`4K BASIC Ver 3-1.wav` holds the program of the `.tap`, encoded in the audio format of the
88-ACR. `basic4k-wav.toml` mounts it and boots. The board decodes the audio in the same way that
the real board did:

```
altairsim basic4k-wav.toml
```

`4K BASIC (Kansas City).wav` holds the same program, encoded in the Kansas City audio format.
The 88-ACR cannot read that format, so the board refuses the tape when you mount it. The tapes
chapter of the manual tells you why.

## The files

| File | What it is |
|---|---|
| `basic4k.toml` | The machine file. `base = "basic4k"` is the built-in hardware. This file puts the tape in the recorder and runs it. |
| `basic4k.ini` | The same machine, set up by monitor commands. |
| `basic4k-wav.toml` | The same machine, with the audio tape in the recorder. |
| `basic4k-wav.ini` | The same, set up by monitor commands. |
| `4K BASIC Ver 3-1.tap` | The cassette. |
| `4K BASIC Ver 3-1.wav` | The program, encoded in the audio format of the 88-ACR. |
| `4K BASIC (Kansas City).wav` | The program, encoded in the Kansas City audio format. The 88-ACR cannot read it. |
| `LDR4K31.HEX` | The bootstrap, assembled. |

The bootstrap source, `LDR4K31.ASM` and `.PRN`, is in `tapes/4KBasic31/` in the repository. It is
source, not product, so it is not in the package.
