# Graphics on CADzilla: the drawing demo

**drawdemo** shows every drawing command of CADzilla's HD63484 ACRTC graphics processor, one
screen for each command. It runs under CP/M 2.2 on an 8" floppy.

```
cd examples/cadzilla
altairsim drawdemo.toml
```

The machine boots itself to the CP/M prompt. Type `DRAWDEMO`:

```
56K CP/M 2.2b v2.3
For Altair 8" Floppy

A>DRAWDEMO

CADzilla drawdemo: the ACRTC drawing commands, 1024x768, 8 bpp

 1/21  DOT   - random dots placed by AMOVE; border dots stepped
              by RMOVE
Press any key for the next screen (ESC quits)...
```

On a build with **SDL3**, a 1024x768 window opens and comes to the front. The console tells you
what each screen shows.

- Press any key for the next screen. A key typed in the window goes to CP/M, the same as a key
  typed in the terminal.
- Press `ESC` or `Ctrl-C` to stop drawdemo and go back to `A>`.
- Press `Ctrl-E` (STOP) to go to the monitor at any time. Type `RUN` to continue.

On a **headless** build, the machine runs the same program and draws nothing.

## Two speeds

| Machine file | Speed |
|---|---|
| `drawdemo.toml` | **Full speed.** The processor runs as fast as the host can run it. Each ACRTC command is complete as soon as it arrives, so each screen appears at once. |
| `drawdemo-real.toml` | **The speed of the real hardware.** A 2 MHz 8080 in real time, and each ACRTC command takes the time that the HD63484 data sheet gives. The filled shapes and the `PAINT` screen take visible time to draw. |

The two files use the same disk. `drawdemo-real.toml` starts from `drawdemo.toml` and changes
two settings: `cpu0 clock_hz = 2000000` and `cad0 draw_rate = "real"`. You can also change them
at the monitor with `SET cpu0 clock_hz=2000000` and `SET cad0 draw_rate=real`.

## The 21 screens

| | Command | What it draws |
|---|---|---|
| 1 | `DOT` | Single pixels, placed with `AMOVE` and `RMOVE` |
| 2 | `ALINE` | Absolute lines |
| 3 | `RLINE` | Relative lines |
| 4 | `ARCT` | Absolute rectangles |
| 5 | `RRCT` | Relative rectangles |
| 6 | `APLL` | Absolute polylines |
| 7 | `RPLL` | Relative polylines |
| 8 | `APLG` | Absolute polygons |
| 9 | `RPLG` | Relative polygons |
| 10 | `AFRCT` | Absolute filled rectangles |
| 11 | `RFRCT` | Relative filled rectangles |
| 12 | `CRCL` | Circles |
| 13 | `ELPS` | Ellipses |
| 14 | `AARC` | Circular arcs, absolute and relative (`RARC`) |
| 15 | `AEARC` | Elliptical arcs, absolute and relative (`REARC`) |
| 16 | `PAINT` | Area fill to an edge color, solid and with a pattern |
| 17 | `PTN` | The pattern RAM, drawn as a picture |
| 18 | `AGCPY` | Graphic copy, absolute and relative (`RGCPY`) |
| 19 | | The eight operation modes (OPM) |
| 20 | | The color modes (COL), line styles and pattern zoom |
| 21 | | The area modes (AREA): clip, hole and stop |

## The machine

`drawdemo.toml` is the built-in `cadzilla` machine with the parts that CP/M needs added:

- An 8080 processor, an 88-2SIO for the console at port `10`, and 56K of RAM.
- The **CADzilla** board at ports `70`-`77`, set to a 1024x768 monitor. drawdemo sets the ACRTC
  for 1024x768, so the board's `mode` must agree.
- An 88-DCDD floppy controller at port `08`, with the disk in drive 0.
- The DBL boot PROM at `FF00`. `RUN FF00` is the whole startup, as on a real disk Altair.
- The host bridge at port `B0`, for `R`, `W` and `HDIR`.

## The disk

`cpm22b23-56k-drawdemo.dsk` is the CP/M 2.2b system disk from `examples/cpm`, changed to make
room for drawdemo. That disk had 18K free, and `DRAWDEMO.COM` is 18K. These files were
removed:

- `MBASIC.COM`
- the BASIC programs that need it: `LUNAR.BAS`, `STARINS.BAS`, `STARTRK.BAS` and `TICTAK.BAS`

The disk now has `DRAWDEMO.COM` and 60K free. All the other programs are still on it: `ASM`,
`LOAD`, `DDT`, `M80`, `L80`, `ED`, `PIP` and the host-bridge utilities `R`, `W` and `HDIR`.

Drive 0 is read/write, as on a real machine. Nothing restores the disk after CP/M writes to it.
Make a copy of the image before you test writes.

`DRAWDEMO.HEX` is the Intel HEX file of the program. CP/M's `LOAD` makes `DRAWDEMO.COM` from it.
`DRAWDEMO.ASM` is the source. `tools/make-drawdemo-disk.sh` makes the disk from the
`examples/cpm` disk.

This example is in the source repository only. It is not in the release package.
