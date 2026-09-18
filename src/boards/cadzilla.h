#pragma once
//
// cadzilla -- an HD63484 ACRTC graphics board with a Bt453 RAMDAC. A board of Todd's own
// design (2026); see docs/boards/cadzilla.md.
//
// TWO CHIPS ON SIX I/O PORTS, AND A FRAME MEMORY THE CPU NEVER SEES.
//
//   I/O     -- the ACRTC at BASE (default 0x70): BASE+0 is RS=0 (OUT: address register,
//              IN: status), BASE+1 is RS=1 (the 16-bit register the address names, one
//              byte per cycle, or the command FIFOs). The Bt453 at DAC (default 0x74): four
//              ports by C1C0 -- address register, color palette RAM, address register
//              again, overlay registers -- all read/write.
//   MEMORY  -- NONE decoded. The frame memory is DRAM on the board, private to the ACRTC;
//              the CPU draws by issuing commands through the FIFO, never by poking a byte.
//              The opposite reason from the Dazzler's for decoding no address.
//   DISPLAY -- once per pump() the board scans the ACRTC's visible raster out of its frame
//              memory into a Surface (one byte per pixel: the eight pixel inputs P0-P7 the
//              Bt453 has) and hands the host the Bt453's 256-entry look-up table as the
//              palette. Never touches SDL: the Display is injected at the composition root
//              (setDisplay), a headless build gets a NullDisplay, and the board runs and is
//              tested with no window (DESIGN.md 7.4).
//
// THE PIPELINE IS THE POINT. ACRTC frame memory -> 8-bit pixel value -> Bt453 LUT -> 24-bit
// RGB, and the Display seam is shaped exactly like it: an Indexed8 Surface IS the P0-P7
// bus, and setPalette() IS the RAMDAC. Nothing is translated; the board just wires them.
//
// WHAT THE BOARD DECIDED (a chip cannot): how much DRAM is fitted (`vram`), that the
// shift register serializes the LOW dot address first so the ACRTC's +X is rightward
// (chips/hd63484.h), that OL1..0 are tied low (no overlay source), that only P0-P7 reach
// the DAC (at 16 bpp the low byte of a pixel is what shows), and that IRQ* is not wired
// to the bus yet.

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

    std::string type() const override { return "cadzilla"; }

    bool    decodes(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;

    void reset(Reset r) override;
    void power() override;
    void pump() override;

    // SNAPSHOT/RESTORE (DESIGN.md 13): both chips, frame memory included -- no memory board
    // holds it, so it must travel here.
    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    std::vector<Property> properties() override;
    std::vector<MapEntry> ioMap() const override;

    // The host video service, wired once in main.cpp / tests/main.cpp (like
    // DazzlerBoard::setDisplay). Borrowed; the composition root owns it.
    static void setDisplay(Display* d);

    // ---- For tests: the chips themselves ----
    Hd63484& acrtc() { return acrtc_; }
    Bt453&   dac() { return dac_; }

private:
    void render();

    Hd63484 acrtc_;
    Bt453   dac_;

    // ---- Straps ----
    uint8_t port_ = 0x70;         // ACRTC: BASE (RS=0) and BASE+1 (RS=1); even
    uint8_t dacPort_ = 0x74;      // Bt453: four ports; a multiple of 4
    int     vramK_ = 128;         // frame memory fitted, in K words (power of two)
    int     videoWidth_ = 0;      // host window width in px, 0 = auto

    // ---- Render bookkeeping ----
    bool dirty_ = true;           // something in the picture moved since the last frame
    int  lastW_ = 0, lastH_ = 0;  // the last geometry drawn, so "off" can paint black
};

} // namespace altair
