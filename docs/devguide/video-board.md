# Writing a video board

The lamp chapter built a board that latches a byte. This one builds a board that **paints** —
and it is the same board with one more connection: a host video service it draws into. The
worked example is **cadzilla** (`src/boards/cadzilla.{h,cpp}`, `docs/boards/cadzilla.md`), an
HD63484 ACRTC with a Bt453 RAMDAC, chosen because it exercises every part of the seam: a chip
with its own frame memory, a chip that *is* the palette, and a picture you can only reach by
issuing commands. The Dazzler and the VDM-1 are the two simpler shapes and are cited where
they differ.

Everything here is tested with **no window**. That is not a limitation to work around; it is
the design, and the last section is how you prove a picture without one.

## 1. The nine questions

A video board is an ordinary `Board` that happens to paint, and nine decisions turn into code.
Answer them in the board's `docs/boards/*.md` **before** writing the class; seven become a
method each, and the other two are the Limitations section.

| # | Question | cadzilla's answer | Where it lands |
|---|---|---|---|
| 1 | **Name.** The chip or the common name, never a catalog number. "board", never "card" (`DESIGN.md` §0.3) | `cadzilla` | `type()`, the registry, the filename |
| 2 | **I/O footprint** — which ports, and *separately* which answer `IN` vs `OUT`: the bus decodes by cycle type | one 8-port block from `port`: `+0`/`+1` the ACRTC (both directions, different registers each way), `+3` the board's own MODE register (write-only), `+4`..`+7` the Bt453 (both directions); `+2` answers nothing | `decodes()`, `ioMap()` |
| 3 | **Memory footprint.** On-board screen RAM the CPU addresses (VDM-1 decodes a 1 KB window), a framebuffer in main RAM (Dazzler decodes none, reads RAM), or private memory (cadzilla decodes none, and the CPU never sees it) | none | `decodes()`, `memMap()` |
| 4 | **Where pixels come from and how you know they moved.** A write to on-board RAM latches a dirty flag; main RAM has to be polled against a shadow; a chip with its own memory tells you | `Hd63484::takeDirty()`, `Bt453::takeDirty()` | `pump()` |
| 5 | **Geometry.** Native w×h — fixed, decoded from registers each frame, or **the monitor's**: a board with a real CRT controller can carry a fixed-frequency display and place the chip's picture in it. `PixelFormat::Indexed8` is the only format the seam has | the `mode` strap's VESA frame (640x480 by default); the ACRTC's picture placed by HDS/VDS against the mode's porches | `render()`, `acquire()` |
| 6 | **Palette.** How many entries, from where. Dazzler: 16 from an RGBI nibble. VDM-1: 2. cadzilla: **256, and they are a chip** | `Bt453::palette()` | `setPalette()` |
| 7 | **Observable timing.** A status bit a guest can *time* (vblank, scan parity) comes off `clock_->now()` — never a poll counter. An oscillator the guest cannot observe (a cursor blink) comes off `Display::hostSeconds()` | none yet — see Limitations | `statusByte()` |
| 8 | **Straps vs live status.** A strap has a setter and round-trips through `CONFIG SAVE`; live status has **no setter**, and that absence is the whole signal. A register that is write-only *on the wire* (a chip's format byte, or a board's own glue register) is still reflected as read-only live status — the board keeps the shadow the wire cannot give back. A fixed hardware fact with no jumper on the real card (cadzilla's 2 MB of SRAM) is neither a strap nor live status — it is a constant. Every video board pushes `Display::widthProperty(videoWidth_)` | `port`, `mode`, `width`, `interrupt`; live `video`, `picture`, `wiring`, `hspol`/`vspol`/`amode`/`olen`, `status`, `irq` | `properties()` |
| 9 | **Snapshot.** Runtime state only — never a strap, never the `Display*`. If no memory board holds your pixels, **they travel with you** | both chips, frame memory as a `blob` | `serialize()` |

Two things that are *not* the board's business, and each is the classic mistake: a **keyboard**
is a separate board or an endpoint, never the window (`host/display.h`'s header note); and the
**frame rate** is the host's (`wantsFrame()`), never yours.

## 2. Chips first, if the board has any

cadzilla's two parts are chips before they are a board, in `src/chips/`, and that split is
the one the FD1771 and the 8257 already made (`chips/wd17xx.h`, `chips/i8257.h`): a chip
knows nothing about S-100 or about the board's ports. It has *pins* — the ACRTC's RS, the
Bt453's C1C0 — and the board decodes an address onto them:

```cpp
uint8_t CadzillaBoard::read(const BusCycle& c) {
    uint8_t off = (uint8_t)(c.port() - port_);
    if (off == 0 || off == 2) return acrtc_.read(off == 2);   // RS: +0 status, +2 data/FIFO
    return dac_.read(off & 3);                                // C1C0 = A1A0
}
```

The pay-off is that a chip is tested on its own, from its data sheet, with no bus at all —
`tests/test_hd63484.cpp` and `tests/test_bt453.cpp` — and the board test is left with only the
board's own decisions to prove. The **decisions a chip cannot make** are the board's, and they
belong in the board header where cadzilla's are: the shift register (8 bits per pixel, 8 words
per fetch, low byte first), the monitor (a fixed VESA frame), how much frame memory was fitted,
what the overlay inputs are tied to, where IRQ\* is strapped. Draw the line where the hardware draws it:
the ACRTC only ever puts an *address* on its bus, so `Hd63484` answers address-level questions
(`backgroundRaster()`, `windowRaster()`, `gaiWords()`) and the *board* fetches words and makes
pixels of them — which is also why a program that sets the chip to 4 bpp gets a scrambled
picture here, as it would on the card, instead of a helpfully re-unpacked one.

## 3. The board: three gates and one call

The whole host side of a video board is `pump()`, and it is three gates in a fixed order:

```cpp
void CadzillaBoard::pump() {
    if (!g_display) return;                      // no service at all (the bench)
    bool a = acrtc_.takeDirty();                 // consume BOTH flags every time, so one
    bool d = dac_.takeDirty();                   //   is never lost behind the other
    dirty_ = dirty_ || a || d;
    if (!dirty_) return;                         // deterministic: would it look different?
    if (!g_display->wantsFrame()) return;        // host-side rate limit; 0 = always, in tests
    render();
    dirty_ = false;
}
```

The order matters. Change detection is **deterministic** and always on: it is what makes a
headless test reproducible. The frame limit is **wall-clock** and off in tests: it is a host
economy that changes what is *drawn*, never what is *computed*. Put them the other way round
and a test's frame count depends on the machine it ran on.

`render()` is one call that is the window, and its arguments are not decoration:

```cpp
Surface* s = g_display->acquire(this, id, m.width, m.height, PixelFormat::Indexed8, videoWidth_);
```

`this` keys **this board's own window** (issue #234: two boards of the same resolution would
otherwise land on one Surface), `id` **titles** it, `videoWidth_` **sizes** it — and `m` is the
monitor mode, so the frame is the monitor's, not the chip's. Get that line right and the SDL
back end's windowing, integer scaling, CRT look, focus policy and close box all arrive with no
further code — and a `NullDisplay` keeps the same Surface per owner for a test to read. Then
paint every pixel (the Surface may be last frame's buffer), hand over the palette, and present:

```cpp
g_display->setPalette(this, dac_.palette());     // the RAMDAC IS the palette, verbatim
s->clear(0);                                     // blanking is black
if (on) paintFrame(s, m.width, m.height);        // the shift register, over the ACRTC's addresses
g_display->present(this, s);
```

That is the seam's whole shape, and for a board with a RAMDAC it is not an abstraction: an
`Indexed8` Surface *is* the eight-bit pixel bus and `setPalette()` *is* the look-up table.
Nothing gets translated.

## 4. Wiring — every file a video board touches

| File | What goes in |
|---|---|
| `CMakeLists.txt` | the `.cpp`s in `altair_core`; the test in `altair_tests` |
| `src/boards/registry.cpp` | `boardTypes()` and `makeBoard()` |
| `src/main.cpp` **and** `tests/main.cpp` | `YourBoard::setDisplay(&g_display)` — miss the second and the board draws into nothing in every test |
| `tests/test.h`, `tests/main.cpp` | the suite's declaration and its table row |
| `machines/<name>.toml` | a sample machine |
| `docs/boards/<name>.md` | from `_TEMPLATE.md`; Limitations and Quirks are the load-bearing sections |
| `docs/manual/boards.md`, `docs/changelog/changelog.md` | the prose list and an `Unreleased` line — **no test guards either** |
| `docs/manual/ref/` | regenerated: `cmake --build build --target docs-reference` |

Nothing in the bus, the monitor, the TOML loader or the MCP server changes. `properties()` is
the entire configuration layer.

## 5. Proving a picture with no window

`test_dazzler.cpp` used to check a picture one pixel at a time — `px(32, 0) == 1` — which is
fine for "is this element red" and useless for "is this the picture", and prints nothing you
can look at when it fails. Two pieces fix that.

**`host/framedump.h`** takes a Surface and its palette out of the seam in the forms a person
and a test can use: `frameText()` renders it as a **text grid**, one character per pixel from a
legend; `framePpm()` / `writePpm()` resolve it through the palette into a **PPM image** any
viewer opens; `frameCrc()` hashes the resolved RGB, so a palette-only change moves it. For a
board with a RAMDAC, `frameRgb()` is literally the DAC in software — what the wire carries.

**`tests/framecheck.h`** builds the assertion on top. The expected picture is a raw string
literal *in the test*, readable by a person; on a mismatch the failure prints the first row that
differs with a caret under the column **and writes what the board drew as a `.ppm`**, naming the
path. A 640x480 frame is not readable one character per pixel, so cadzilla's end-to-end test
draws its rectangle, line and dot on a 32-pixel grid and samples the frame at the same pitch —
20 x 15 characters that a person can check against the commands that drew them:

```cpp
TextGridOpts every32;
every32.xStep = 32;
every32.yStep = 32;
CHECK_FRAME_OPTS(g.disp, g.cad, R"(
11111111111111111111
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1.3333333333333333.1
1..................1
1..................1
1..................1
1.........2........1
1..................1
1..................1
11111111111111111111
)", every32, "sampled every 32nd pixel: the rectangle, the line and the dot, right way up");
```

Every character is a pixel's palette index. A rectangle one row too high, a mirrored X axis, a
picture placed one memory cycle late — each is visible in the diff, and the `.ppm` beside it
shows the full-resolution picture through the colors the guest loaded.

**Be clear about what a sampled grid proves.** It looks at one pixel in a thousand: it proves
the *geometry* in a form a person can read, and nothing about the pixels between the samples.
So pair it with **`CHECK_FRAME_PIXELS`**, which compares **every** pixel against an oracle the
test writes — a function from `(x, y)` to the palette index the commands must have left there.
For a rectangle, a line and a dot that is four comparisons:

```cpp
auto oracle = [](int x, int y) -> uint8_t {
    if (x == 320 && y == 352) return 2;                                  // the dot
    if (y == 224 && x >= 64 && x <= 544) return 3;                       // the line, end excluded
    bool onRect = (y == 0 || y == 448) ? (x <= 608)                      // top and bottom edges
                                        : ((x == 0 || x == 608) && y < 448);   // the sides
    return onRect ? 1 : 0;
};
CHECK_FRAME_PIXELS(g.disp, g.cad, oracle, "all 307,200 pixels match the oracle");
```

A single wrong pixel anywhere fails it, and the failure names the first differing pixel, how
many differ in all, and the `.ppm`. The grid is for the reader; the oracle is for the proof.
Use both. For a frame you cannot describe in a function, keep a golden file under
`tests/golden/` and `CHECK_FRAME_GOLDEN` it; the golden is rewritten only under
`ALTAIR_TEST_WRITE_GOLDEN=1`, because a golden that rewrites itself asserts nothing.

Then prove the two things a picture alone does not: that a **palette-only change** reaches the
host (`frames()` advanced, `frameCrc()` moved, the pixel bytes unchanged), and that a
**snapshot** repaints the same grid on a fresh board. Both are in `tests/test_cadzilla.cpp`.

## What cadzilla left for next time

Its own `docs/boards/cadzilla.md` says exactly what is not modeled. **Interrupts** are wired —
`IrqJumper` and `irqJumperProperty()`, `assertsInt()`/`assertsVi()` reading the chip's own
`irq()`, and `intChanged()` called from every place that could move, per the lamp chapter's
closing section — so that recipe step is already done here and is the worked example for the
next board that needs it. Two still remain: **observable timing** (a raster counter a guest
reads must come off the `Clock`, like the Dazzler's vblank bit), and **the rest of the command
set**, which drops into `Hd63484::execute()`'s dispatch with its parameter count already in
`paramsFor()`.
