#include "test.h"

#include "chips/hd63484.h"
#include "core/statefile.h"

#include <cstdint>
#include <vector>

using namespace altair;

// The HD63484 ACRTC (chips/hd63484.h) driven through its two host locations in 8-bit
// MPU mode, with no board and no bus. Every expected value is derived from the Hitachi
// datasheet (reference/Hitachi HD63484 ACRTC.md) or the User's Manual -- the worked
// examples on its command pages where one exists -- never from running another model.

namespace {

// A chip with 4K words of frame memory: small enough that a test can reason about an
// address, big enough for a 64-pixel-wide 4 bpp screen a few rasters tall.
struct Rig {
    Hd63484 c{4096};

    // ---- the host's view: RS=0 is AR (write) / SR (read), RS=1 is data ----
    void    ar(uint8_t a) { c.write(false, a); }
    uint8_t sr() { return c.read(false); }
    void    data(uint8_t v) { c.write(true, v); }
    uint8_t data() { return c.read(true); }

    // A 16-bit control register in 8-bit mode: even AR = high byte, odd = low byte.
    // r80-rFF auto-increment so one AR write covers both; r02-r06 need two.
    void reg(uint8_t even, uint16_t v) {
        ar(even);
        data((uint8_t)(v >> 8));
        if (even < 0x80) ar((uint8_t)(even | 1));
        data((uint8_t)v);
    }

    // A command or parameter word through the write FIFO: AR = 0, high byte, low byte.
    void word(uint16_t w) {
        ar(0);
        data((uint8_t)(w >> 8));
        data((uint8_t)w);
    }
    void cmd(uint16_t op, std::initializer_list<uint16_t> params = {}) {
        word(op);
        for (uint16_t p : params) word(p);
    }
    // A word out of the read FIFO, high byte then low.
    uint16_t readWord() {
        ar(0);
        uint16_t hi = data();
        uint16_t lo = data();
        return (uint16_t)((hi << 8) | lo);
    }

    // ---- a ready-made 4 bpp screen: MW = $10 words = 64 pixels; origin on screen 1 ----
    // ORG at word $100 (raster 16 counting from the start address), dot 0. +Y is the
    // raster ABOVE, i.e. $10 words lower.
    static constexpr uint32_t kOrg = 0x100;
    void screen4bpp() {
        reg(0x02, 0x0200);            // CCR: GBM = 010 -> 4 bpp, ABT clear
        reg(0xCA, 0x0010);            // MWR1: base screen, 16 words per raster
        cmd(0x0400, {0x4000, (uint16_t)(kOrg << 4)});   // ORG: DN = 01 (base), DPA = $100, DPD = 0
        cmd(0x0801, {0xFFFF});        // WPR CL1 = all ones in every field
        cmd(0x0800, {0x0000});        // WPR CL0 = zero
        cmd(0x1800, {4, 0xFFFF, 0xFFFF});   // WPTN pattern rows 0-1 all ones (n = 2 words x 2)
        cmd(0x0806, {0x0000});        // PRC: PSX = PSY = 0
        cmd(0x0807, {0x00F0});        // PRC: PEX = 15, PEY = 0, no zoom
        cmd(0x0805, {0x0000});        // PRC: PPX = PPY = 0
    }
    // The word holding pixel (x, y) of that screen, and the nibble for it.
    uint32_t wordAt(int x, int y) const { return (uint32_t)((int)kOrg + x / 4 - y * 0x10); }
    int      nib(int x, int y) const { return (c.peekWord(wordAt(x, y)) >> ((x % 4) * 4)) & 0xF; }
};

} // namespace

void test_hd63484() {
    SECTION("HD63484 -- reset: SR = $23, CCR ABT set, OMR STR clear, FIFOs empty");
    {
        Rig g;
        CHECK(g.sr() == 0x23, "SR after RES*: CED | WFR | WFE (manual 5.3)");
        CHECK(g.c.ccr() == 0x8000, "CCR: ABT = 1, everything else 0 (manual 5.5)");
        CHECK((g.c.omr() & 0xC000) == 0, "OMR: M/S and STR reset (manual 5.6)");
        CHECK(g.c.writeFifoWords() == 0 && g.c.readFifoWords() == 0, "both FIFOs empty");
        CHECK(!g.c.displayOn(), "and nothing is displayed until STR");
        CHECK(g.c.takeDirty() && !g.c.takeDirty(), "a fresh chip owes one frame");
    }

    SECTION("HD63484 -- 8-bit register access: even AR high byte, odd AR low byte");
    {
        Rig g;
        // r80-rFF auto-increment (manual 5.2): one AR write, two data writes.
        g.ar(0x84);
        g.data(0x12);
        g.data(0x34);
        CHECK(g.c.reg(0x84) == 0x1234, "HDR written high then low through the auto-increment");
        CHECK(g.c.ar() == 0x86, "AR advanced by one per byte");
        g.ar(0x84);
        CHECK(g.data() == 0x12 && g.data() == 0x34, "and reads back the same way");

        // r00-r7F do NOT auto-increment: the same byte again.
        g.ar(0x04);
        g.data(0x40);                 // OMR high: STR
        g.data(0x40);                 // still OMR high
        CHECK(g.c.ar() == 0x04, "AR stays put below r80");
        CHECK((g.c.omr() >> 8) == 0x40 && (g.c.omr() & 0xFF) == 0, "only the high byte moved");
        g.ar(0x05);
        g.data(0x02);                 // OMR low: ACM
        CHECK(g.c.omr() == 0x4002, "the odd AR reaches the low byte");
    }

    SECTION("HD63484 -- FIFO and status: CED clears on a command, sets on its end; CER on nonsense; ABT");
    {
        Rig g;
        g.cmd(0x0400, {0x0000, 0x0000});          // ORG, three words, executes at once
        CHECK((g.sr() & 0x20) != 0, "CED set: the command ended");
        CHECK((g.sr() & 0x03) == 0x03, "WFE and WFR: the write FIFO drained (drawing is instant)");

        g.word(0x0400);                           // ORG, but only the opcode
        CHECK((g.sr() & 0x20) == 0, "CED cleared: a command is in progress, waiting for parameters");
        g.word(0);
        g.word(0);
        CHECK((g.sr() & 0x20) != 0, "the last parameter ends it");

        g.word(0x0000);                           // no such opcode
        CHECK((g.sr() & 0x80) != 0, "CER: an undefined command");
        CHECK((g.sr() & 0x20) != 0, "...and the chip is free again (CED)");
        g.cmd(0x0801, {0x1234});
        CHECK((g.sr() & 0x80) != 0, "CER is sticky through later commands");
        g.reg(0x02, 0x8000);                      // ABT
        CHECK(g.sr() == 0x23, "ABT: SR is $23 again (manual 5.5)");
        CHECK(g.c.readFifoWords() == 0, "and the FIFOs are cleared");
    }

    SECTION("HD63484 -- WPR/RPR: the manual's example, $1111 into CL1 and back through the read FIFO");
    {
        Rig g;
        g.cmd(0x0801, {0x1111});                  // WPR (RN = 01) $1111  (manual WPR-2)
        CHECK(g.c.param(1) == 0x1111, "CL1 holds $1111");
        CHECK((g.sr() & 0x04) == 0, "nothing in the read FIFO yet");
        g.cmd(0x0C01);                            // RPR (RN = 01)          (manual RPR-2)
        CHECK((g.sr() & 0x04) != 0, "RFR: the read FIFO has a word");
        CHECK(g.readWord() == 0x1111, "and it is CL1, high byte first");
        CHECK((g.sr() & 0x04) == 0, "RFR clears once it is drained");
        g.cmd(0x0810, {1});                       // WPR to Pr10 (DP, read-only)
        CHECK((g.sr() & 0x80) != 0, "writing a read-only parameter register is a command error");
    }

    SECTION("HD63484 -- WPTN/RPTN: the manual's example, $2314 $5713 at pattern address $B");
    {
        Rig g;
        // 8-bit mode: n counts BYTES (manual WPTN-1), so two words is n = 4.
        g.cmd(0x180B, {4, 0x2314, 0x5713});
        CHECK(g.c.pattern(0xB) == 0x2314 && g.c.pattern(0xC) == 0x5713, "two pattern words landed at $B, $C");
        CHECK((g.sr() & 0x20) != 0, "CED after the last data word");
        g.cmd(0x1C0B, {2});                       // RPTN (PRA = $B) n = 2
        CHECK(g.readWord() == 0x2314 && g.readWord() == 0x5713, "read back in order, high byte first (manual RPTN-2)");
    }

    SECTION("HD63484 -- RWP, WT, RD, MOD: one word at a time, RWP incrementing");
    {
        Rig g;
        g.cmd(0x080C, {0x0000});                  // RWP high: DN = 00, RWPH = 0   (manual CLR-3 example)
        g.cmd(0x080D, {0x0560});                  // RWP low:  RWPL = $056 -> address $56
        CHECK(g.c.param(0x0D) == 0x0560 && g.c.param(0x0C) == 0, "RWP reads back as written");
        g.cmd(0x4800, {0x5555});                  // WT $5555
        CHECK(g.c.peekWord(0x56) == 0x5555, "WT put the word at RWP");
        CHECK(g.c.param(0x0D) == 0x0570, "and RWP incremented to $57");
        g.cmd(0x4800, {0xAAAA});
        CHECK(g.c.peekWord(0x57) == 0xAAAA, "the next WT lands at $57");

        g.cmd(0x080D, {0x0560});
        g.cmd(0x4400);                            // RD
        CHECK(g.readWord() == 0x5555, "RD reads the word at RWP into the read FIFO");
        CHECK(g.c.param(0x0D) == 0x0570, "and increments RWP");

        // MOD under MASK (manual 6.5.1, Figure 6.5(b)): only masked bits take the OR.
        g.cmd(0x0804, {0x0FF0});                  // MASK
        g.cmd(0x080D, {0x0560});
        g.cmd(0x4C01, {0xFFFF});                  // MOD (MM = 01, OR) $FFFF
        CHECK(g.c.peekWord(0x56) == 0x5FF5, "bits under the mask ORed, the rest untouched");
        g.cmd(0x080D, {0x0560});
        g.cmd(0x4C03, {0x0FF0});                  // EOR
        CHECK(g.c.peekWord(0x56) == 0x5005, "EOR under the mask");
    }

    SECTION("HD63484 -- CLR: the manual's example, 5 x 7 words of $1111 from RWP $56, negative AX/AY");
    {
        Rig g;
        g.reg(0xC2, 0x0010);                      // MWR0: screen 0, MW = $10
        g.cmd(0x080C, {0x0000});
        g.cmd(0x080D, {0x0560});                  // RWP = $56 on screen 0
        g.cmd(0x5800, {0x1111, 0xFFFC, 0xFFFA});  // CLR $1111, AX = -4, AY = -6  (manual CLR-3)
        // AX = -4: five words leftward, $52..$56. AY = -6: seven rasters DOWNWARD in Y,
        // which is UPWARD in memory: $56, $66, ... $B6 (manual CLR-4: "7 words", Pc $B2).
        bool all = true;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (g.c.peekWord((uint32_t)(0x56 - col + row * 0x10)) != 0x1111) all = false;
        CHECK(all, "35 words of $1111 from $52-$56 through $B2-$B6");
        CHECK(g.c.peekWord(0x51) == 0 && g.c.peekWord(0x57) == 0 && g.c.peekWord(0xC6) == 0,
              "and nothing beside or beyond the block");
        CHECK(g.c.param(0x0D) == 0x0B60, "RWPe is $B6 (manual CLR-4)");
    }

    SECTION("HD63484 -- ORG, AMOVE and DOT: a pixel's word and dot address (manual ORG-3)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {1, 0});                    // AMOVE (1, 0)
        CHECK(g.c.cpx() == 1 && g.c.cpy() == 0, "CP moved");
        g.cmd(0xCC00);                            // DOT, COL 00 OPM replace
        CHECK(g.c.peekWord(Rig::kOrg) == 0x00F0, "pixel x = 1 at 4 bpp is bits 4-7: dot 1 is the SECOND nibble up");
        g.cmd(0x8000, {4, 0});
        g.cmd(0xCC00);
        CHECK(g.c.peekWord(Rig::kOrg + 1) == 0x000F, "x = 4 is dot 0 of the NEXT word");
        g.cmd(0x8000, {0, 1});
        g.cmd(0xCC00);
        CHECK(g.c.peekWord(Rig::kOrg - 0x10) == 0x000F, "+Y is the raster above: MW words lower");
        g.cmd(0x8000, {(uint16_t)-1, 0});         // x = -1
        g.cmd(0xCC00);
        CHECK(g.c.peekWord(Rig::kOrg - 1) == 0xF000, "x = -1 is the top nibble of the word before");

        // ORG with a dot offset: DPD = 4 at 4 bpp is dot 1, "bit position 4-7" (manual ORG-3).
        g.cmd(0x0400, {0x4000, (uint16_t)((Rig::kOrg << 4) | 4)});
        g.cmd(0x8000, {0, 0});
        g.cmd(0xCC00);
        CHECK((g.c.peekWord(Rig::kOrg) & 0x00F0) == 0x00F0, "the origin itself now sits at bits 4-7");
    }

    SECTION("HD63484 -- ALINE: draws from CP toward Pe, Pe itself NOT drawn, CP = Pe (manual ALINE-1)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8800, {8, 0});                    // ALINE (0,0) -> (8,0)
        CHECK(g.c.peekWord(Rig::kOrg) == 0xFFFF && g.c.peekWord(Rig::kOrg + 1) == 0xFFFF,
              "pixels 0-7 are drawn");
        CHECK(g.c.peekWord(Rig::kOrg + 2) == 0x0000, "pixel 8, the end point, is not");
        CHECK(g.c.cpx() == 8 && g.c.cpy() == 0, "CP is at Pe");

        g.cmd(0x8800, {8, 3});                    // straight up to (8,3)
        CHECK(g.nib(8, 0) == 0xF && g.nib(8, 1) == 0xF && g.nib(8, 2) == 0xF, "(8,0),(8,1),(8,2) drawn");
        CHECK(g.nib(8, 3) == 0, "(8,3) not");

        g.cmd(0x8C00, {(uint16_t)-4, 0});         // RLINE dX = -4: leftward from (8,3)
        CHECK(g.nib(8, 3) == 0xF && g.nib(5, 3) == 0xF && g.nib(4, 3) == 0, "relative, leftward, end excluded");
        CHECK(g.c.cpx() == 4 && g.c.cpy() == 3, "CP followed");
    }

    SECTION("HD63484 -- a dashed line: the pattern scans from PPX along the line (manual 6.8.3)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x1800, {2, 0x0005});               // pattern row 0 = ...0101: bit 0 on, bit 1 off
        g.cmd(0x0807, {0x0010});                  // PEX = 1: a two-bit pattern
        g.cmd(0x8800, {8, 0});                    // COL 00: bit 1 -> CL1 (F), bit 0 -> CL0 (0)
        CHECK(g.c.peekWord(Rig::kOrg) == 0x0F0F && g.c.peekWord(Rig::kOrg + 1) == 0x0F0F,
              "on, off, on, off... from PPX = 0");

        g.cmd(0x0805, {0x0010});                  // PPX = 1: start on the off bit
        g.cmd(0x8000, {0, 1});
        g.cmd(0x8800, {8, 1});
        CHECK(g.c.peekWord(Rig::kOrg - 0x10) == 0xF0F0, "starting at PPX = 1 inverts the phase");

        g.cmd(0x0805, {0x0000});
        g.cmd(0x8000, {0, 2});
        g.c.pokeWord(Rig::kOrg - 0x20, 0x2222);   // something already there
        g.cmd(0x8808, {8, 2});                    // COL 01 (bits 4-3): bit 0 -> suppressed, not CL0
        CHECK(g.c.peekWord(Rig::kOrg - 0x20) == 0x2F2F, "COL 01 leaves the off pixels as they were");
    }

    SECTION("HD63484 -- ARCT: the perimeter, X first, every corner once, CP back where it was");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x9000, {3, 2});                    // ARCT to (3,2) from (0,0)
        CHECK(g.c.peekWord(Rig::kOrg) == 0xFFFF, "bottom edge: (0..3, 0)");
        CHECK(g.c.peekWord(Rig::kOrg - 0x20) == 0xFFFF, "top edge: (0..3, 2)");
        CHECK(g.c.peekWord(Rig::kOrg - 0x10) == 0xF00F, "sides: (0,1) and (3,1) only");
        CHECK(g.c.cpx() == 0 && g.c.cpy() == 0, "CP is unchanged (manual ARCT-1)");

        // OPM = 011 (EOR): drawing the same rectangle again erases it, which also proves
        // no pixel was drawn twice.
        g.cmd(0x9003, {3, 2});
        CHECK(g.c.peekWord(Rig::kOrg) == 0 && g.c.peekWord(Rig::kOrg - 0x10) == 0 &&
                  g.c.peekWord(Rig::kOrg - 0x20) == 0,
              "EOR twice is nothing: each perimeter pixel was drawn exactly once");
    }

    SECTION("HD63484 -- the operation modes (manual 6.6.1): OR, AND, and the conditional replaces");
    {
        Rig g;
        g.screen4bpp();
        g.c.pokeWord(Rig::kOrg, 0x0003);          // pixel 0 = 3
        g.cmd(0x0801, {0x5555});                  // CL1 = 5 in every field
        g.cmd(0xCC01);                            // DOT, OR
        CHECK(g.nib(0, 0) == 7, "3 | 5 = 7");
        g.cmd(0xCC02);                            // AND
        CHECK(g.nib(0, 0) == 5, "7 & 5 = 5");
        g.cmd(0x0802, {0x5555});                  // CCMP = 5
        g.cmd(0x0801, {0x9999});                  // CL1 = 9
        g.cmd(0xCC05);                            // replace if P != CCMP: P is 5, so no
        CHECK(g.nib(0, 0) == 5, "P == CCMP: conditional replace (P != CCMP) leaves it");
        g.cmd(0xCC04);                            // replace if P == CCMP
        CHECK(g.nib(0, 0) == 9, "P == CCMP: replaced with the color");
        g.cmd(0x0801, {0x4444});                  // CL1 = 4
        g.cmd(0xCC07);                            // replace if P > CL: 9 > 4
        CHECK(g.nib(0, 0) == 4, "P > CL: replaced");
        g.cmd(0x0801, {0x2222});
        g.cmd(0xCC07);                            // 4 > 2
        CHECK(g.nib(0, 0) == 2, "again");
        g.cmd(0xCC06);                            // replace if P < CL: 2 < 2 is false
        CHECK(g.nib(0, 0) == 2, "P < CL false: untouched");
    }

    SECTION("HD63484 -- AREA modes (manual 6.6.3): stop with ARD+CED, or suppress with/without ARD");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x0808, {0});                       // XMIN = 0
        g.cmd(0x080A, {4});                       // XMAX = 4
        g.cmd(0x0809, {0});                       // YMIN
        g.cmd(0x080B, {8});                       // YMAX

        g.cmd(0x8800 | 0x20, {10, 0});            // AREA = 001: stop on exit
        CHECK(g.nib(4, 0) == 0xF && g.nib(5, 0) == 0, "pixels 0-4 drawn, drawing stopped at x = 5");
        CHECK((g.sr() & 0x40) != 0, "ARD set");
        CHECK((g.sr() & 0x20) != 0, "and CED: the command was terminated");
        g.cmd(0x0C00);                            // any RPR
        g.readWord();
        CHECK((g.sr() & 0x40) == 0, "RPR clears ARD (manual Table 5.1)");

        g.cmd(0x8000, {0, 1});
        g.cmd(0x8800 | 0x40, {10, 1});            // AREA = 010: suppress outside, no ARD
        CHECK(g.nib(4, 1) == 0xF && g.nib(5, 1) == 0 && g.nib(9, 1) == 0, "outside pixels suppressed");
        CHECK((g.sr() & 0x40) == 0, "and ARD stays clear");
        CHECK(g.c.cpx() == 10, "but the line ran to its end: CP = Pe");

        g.cmd(0x8000, {0, 2});
        g.cmd(0x8800 | 0x60, {10, 2});            // AREA = 011: suppress, and say so
        CHECK((g.sr() & 0x40) != 0, "AREA 011 sets ARD when the pointer leaves the area");

        g.cmd(0x8000, {6, 3});
        g.cmd(0x8800 | 0xC0, {0, 3});             // AREA = 110: suppress INSIDE
        CHECK(g.nib(6, 3) == 0xF && g.nib(5, 3) == 0xF && g.nib(4, 3) == 0 && g.nib(1, 3) == 0,
              "drawn outside the area only");
    }

    SECTION("HD63484 -- AFRCT tiles the pattern over the rectangle; CP ends one raster past it");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x1800, {2, 0x0005});               // pattern row 0 = 0101
        g.cmd(0x0807, {0x0030});                  // PEX = 3, PEY = 0: a 4x1 pattern
        g.cmd(0xC000, {3, 1});                    // AFRCT to (3,1): 4 x 2 pixels
        CHECK(g.c.peekWord(Rig::kOrg) == 0x0F0F, "row 0: on off on off");
        CHECK(g.c.peekWord(Rig::kOrg - 0x10) == 0x0F0F, "row 1 the same (the pattern is one row)");
        CHECK(g.c.peekWord(Rig::kOrg - 0x20) == 0, "row 2 untouched");
        CHECK(g.c.cpx() == 0 && g.c.cpy() == 2, "Pe = (A, Y+1) (manual AFRCT-1)");
    }

    SECTION("HD63484 -- APLL/APLG: a polyline moves CP to its end; a polygon closes on CP");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x9800, {2, 3, 0, 3, 2});           // APLL n=2: (0,0)->(3,0)->(3,2)
        CHECK(g.nib(0, 0) == 0xF && g.nib(2, 0) == 0xF && g.nib(3, 0) == 0xF && g.nib(3, 1) == 0xF,
              "both segments drawn");
        CHECK(g.nib(3, 2) == 0, "the final end point is not");
        CHECK(g.c.cpx() == 3 && g.c.cpy() == 2, "CP at the end");

        Rig h;
        h.screen4bpp();
        h.cmd(0xA000, {2, 3, 0, 3, 2});           // APLG: the same, plus the closing edge back to (0,0)
        // The closing edge (3,2)->(0,0) steps (3,2), (2,1), (1,1) and excludes (0,0).
        CHECK(h.nib(3, 2) == 0xF && h.nib(2, 1) == 0xF && h.nib(1, 1) == 0xF, "the closing edge is drawn");
        CHECK(h.c.cpx() == 0 && h.c.cpy() == 0, "CP stays at the polygon's start");
    }

    SECTION("HD63484 -- DWT under program control: the data words follow through the write FIFO");
    {
        Rig g;
        g.reg(0xCA, 0x0010);                      // MWR1
        g.cmd(0x080C, {0x4000});                  // RWP: DN = 01 (base), RWPH = 0
        g.cmd(0x080D, {0x1000});                  // RWPL = $100
        g.cmd(0x2800, {1, 1});                    // DWT AX = 1, AY = 1: a 2 x 2 block
        CHECK((g.sr() & 0x20) == 0, "CED clear: the chip is waiting for four data words");
        g.word(0xAAAA);
        g.word(0xBBBB);
        g.word(0xCCCC);
        CHECK((g.sr() & 0x20) == 0, "still waiting after three");
        g.word(0xDDDD);
        CHECK((g.sr() & 0x20) != 0, "CED after the fourth");
        CHECK(g.c.peekWord(0x100) == 0xAAAA && g.c.peekWord(0x101) == 0xBBBB, "first raster at RWP");
        CHECK(g.c.peekWord(0x0F0) == 0xCCCC && g.c.peekWord(0x0F1) == 0xDDDD, "second raster one MW lower (+Y)");
        CHECK(g.c.param(0x0D) == 0x0F00, "RWPe on the last raster");
    }

    SECTION("HD63484 -- DRD under program control: words appear in the read FIFO; only ABT ends it");
    {
        Rig g;
        g.reg(0xCA, 0x0010);
        g.c.pokeWord(0x100, 0x1234);
        g.c.pokeWord(0x101, 0x5678);
        g.cmd(0x080C, {0x4000});
        g.cmd(0x080D, {0x1000});
        g.cmd(0x2400, {1, 0});                    // DRD AX = 1, AY = 0: two words
        CHECK(g.c.readFifoWords() == 2, "both words are already in the read FIFO");
        CHECK(g.readWord() == 0x1234 && g.readWord() == 0x5678, "in order");
        CHECK((g.sr() & 0x20) == 0, "CED stays clear: 'an indefinite wait state' (manual 6.5)");
        g.reg(0x02, 0x8000);                      // ABT
        CHECK((g.sr() & 0x20) != 0, "ABT ends it");

        // Bigger than the FIFO: it refills as the host drains.
        for (uint32_t i = 0; i < 12; ++i) g.c.pokeWord(0x100 + i, (uint16_t)(0x100 + i));
        g.reg(0x02, 0x0000);
        g.cmd(0x080C, {0x4000});
        g.cmd(0x080D, {0x1000});
        g.cmd(0x2400, {11, 0});                   // 12 words, FIFO holds 8
        CHECK(g.c.readFifoWords() == 8 && (g.sr() & 0x08) != 0, "the FIFO fills to 8 words -- RFF");
        bool inOrder = true;
        for (uint32_t i = 0; i < 12; ++i)
            if (g.readWord() != (uint16_t)(0x100 + i)) inOrder = false;
        CHECK(inOrder, "all twelve come out in order as space frees up");
    }

    SECTION("HD63484 -- an unimplemented but valid opcode consumes its parameters and reports CER");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0xA800, {5});                       // CRCL r = 5: recognized, not drawn
        CHECK((g.sr() & 0x80) != 0, "CER");
        CHECK((g.sr() & 0x20) != 0, "CED: free again");
        g.cmd(0x8000, {2, 0});                    // the next command is parsed as a command...
        CHECK(g.c.cpx() == 2, "...not as a stray parameter: the stream stayed in step");
    }

    SECTION("HD63484 -- scan-out: HDW, SP1, SAR1/MWR1, low dot address leftmost");
    {
        Rig g;
        g.reg(0x02, 0x0200);                      // 4 bpp
        g.reg(0x84, 0x0003);                      // HDS = 0, HDW = 3 -> 4 memory cycles
        g.reg(0x8A, 0x0002);                      // SP1 = 2 rasters
        g.reg(0xCA, 0x0010);                      // MWR1 = $10
        g.reg(0xCC, 0x0000);                      // SAR1 high
        g.reg(0xCE, 0x0100);                      // SAR1 low = $100
        CHECK(!g.c.displayOn(), "not yet: STR and SE1 are clear");
        g.reg(0x04, 0x4000);                      // OMR STR
        g.reg(0x06, 0x4000);                      // DCR SE1
        CHECK(g.c.displayOn(), "STR + SE1: displaying");
        CHECK(g.c.displayWidth() == 16, "4 cycles x 4 pixels per word at 4 bpp");
        CHECK(g.c.displayHeight() == 2, "SP1 rasters, upper and lower disabled");
        CHECK(g.c.bitsPerPixel() == 4, "GBM 010");

        g.c.pokeWord(0x100, 0x4321);
        g.c.pokeWord(0x103, 0x000F);
        g.c.pokeWord(0x110, 0x0008);
        std::vector<uint16_t> row(16, 0xEE);
        g.c.scanline(0, row);
        CHECK(row[0] == 1 && row[1] == 2 && row[2] == 3 && row[3] == 4, "word $100 = $4321: dot 0 (low nibble) is leftmost");
        CHECK(row[12] == 0xF && row[13] == 0, "word $103 is pixels 12-15");
        g.c.scanline(1, row);
        CHECK(row[0] == 8, "raster 1 starts at SAR + MW");
        CHECK(row[4] == 0, "and the rest is black");

        g.reg(0x06, 0x0000);                      // SE1 clear: base blanked
        g.c.scanline(0, row);
        CHECK(row[0] == 0, "a blanked screen scans out black");

        g.reg(0x06, 0x7000);                      // SE1 + SE0 = 11: an upper screen too
        g.reg(0x8C, 0x0001);                      // SP0 = 1 raster
        g.reg(0xC2, 0x0010);                      // MWR0
        g.reg(0xC4, 0x0000);
        g.reg(0xC6, 0x0200);                      // SAR0 = $200
        g.c.pokeWord(0x200, 0x0007);
        CHECK(g.c.displayHeight() == 3, "SP0 + SP1");
        g.c.scanline(0, row);
        CHECK(row[0] == 7, "raster 0 is now the upper screen's first raster");
        g.c.scanline(1, row);
        CHECK(row[0] == 1, "and the base screen begins below it");

        g.reg(0x02, 0x0300);                      // 8 bpp
        CHECK(g.c.displayWidth() == 8 && g.c.bitsPerPixel() == 8, "8 bpp halves the pixels per cycle");
        g.c.scanline(1, row);
        CHECK(row[0] == 0x21 && row[1] == 0x43, "at 8 bpp a word is two pixels, low byte first");
    }

    SECTION("HD63484 -- the window screen overlays the background (DCR SE3 = 11)");
    {
        Rig g;
        g.reg(0x02, 0x0200);                      // 4 bpp
        g.reg(0x84, 0x0003);                      // HDS = 0 (start at cycle 1), HDW = 3
        g.reg(0x88, 0x0001);                      // VDS = 0 (start at raster 1), VSW = 1
        g.reg(0x8A, 0x0004);                      // SP1 = 4
        g.reg(0xCA, 0x0010);
        g.reg(0xCC, 0x0000);
        g.reg(0xCE, 0x0100);                      // base at $100
        g.reg(0x92, 0x0100);                      // HWS = 1 (cycle 2), HWW = 0 (one cycle)
        g.reg(0x94, 0x0001);                      // VWS = 1 (raster 2)
        g.reg(0x96, 0x0002);                      // VWW = 2 rasters
        g.reg(0xDA, 0x0004);                      // MWR3 = 4
        g.reg(0xDC, 0x0000);
        g.reg(0xDE, 0x0300);                      // window at $300
        g.reg(0x04, 0x4000);
        g.reg(0x06, 0x4300);                      // SE1, SE3 = 11
        g.c.pokeWord(0x300, 0xABCD);
        g.c.pokeWord(0x304, 0x0001);
        std::vector<uint16_t> row(16, 0);
        g.c.scanline(0, row);
        CHECK(row[4] == 0, "raster 0 is above the window");
        g.c.scanline(1, row);
        CHECK(row[4] == 0xD && row[5] == 0xC && row[7] == 0xA, "raster 1: the window's first raster at cycle 2 (pixels 4-7)");
        CHECK(row[3] == 0 && row[8] == 0, "and only that cycle");
        g.c.scanline(2, row);
        CHECK(row[4] == 1, "raster 2: the window's second raster, one MW3 on");
        g.c.scanline(3, row);
        CHECK(row[4] == 0, "raster 3 is below it");
    }

    SECTION("HD63484 -- the picture is dirty when anything it depends on moves");
    {
        Rig g;
        g.screen4bpp();
        g.c.takeDirty();
        g.cmd(0x8000, {3, 3});                    // AMOVE draws nothing
        CHECK(!g.c.takeDirty(), "moving the pointer is not a change");
        g.cmd(0xCC00);
        CHECK(g.c.takeDirty(), "a DOT is");
        g.reg(0x8A, 0x0010);
        CHECK(g.c.takeDirty(), "a timing register is");
        g.reg(0x06, 0x4000);
        CHECK(g.c.takeDirty(), "DCR is");
        g.cmd(0x0801, {0x1234});
        CHECK(!g.c.takeDirty(), "a drawing parameter alone is not");
    }

    SECTION("HD63484 -- a snapshot carries the registers, the drawing state and the frame memory");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x9000, {3, 2});
        g.cmd(0x8000, {5, 6});
        g.reg(0x84, 0x0003);
        g.word(0x0400);                           // a command left half-issued
        g.word(0x0000);

        StateWriter w;
        g.c.serialize(w);
        Hd63484     d{4096};
        StateReader r(w.data());
        d.deserialize(r);
        CHECK(r.ok(), "reads back cleanly");
        CHECK(d.vram()[Rig::kOrg] == 0xFFFF && d.vram()[Rig::kOrg - 0x10] == 0xF00F, "the frame memory travelled");
        CHECK(d.cpx() == 5 && d.cpy() == 6, "and the current pointer");
        CHECK(d.reg(0x84) == 0x0003 && d.ccr() == 0x0200, "and the control registers");
        CHECK(d.param(1) == 0xFFFF && d.pattern(0) == 0xFFFF, "and the drawing parameters and pattern RAM");
        CHECK((d.status() & 0x20) == 0, "the half-issued ORG is still pending");
        d.write(false, 0);
        d.write(true, 0x00);
        d.write(true, 0x00);                      // its last parameter
        CHECK((d.status() & 0x20) != 0, "and completes on the restored chip");
        CHECK(d.takeDirty(), "a restored chip owes a frame");
    }
}
