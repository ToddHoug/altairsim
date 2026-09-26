#pragma once
//
// CADzilla -- an HD63484 ACRTC graphics board with a Bt453 RAMDAC. A new custom product
// based on period components (2026); see docs/boards/cadzilla.md.
//
// TWO CHIPS ON SIX I/O PORTS, AND A FRAME MEMORY THE CPU NEVER SEES.
//
//   I/O     -- ONE 8-port block from BASE (default 0x70; BASE must be a multiple of 8):
//                BASE+0  ACRTC RS=0 (OUT: address register, IN: status)
//                BASE+1  MODE register (write-only) -- the board's own glue-logic strap,
//                        not a chip register; see MODE REGISTER below
//                BASE+2  ACRTC RS=1 (the 16-bit register the address names, one byte per
//                        cycle, or the command FIFOs)
//                BASE+3  the high byte of a 16-bit Command/FIFO transfer on the real
//                        card (SW1-7) -- an 8080/Z80 never makes one, so nothing answers
//                BASE+4..+7  Bt453 by C1C0 -- address register, color palette RAM, address
//                        register again, overlay registers -- all read/write
//              The ACRTC's own RS=0/RS=1 pair is therefore NOT adjacent on this board --
//              MODE sits between them. That is a real decode choice (SW1 selects the
//              8-port BASE; within the block, address bit A1 is the ACRTC's RS pin and
//              A0 picks MODE vs. the ACRTC at A1=0), not an accident.
//   MEMORY  -- NONE decoded. The frame memory is 2 MB of SRAM on the board, private to the
//              ACRTC and fixed (no refresh cycles, no `vram` strap -- the reference design
//              is exactly this much and no other); the CPU draws by issuing commands
//              through the FIFO, never by poking a byte. The opposite reason from the
//              Dazzler's for decoding no address.
//   DISPLAY -- once per pump() the board builds the MONITOR'S frame -- a fixed VESA raster
//              chosen by the `mode` strap, 1024x768 by default -- by running its own shift
//              register over the addresses the ACRTC puts out, and hands the host the
//              Bt453's 256-entry look-up table as the palette. Never touches SDL: the
//              Display is injected at the composition root (setDisplay), a headless build
//              gets a NullDisplay, and the board runs and is tested with no window
//              (DESIGN.md 7.4).
//
// THE PIPELINE IS THE POINT. ACRTC frame memory -> 8-bit pixel value -> Bt453 LUT -> 24-bit
// RGB, and the Display seam is shaped exactly like it: an Indexed8 Surface IS the P0-P7
// bus, and setPalette() IS the RAMDAC. Nothing is translated; the board just wires them.
//
// ---------------------------------------------------------------------------
// WHAT THE BOARD DECIDED (a chip cannot), and every one of them is visible from the bus:
//
//   THE SHIFT REGISTER IS WIRED FOR 8 BITS PER PIXEL AND 8 WORDS PER FETCH. Each display
//   cycle the ACRTC puts one address on MAD; the board fetches EIGHT consecutive words
//   there (128 bits) and shifts them out as SIXTEEN 8-bit pixels, low byte of the low word
//   first (so the ACRTC's +X is rightward -- chips/hd63484.h). That is a hardware fact,
//   not a register: a program must set CCR GBM = 011 (8 bpp) so the drawing engine's
//   pixel arithmetic agrees with the wire, and OMR GAI = 011 (+8 words) so the ACRTC's
//   address steps over exactly the words the board fetched. A program that sets either
//   differently gets what the hardware would give -- a scrambled picture, not an error --
//   and SHOW's `wiring` line says which register is off.
//
//   THE MONITOR IS A FIXED-FREQUENCY VESA DISPLAY, chosen by `mode`: the three primary VESA
//   resolutions -- 640x480, 800x600 or 1024x768 (default), settable in the machine file or
//   with SET. The frame the window shows and the tests capture is
//   ALWAYS the mode's size. The ACRTC's programmed display area is placed in it exactly as
//   a monitor would place it: its horizontal display start (HDS) is counted in memory
//   cycles from HSYNC's rising edge and the mode's back porch is a fixed number of those,
//   so a picture programmed to start where the porch ends fills the frame from the left
//   edge, and one that starts earlier or later is shifted and clipped. Vertically the same
//   with VDS and the vertical back porch. The register values for each mode are in
//   docs/boards/cadzilla.md.
//
//   ACCESS MODES: single (ACM = 0x) and interleaved (ACM = 10). In interleaved mode the
//   ACRTC takes two memory cycles per display cycle, so every horizontal register value
//   (HC, HSW, HDS, HDW, HWS, HWW) is doubled for the same picture and each memory cycle is
//   worth 8 pixels of the frame instead of 16. Superimposed mode (ACM = 11) is not wired:
//   the window's second phase would need its own fetch path.
//
//   Also: OL1..0 tied low (no overlay source); IRQ* wired to the `interrupt` strap (SW1-8
//   on the real card enables it; off, the default, disconnects it -- see below).
//
//   2CLK IS THE BOARD'S. The ACRTC's clock comes from the monitor's pixel clock (PCLK, the
//   VESA rate of `mode`): PCLK/8 in single access mode, PCLK/4 in interleaved -- selected
//   by MODE AMODE, the board's glue, not the chip's OMR ACM. A memory cycle is then 16
//   pixels single and 8 interleaved, which is why the horizontal registers double.
//
// DRAWING TIME -- the `draw_rate` strap. `full` (the default): the ACRTC draws in no time,
// as it always has here -- the fastest bench for developing a program. `real`: each command
// costs its datasheet Table 3 time in 2CLK (chips/hd63484.h, DRAWING TIME), so the write
// FIFO backs up and CED comes late exactly as a timing-sensitive program -- a game -- would
// see on the card. The board turns T-states into 2CLK (sync) and arms one Clock deadline
// at the command's end (arm), so an interrupt on CED lands on time with the CPU halted.
// It is not the CPU's `clock_hz`: that only decides whether the host waits, and the guest
// cannot see it; the drawing time is emulated time, the same number of instructions long
// at any `clock_hz`.
//
// THE MODE REGISTER (BASE+1, write-only) is the board's own glue logic, not a register on
// either chip -- the host programs it in tandem with the ACRTC's own OMR when it sets up
// the picture, and it is what the board's OWN fetch logic (programmedWidth/X, paintFrame)
// reads for single vs. interleaved access, not the ACRTC's OMR bit. The bit assignment
// below (0..3 in the order the fields were specified) is this implementation's own choice
// -- the spec names the four fields and their meanings, not their bit positions -- and this
// comment is the one place that choice is recorded:
//
//   bit 0  HSPOL  horizontal sync polarity: 0 = positive sync pulse, 1 = negative
//   bit 1  VSPOL  vertical sync polarity:   0 = positive sync pulse, 1 = negative
//   bit 2  AMODE  access mode the board's glue expects: 0 = single, 1 = interleaved --
//                 must agree with the ACRTC's own OMR ACM bit, or `wiring` says so
//   bit 3  OLEN   overlay enable -- TBD; keep 0. Not wired to anything yet: OL1..0 stay
//                 tied low regardless (see above), so a set OLEN changes nothing today.
//   bits 4-7      unused
//
// HSPOL/VSPOL are recorded and reported (`SHOW`'s hspol/vspol) but drive nothing: this
// model has no separate sync-pulse signal for a polarity to invert. See Limitations.
//
// THE 8-POSITION DIP SWITCH SW1, for reference (docs/boards/cadzilla.md has the full row):
// SW1-1..5 pick the I/O BASE (bits 7-3, i.e. the `port` strap); SW1-6 (16-bit host mode)
// and SW1-7 (16-bit I/O decode) have no expressible effect on an 8080/Z80 S-100 bus, which
// only ever runs 8-bit IN/OUT cycles -- both are fixed Off in this machine, not straps;
// SW1-8 is the `interrupt` strap below: Off (`none`, the default) disconnects IRQ*, On
// means the strap names which S-100 line it reaches (`int` or `vi0`..`vi7`).
// ---------------------------------------------------------------------------

#include "chips/bt453.h"
#include "chips/hd63484.h"
#include "core/board.h"

#include <cstdint>
#include <string>
#include <vector>

namespace altair {

class Display;  // host/display.h -- injected; the board never learns it is SDL

class CadzillaBoard : public Board {
public:
    CadzillaBoard();
    ~CadzillaBoard() override;  // cancels the drawing deadline (a fired stale alarm is a UAF)

    std::string type() const override { return "cadzilla"; }

    bool    decodes(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;

    void reset(Reset r) override;
    void power() override;
    void pump() override;

    // ---- Interrupts: IRQ* strapped to a bus line, or disconnected (SW1-8) ----
    //
    // acrtc_.irq() is already pure and combinational (SR & CCR's enable bits) -- exactly
    // the shape assertsInt()/assertsVi() want. `irq_ == IrqJumper::None` (the default) is
    // SW1-8 off: IRQ* is asked for but nothing is soldered to it, same as the front
    // panel's own unclaimed lines. See docs/devguide/adding-a-board.md's interrupt section
    // for why intChanged() must be called from every place assertsInt()/assertsVi() could
    // move -- here, that is every ACRTC register or FIFO write and read.
    bool    assertsInt() const override { return irq_ == IrqJumper::Int && acrtc_.irq(); }
    uint8_t assertsVi() const override { return acrtc_.irq() ? viBit(irq_) : 0; }

    // SNAPSHOT/RESTORE (DESIGN.md 13): both chips, frame memory included -- no memory board
    // holds it, so it must travel here.
    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    std::vector<Property> properties() override;
    std::vector<MapEntry> ioMap() const override;

    // The host video service, wired once in main.cpp / tests/main.cpp (like
    // DazzlerBoard::setDisplay). Borrowed; the composition root owns it.
    static void setDisplay(Display* d);

    // ---- The monitor: one VESA mode, in the board's own memory-cycle units ----
    //
    // Horizontal figures are memory cycles of 16 pixels (single access mode); vertical
    // are rasters. Where the VESA pixel counts are not multiples of 16 the board rounds
    // its porches to whole cycles and keeps the total, as a real card's timing PROM
    // would; the pixel clocks are the VESA ones.
    struct Mode {
        const char* name;
        int width, height;         // the active picture, in pixels
        int hsw, hbp, hfp;         // sync, back porch, front porch: memory cycles
        int vsw, vbp, vfp;         // sync, back porch, front porch: rasters
        long long pclk;            // the pixel clock, Hz (2CLK is PCLK/8 or PCLK/4)
        int hc() const { return hsw + hbp + width / 16 + hfp; }   // total cycles per line
        int vc() const { return vsw + vbp + height + vfp; }       // total rasters per frame
    };
    static const Mode& mode(int i);
    static int         modeCount();
    const Mode&        currentMode() const { return mode(mode_); }

    // The programmed picture as the board sees it: pixels wide (HDW memory cycles at 16 or
    // 8 pixels each by access mode), rasters tall (the enabled SP0/SP1/SP2), and where its
    // top-left corner lands in the monitor's frame (may be negative or beyond the frame).
    int programmedWidth() const;
    int programmedHeight() const;
    int programmedX() const;
    int programmedY() const;

    // What SHOW's `wiring` says: "ok", or which of GBM / GAI / ACM disagrees with the board.
    std::string wiring() const;

    // The ACRTC's 2CLK in Hz: the mode's pixel clock over 8 (MODE AMODE single) or 4
    // (interleaved).
    long long twoClkHz() const;

    // ---- For tests: the chips themselves ----
    Hd63484& acrtc() { return acrtc_; }
    Bt453&   dac() { return dac_; }

private:
    void render();
    void paintFrame(Surface* s, int w, int h);

    // Drawing time: bring the ACRTC's 2CLK count up to the Clock, at the rate in force
    // since the last sync; then (re)arm the deadline at the end of whatever it is drawing.
    // Anything that changes the 2CLK rate (MODE AMODE, `mode`) syncs BEFORE it changes it.
    void sync();
    void arm();
    void clockAttached() override;

    // The board's own notion of access mode -- from the MODE register (BASE+1), not the
    // ACRTC's OMR. This is what the shift register (paintFrame, programmedWidth/X) runs on;
    // `wiring()` is what compares it against the chip's own OMR ACM bit.
    int glueAccessMode() const { return (modeReg_ & kModeAmode) ? 2 : 1; }

    Hd63484 acrtc_;
    Bt453   dac_;

    // MODE register bits (BASE+1) -- see the header comment for why these positions.
    static constexpr uint8_t kModeHspol = 0x01;
    static constexpr uint8_t kModeVspol = 0x02;
    static constexpr uint8_t kModeAmode = 0x04;
    static constexpr uint8_t kModeOlen  = 0x08;

    // 2 MB (1 M sixteen-bit words) -- the reference design's fixed SRAM fit; the ACRTC's
    // own 20-bit address space, exactly. Not a strap: the real card has no jumper for it.
    static constexpr size_t kVramWords = 1u << 20;

    // ---- Straps ----
    uint8_t   port_ = 0x70;       // the 8-port block's BASE; a multiple of 8
    int       mode_ = 2;          // index into the VESA mode table: 1024x768
    int       videoWidth_ = 0;    // host window width in px, 0 = auto
    IrqJumper irq_ = IrqJumper::None;   // SW1-8: where IRQ* lands, if anywhere
    bool      drawReal_ = false;  // draw_rate: false = full (instant), true = real (Table 3)

    // ---- Runtime state written by the guest, not a strap ----
    uint8_t modeReg_ = 0;         // MODE register (BASE+1): HSPOL/VSPOL/AMODE/OLEN

    // ---- Render bookkeeping ----
    bool dirty_ = true;           // something in the picture moved since the last frame

    // ---- Drawing time: the ACRTC's 2CLK count, kept in step with the Clock ----
    uint64_t      lastT_ = 0;     // the Clock's T-state at the last sync
    uint64_t      acc2clk_ = 0;   // 2CLK cycles since power-on
    uint64_t      rem_ = 0;       // the fraction carried: (T-states x 2CLK Hz) mod CPU Hz
    Clock::Handle wake_ = Clock::kNone;   // the end of the command being drawn
};

} // namespace altair
