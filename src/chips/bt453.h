#pragma once
//
// Brooktree Bt453 -- a 256 x 24 color-palette RAMDAC. A CHIP, NOT A CARD, exactly as
// the FD1771 and the 8257 next door are. The cadzilla video board has one between its
// ACRTC and the monitor; the next board that turns up with a Bt453 gets it for free.
// Modeled from the Brooktree data sheet (reference/Brooktree Bt453 RAMDAC.md), NOT from
// any one program that loads it.
//
// It knows nothing about S-100 and nothing about the board's ports. The CARD decodes
// four consecutive I/O locations onto some base and forwards each as C1C0 (0..3); the
// chip holds the LOOK-UP TABLE and the address logic, nothing more.
//
// ---------------------------------------------------------------------------
// THE MPU SIDE (data sheet Tables 1 and 2)
//
//   C1C0 = 00   address register           C1C0 = 01   color palette RAM
//   C1C0 = 10   address register (SAME)    C1C0 = 11   overlay registers
//
// A color is THREE byte cycles -- red, green, blue -- through a hidden modulo-3 phase
// counter (ADDRa,b) that the MPU cannot read. The rules, each of which a driver can
// trip over and each of which the tests pin down:
//
//   - A WRITE to the address register resets the phase to red. A READ of it does not.
//   - The 24-bit word lands in the addressed entry on the BLUE cycle; a sequence cut
//     short after red or green has written nothing.
//   - After the blue cycle (read OR write) the address increments, so a full 256-entry
//     load is one address write and 768 data writes. Location $FF wraps to $00.
//   - Overlay addressing uses ADDR1..0 only: 01/10/11 are overlay colors 1-3 and 00 is
//     "reserved". The sheet does not say what a write to reserved does; here it is a
//     fourth register nothing displays, so a driver that touches it neither corrupts a
//     real entry nor is refused.
//
// THE VIDEO SIDE is one function: lookup(p, ol) is Table 3, the 24-bit color the DACs
// would put on the wire for pixel value p under overlay select ol. The board hands
// palette() -- all 256 entries -- to Display::setPalette(), which is the whole reason an
// Indexed8 surface plus a palette is the right shape for a RAMDAC board: the SURFACE
// carries P0-P7 and the PALETTE is this chip. Nothing analog is modeled (sync, blank,
// current levels, the CS*-blanks-video artifact) -- the MPU cannot observe any of it.
//
// There is NO read-mask register and NO command register on this part; those are the
// Bt458/471/476. A driver written for one of those and pointed here is writing the
// address register at C1C0=10, which is what the silicon would do too.
// ---------------------------------------------------------------------------

#include "host/display.h"  // Color -- the 24-bit word the DACs resolve to

#include <array>
#include <cstdint>

namespace altair {

class StateWriter;  // core/statefile.h -- SNAPSHOT/RESTORE
class StateReader;

class Bt453 {
public:
    Bt453();

    // ---- THE FOUR LOCATIONS (C1C0 = 0..3). The card decodes the base and hands us the
    //      two low address bits. Both are the MPU's own cycles: a read advances the
    //      phase exactly as a write does. ----
    uint8_t read(uint8_t c);
    void    write(uint8_t c, uint8_t v);

    // Power-on. The data sheet defines no reset pin and no reset state for the RAM --
    // a real part comes up with garbage and a driver loads it. We come up BLACK (every
    // entry 0), which is the one deterministic choice, and with the address at 0/red.
    void reset();

    // ---- THE VIDEO SIDE ----

    // Table 3: the color the DACs drive for pixel value `p` under overlay select `ol`
    // (OL1..0). ol == 0 -> palette RAM entry p; ol == 1..3 -> overlay color 1..3, and
    // P0-P7 are ignored.
    Color lookup(uint8_t p, uint8_t ol = 0) const;

    // All 256 palette entries, in order -- what a board hands Display::setPalette().
    std::array<Color, 256> palette() const;

    // Has the look-up table changed since this was last asked? A board repaints its
    // palette (not its pixels) when it has. Consuming.
    bool takeDirty() {
        bool d = dirty_;
        dirty_ = false;
        return d;
    }

    // ---- For tests: the address logic, straight ----
    uint8_t address() const { return addr_; }
    uint8_t phase() const { return phase_; }  // 0 red, 1 green, 2 blue

    // SNAPSHOT/RESTORE (DESIGN.md 13): the RAM, the overlays, the address and the phase.
    void serialize(StateWriter& w) const;
    void deserialize(StateReader& r);

private:
    // One R/G/B byte cycle against the palette RAM (c == 1) or the overlays (c == 3).
    uint8_t  cycleRead(bool overlay);
    void     cycleWrite(bool overlay, uint8_t v);
    void     advance(bool overlay);  // the phase, and after blue the address

    uint8_t ram_[256][3] = {};   // color palette RAM: [entry][R,G,B]
    uint8_t ovl_[4][3]   = {};   // overlay registers: [0] reserved, [1..3] overlay 1..3
    uint8_t addr_        = 0;    // ADDR0-7
    uint8_t phase_       = 0;    // ADDRa,b -- 0 red, 1 green, 2 blue
    uint8_t pending_[3]  = {};   // the red and green bytes of a write in progress
    bool    dirty_       = true; // a fresh chip owes the host its (black) palette once
};

} // namespace altair
