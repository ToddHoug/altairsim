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
    bool     lit(int x, int y) const { return nib(x, y) != 0; }
    // Every lit pixel of the screen's x 0..63, y -16..16, for comparing whole drawings.
    std::vector<bool> picture() const {
        std::vector<bool> v;
        for (int y = -16; y <= 16; ++y)
            for (int x = 0; x < 64; ++x) v.push_back(lit(x, y));
        return v;
    }
    int count() const {
        int n = 0;
        for (bool b : picture()) n += b;
        return n;
    }
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
        CHECK(g.c.cpx() == 5 && g.c.cpy() == 0,
              "CP is left where the drawing crossed out, not at Pe (manual 6.6.3: 'as long as the CP resides in the area')");
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

    SECTION("HD63484 -- AREA 101 stops on ENTERING the area; 111 suppresses inside and sets ARD");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x0808, {4});                       // XMIN = 4
        g.cmd(0x080A, {8});                       // XMAX = 8
        g.cmd(0x0809, {0});
        g.cmd(0x080B, {8});

        g.cmd(0x8800 | 0xA0, {12, 0});            // AREA = 101: stop on entry
        CHECK(g.nib(3, 0) == 0xF && g.nib(4, 0) == 0 && g.nib(10, 0) == 0, "pixels 0-3 drawn, stopped at x = 4");
        CHECK((g.sr() & 0x60) == 0x60, "ARD and CED");
        CHECK(g.c.cpx() == 4 && g.c.cpy() == 0, "CP at the crossing");

        g.cmd(0x0C00);
        g.readWord();                             // RPR: clear ARD
        g.cmd(0x8000, {0, 1});
        g.cmd(0x8800 | 0xE0, {12, 1});            // AREA = 111: suppress inside, say so
        CHECK(g.nib(3, 1) == 0xF && g.nib(4, 1) == 0 && g.nib(8, 1) == 0 && g.nib(9, 1) == 0xF,
              "drawn on both sides, suppressed inside");
        CHECK((g.sr() & 0x40) != 0, "ARD set: the pointer went inside");
        CHECK(g.c.cpx() == 12, "and the line ran to Pe");
    }

    SECTION("HD63484 -- the pattern pointer is live: it carries across segments and commands");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x1800, {2, 0x0005});               // row 0 = ...0101
        g.cmd(0x0807, {0x0010});                  // PEX = 1: a two-bit dash
        g.cmd(0x9800, {2, 3, 0, 3, 3});           // APLL (0,0)->(3,0)->(3,3)
        CHECK(g.nib(0, 0) == 0xF && g.nib(1, 0) == 0 && g.nib(2, 0) == 0xF, "first segment: on off on");
        CHECK(g.nib(3, 0) == 0 && g.nib(3, 1) == 0xF && g.nib(3, 2) == 0,
              "the second segment continues the dash (off on off) instead of restarting it (manual 6.8.3)");

        Rig h;
        h.screen4bpp();                           // PEX = 15, PPX = 0
        h.cmd(0x8800, {5, 0});                    // five pixels
        h.cmd(0x0C05);                            // RPR Pr05
        CHECK(h.readWord() == 0x0050, "RPR Pr05 reads PPX = 5 after a five-pixel line");
        h.cmd(0x8800, {9, 0});                    // four more
        h.cmd(0x0C05);
        CHECK(h.readWord() == 0x0090, "and the next line picks up where the last one left it");
    }

    SECTION("HD63484 -- pattern zoom: PZX repeats each bit; PZCX is the counter's starting value");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x1800, {2, 0x0005});
        g.cmd(0x0807, {0x0011});                  // PEX = 1, PZX = 1: each bit twice
        g.cmd(0x8800, {8, 0});
        CHECK(g.c.peekWord(Rig::kOrg) == 0x00FF && g.c.peekWord(Rig::kOrg + 1) == 0x00FF,
              "on on off off, on on off off (manual 6.8.3: '1111101010' zoomed is each bit scanned twice)");

        g.cmd(0x0805, {0x0001});                  // PPX = 0, PZCX = 1: the first bit is already half used
        g.cmd(0x8000, {0, 1});
        g.cmd(0x8800, {8, 1});
        CHECK(g.c.peekWord(Rig::kOrg - 0x10) == 0xF00F && g.c.peekWord(Rig::kOrg - 0x0F) == 0xF00F,
              "PZCX = 1 shifts the phase by one pixel: on off off on");
    }

    SECTION("HD63484 -- COL 10 and COL 11 (Pattern RAM direct, a 4x4 color tile)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x1800, {2, 0x0005});
        g.cmd(0x0807, {0x0010});
        g.cmd(0x0800, {0xAAAA});                  // CL0 = A
        g.c.pokeWord(Rig::kOrg, 0x2222);
        g.cmd(0x8810, {4, 0});                    // COL 10: bit 1 suppressed, bit 0 -> CL0
        CHECK(g.c.peekWord(Rig::kOrg) == 0xA2A2, "COL 10 draws CL0 on the off bits and leaves the on bits");

        Rig h;
        h.screen4bpp();
        // Pattern RAM direct (manual 6.6.2, Figure 6.11(d)): words 0-3 are row 0 of the tile,
        // 4-7 row 1, each a color replicated across the word.
        h.cmd(0x1800, {16, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888});
        h.cmd(0x0807, {0x1030});                  // PEX = 3, PEY = 1
        h.cmd(0x8818, {8, 0});                    // ALINE COL 11
        CHECK(h.c.peekWord(Rig::kOrg) == 0x4321 && h.c.peekWord(Rig::kOrg + 1) == 0x4321,
              "a line cycles through row PPY of the tile: 1 2 3 4 1 2 3 4");
        h.cmd(0x8000, {0, 2});
        h.cmd(0x0805, {0x0000});
        h.cmd(0xC018, {3, 3});                    // AFRCT COL 11, 4 x 2
        CHECK(h.c.peekWord(Rig::kOrg - 0x20) == 0x4321 && h.c.peekWord(Rig::kOrg - 0x30) == 0x8765,
              "a plane fill steps PPY per row: row 0 then row 1 of the tile");
    }

    SECTION("HD63484 -- AFRCT leaves PPY on the next row, so a stacked fill continues the tiling");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x1800, {4, 0x0005, 0x000A});       // row 0 = 0101, row 1 = 1010
        g.cmd(0x0807, {0x1030});                  // PEX = 3, PEY = 1
        g.cmd(0xC000, {3, 0});                    // one raster: row 0
        CHECK(g.c.peekWord(Rig::kOrg) == 0x0F0F, "first fill uses pattern row 0");
        CHECK(g.c.cpy() == 1, "CP is on the next raster");
        g.cmd(0xC000, {3, 1});                    // one more raster from CP: row 1
        CHECK(g.c.peekWord(Rig::kOrg - 0x10) == 0xF0F0, "the second fill picks up at pattern row 1");
        g.cmd(0x0C05);
        CHECK(g.readWord() == 0x0000, "PPY wrapped back to PSY; PPX back where the rows start");
    }

    SECTION("HD63484 -- the other straight-edge commands: RRCT, RPLL, RPLG, RFRCT");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {1, 1});
        g.cmd(0x9400, {2, 1});                    // RRCT: (1,1)-(3,2)
        CHECK(g.nib(1, 1) == 0xF && g.nib(2, 1) == 0xF && g.nib(3, 1) == 0xF && g.nib(1, 2) == 0xF &&
                  g.nib(3, 2) == 0xF,
              "the relative rectangle's perimeter");
        CHECK(g.nib(0, 1) == 0 && g.nib(4, 1) == 0 && g.nib(1, 3) == 0, "and nothing outside it");
        CHECK(g.c.cpx() == 1 && g.c.cpy() == 1, "CP unchanged");

        Rig h;
        h.screen4bpp();
        h.cmd(0x9C00, {2, 3, 0, 0, 2});           // RPLL: (0,0)->(3,0)->(3,2), each relative to the last
        CHECK(h.nib(2, 0) == 0xF && h.nib(3, 1) == 0xF && h.nib(3, 2) == 0, "relative polyline, end excluded");
        CHECK(h.c.cpx() == 3 && h.c.cpy() == 2, "CP at its end");

        Rig k;
        k.screen4bpp();
        k.cmd(0xA400, {2, 3, 0, 0, 2});           // RPLG: the same, closed
        CHECK(k.nib(3, 2) == 0xF && k.nib(1, 1) == 0xF, "the closing edge");
        CHECK(k.c.cpx() == 0 && k.c.cpy() == 0, "CP back at the start");

        Rig m;
        m.screen4bpp();
        m.cmd(0x8000, {1, 0});
        m.cmd(0xC400, {1, 1});                    // RFRCT: (1,0)-(2,1)
        CHECK(m.c.peekWord(Rig::kOrg) == 0x0FF0 && m.c.peekWord(Rig::kOrg - 0x10) == 0x0FF0, "2 x 2 filled");
        CHECK(m.c.cpx() == 1 && m.c.cpy() == 2, "CP = (A, Y+1)");
    }

    SECTION("HD63484 -- SCLR and DMOD: the modify operations under MASK");
    {
        Rig g;
        g.reg(0xC2, 0x0010);
        g.c.pokeWord(0x56, 0x1234);
        g.c.pokeWord(0x57, 0x1234);
        g.cmd(0x080C, {0x0000});
        g.cmd(0x080D, {0x0560});
        g.cmd(0x0804, {0x00FF});                  // MASK: the low byte only
        g.cmd(0x5C01, {0xFFFF, 1, 0});            // SCLR (OR) $FFFF, AX = 1, AY = 0
        CHECK(g.c.peekWord(0x56) == 0x12FF && g.c.peekWord(0x57) == 0x12FF, "SCLR ORs under the mask");

        g.cmd(0x0804, {0xFFFF});
        g.cmd(0x080D, {0x0560});
        g.cmd(0x2C03, {1, 0});                    // DMOD (EOR) AX = 1, AY = 0
        g.word(0x0F0F);
        g.word(0xF0F0);
        CHECK(g.c.peekWord(0x56) == (0x12FF ^ 0x0F0F) && g.c.peekWord(0x57) == (0x12FF ^ 0xF0F0),
              "DMOD EORs each data word into the block");
        CHECK((g.sr() & 0x20) != 0, "and ends with the last one");
    }

    SECTION("HD63484 -- DWT/DRD with negative AX/AY run the other way (manual DRD-2, DWT-2)");
    {
        Rig g;
        g.reg(0xCA, 0x0010);
        g.cmd(0x080C, {0x4000});
        g.cmd(0x080D, {0x1000});                  // RWP = $100 on the base screen
        g.cmd(0x2800, {0xFFFF, 0xFFFF});          // DWT AX = -1, AY = -1
        CHECK((g.sr() & 0x80) == 0, "no CER: negative sizes are legal");
        g.word(0xAAAA);
        g.word(0xBBBB);
        g.word(0xCCCC);
        g.word(0xDDDD);
        CHECK(g.c.peekWord(0x100) == 0xAAAA && g.c.peekWord(0x0FF) == 0xBBBB, "leftward along the first raster");
        CHECK(g.c.peekWord(0x110) == 0xCCCC && g.c.peekWord(0x10F) == 0xDDDD,
              "then DOWN in Y: one MW higher in memory, as CLR walks it");
        CHECK(g.c.param(0x0D) == 0x1100, "RWPe on the last raster");

        g.cmd(0x080D, {0x1000});
        g.cmd(0x2400, {0xFFFF, 0});               // DRD AX = -1: $100 then $0FF
        CHECK(g.readWord() == 0xAAAA && g.readWord() == 0xBBBB, "DRD reads leftward too");
    }

    SECTION("HD63484 -- a read bigger than the FIFO waits for room, and so does the command stream");
    {
        Rig g;
        g.cmd(0x1800, {24, 0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, 0x109, 0x10A, 0x10B});
        g.cmd(0x1C00, {12});                      // RPTN n = 12 words
        CHECK(g.c.readFifoWords() == 8 && (g.sr() & 0x08) != 0, "8 words in, RFF");
        CHECK((g.sr() & 0x20) == 0, "CED clear: the command is still delivering");
        g.cmd(0x0800, {0x7777});                  // WPR CL0 behind it
        CHECK(g.c.param(0) != 0x7777, "the next command waits in the write FIFO");
        CHECK((g.sr() & 0x01) == 0, "so the write FIFO is not empty");
        bool inOrder = true;
        for (uint16_t i = 0; i < 12; ++i)
            if (g.readWord() != (uint16_t)(0x100 + i)) inOrder = false;
        CHECK(inOrder, "all twelve words, in order (manual RD-1: 'a wait state until space becomes available')");
        CHECK(g.c.param(0) == 0x7777 && (g.sr() & 0x21) == 0x21, "then the stalled command ran, and the chip is idle");
    }

    SECTION("HD63484 -- RWP is ONE register: rewriting its high word keeps the low bits");
    {
        Rig g;
        g.cmd(0x080D, {0x0560});                  // RWPL = $056
        g.cmd(0x080C, {0x4000});                  // DN = 01, RWPH = 0
        CHECK(g.c.param(0x0D) == 0x0560 && g.c.param(0x0C) == 0x4000, "DN changed, the address did not");
    }

    SECTION("HD63484 -- CPY: the manual's example, S = 1 DSD = 000 -- a 4 x 7 source lands transposed (CPY-5/6)");
    {
        Rig g;
        g.reg(0xC2, 0x0010);                      // MWR0: MW = $10
        // The source, Pss = $89, AX = 3, AY = 6: four words wide, seven rasters UP (+Y is
        // MW lower), so Pse = $89 + 3 - 6 x $10 = $2C. Each word holds its own address.
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 4; ++x) {
                uint32_t a = (uint32_t)(0x89 + x - y * 0x10);
                g.c.pokeWord(a, (uint16_t)a);
            }
        g.cmd(0x080C, {0x0000});
        g.cmd(0x080D, {0x0B00});                  // RWP = $B0 on screen 0
        g.cmd(0x6800, {0x0000, 0x0890, 3, 6});    // CPY S = 1, DSD = 000: SA = $89
        CHECK((g.sr() & 0xA0) == 0x20, "executed: CED, no CER");
        // S = 1 scans COLUMNS of the source (upward); DSD = 000 writes ROWS rightward,
        // successive rows upward: source column x becomes destination row x.
        bool ok = true;
        for (int x = 0; x < 4; ++x)
            for (int y = 0; y < 7; ++y)
                if (g.c.peekWord((uint32_t)(0xB0 + y - x * 0x10)) != (uint16_t)(0x89 + x - y * 0x10)) ok = false;
        CHECK(ok, "four destination rows of seven words at $B0, $A0, $90, $80");
        CHECK(g.c.peekWord(0xB7) == 0 && g.c.peekWord(0x70) == 0, "and nothing past them");
        CHECK(g.c.param(0x0D) == 0x0700, "RWPe = $B0 - 3 x $10 - $10 = $70 (manual CPY-6)");
    }

    SECTION("HD63484 -- SCPY: MM under MASK, the manual's example $F0F0 (SCPY-6/7)");
    {
        Rig g;
        g.reg(0xC2, 0x0010);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 2; ++x) g.c.pokeWord((uint32_t)(0x85 + x - y * 0x10), 0xAAAA);
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 7; ++x) g.c.pokeWord((uint32_t)(0xB0 + x - y * 0x10), 0x5555);
        g.cmd(0x0804, {0xF0F0});                  // MASK
        g.cmd(0x080C, {0x0000});
        g.cmd(0x080D, {0x0B00});
        g.cmd(0x7800, {0x0000, 0x0850, 1, 6});    // SCPY S = 1, DSD = 000, MM = 00: SA = $85
        bool ok = true;
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 7; ++x)
                if (g.c.peekWord((uint32_t)(0xB0 + x - y * 0x10)) != 0xA5A5) ok = false;
        CHECK(ok, "only the MASK bits took the source: $5555 -> $A5A5");
        CHECK(g.c.param(0x0D) == 0x0900, "RWPe = $90 (manual SCPY-7)");

        g.cmd(0x0804, {0xFFFF});
        g.cmd(0x080D, {0x0B00});
        g.c.pokeWord(0x85, 0x0F0F);
        g.cmd(0x7003, {0x0000, 0x0850, 0, 0});    // SCPY S = 0, one word, MM = 11: EOR
        CHECK(g.c.peekWord(0xB0) == (0xA5A5 ^ 0x0F0F), "MM = 11 EORs the source into the destination");
    }

    SECTION("HD63484 -- CPY directions: negative AX runs the source leftward; DSD = 011 writes leftward, downward");
    {
        Rig g;
        g.reg(0xC2, 0x0010);
        g.c.pokeWord(0x40, 0x1111);
        g.c.pokeWord(0x3F, 0x2222);
        g.c.pokeWord(0x30, 0x3333);               // the raster ABOVE $40 (+Y)
        g.c.pokeWord(0x2F, 0x4444);
        g.cmd(0x080C, {0x0000});
        g.cmd(0x080D, {0x0B00});
        g.cmd(0x6300, {0x0000, 0x0400, 0xFFFF, 1});   // CPY S = 0, DSD = 011: AX = -1, AY = +1
        CHECK(g.c.peekWord(0xB0) == 0x1111 && g.c.peekWord(0xAF) == 0x2222,
              "first line: the source leftward from $40, written leftward from $B0");
        CHECK(g.c.peekWord(0xC0) == 0x3333 && g.c.peekWord(0xBF) == 0x4444,
              "second line: the source's raster above, written one raster DOWN (MW higher)");
        CHECK(g.c.param(0x0D) == 0x0D00, "RWPe one more raster down, at the line start: $D0");
    }

    SECTION("HD63484 -- CPY with MM bits is undefined: CER, and the stream stays in step");
    {
        Rig g;
        g.word(0x6001);
        CHECK((g.sr() & 0xA0) == 0xA0, "CER, CED");
        g.cmd(0x8000, {3, 0});
        CHECK(g.c.cpx() == 3, "the next word was a command");
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
        g.cmd(0xE000, {1, 2, 3, 4});              // AGCPY: recognized, not drawn
        CHECK((g.sr() & 0x80) != 0, "CER");
        CHECK((g.sr() & 0x20) != 0, "CED: free again");
        g.cmd(0x8000, {2, 0});                    // the next command is parsed as a command...
        CHECK(g.c.cpx() == 2, "...not as a stray parameter: the stream stayed in step");
    }

    SECTION("HD63484 -- CRCL: from (A+r, B) once round, the start drawn once, CP back at the center (CRCL-1/2)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {20, 8});
        g.cmd(0xA800, {7});                       // CRCL r = 7, counter-clockwise
        CHECK((g.sr() & 0xA0) == 0x20, "executed: CED, no CER");
        CHECK(g.lit(27, 8), "Ps = (A + r, B) is drawn");
        CHECK(g.c.cpx() == 20 && g.c.cpy() == 8, "CP is the center again");
        bool sym = true, near = true, inside = true;
        for (int y = -16; y <= 16; ++y)
            for (int x = 0; x < 64; ++x) {
                if (!g.lit(x, y)) continue;
                const int dx = x - 20, dy = y - 8;
                if (dx < -7 || dx > 7 || dy < -7 || dy > 7) inside = false;
                else if (!g.lit(20 - dx, y) || !g.lit(x, 8 - dy) || !g.lit(20 + dy, 8 + dx)) sym = false;
                const int e = dx * dx + dy * dy - 49;
                if (e < -7 || e > 7) near = false;
            }
        CHECK(inside && near, "every pixel within half a pixel of the circle");
        CHECK(sym, "and the circle is 8-way symmetric");

        Rig e;
        e.screen4bpp();
        e.cmd(0x8000, {20, 8});
        e.cmd(0xA803, {7});                       // EOR
        CHECK(e.picture() == g.picture(), "EOR-drawn it is the same picture: no pixel was drawn twice");
        Rig k;
        k.screen4bpp();
        k.cmd(0x8000, {20, 8});
        k.cmd(0xA900, {7});                       // C = 1: clockwise
        CHECK(k.picture() == g.picture(), "clockwise draws the same pixels");
    }

    SECTION("HD63484 -- ELPS: a : b = dX^2 : dY^2, the manual's example a = 9 b = 4 dX = 9 (ELPS-3)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {16, 10});
        g.cmd(0xAC00, {9, 4, 9});                 // "9 : 4 = 9^2 : 6^2" -> dY = 6
        CHECK(g.lit(25, 10) && g.lit(7, 10), "the X extremes, dX = 9 either side");
        CHECK(g.lit(16, 16) && g.lit(16, 4), "the Y extremes, dY = 6 either side");
        CHECK(!g.lit(16, 17) && !g.lit(26, 10), "and no further");
        CHECK(g.c.cpx() == 16 && g.c.cpy() == 10, "CP back at the center");
        bool sym = true;
        for (int y = 4; y <= 16; ++y)
            for (int x = 7; x <= 25; ++x)
                if (g.lit(x, y) && (!g.lit(32 - x, y) || !g.lit(x, 20 - y))) sym = false;
        CHECK(sym, "symmetric about both axes");
        g.cmd(0xAC00, {0, 4, 9});
        CHECK((g.sr() & 0x80) != 0, "a = 0 is an invalid parameter: CER");
    }

    SECTION("HD63484 -- AARC: from CP round the center to Pe, Pe not drawn, CP = Pe (AARC-2)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {12, 4});
        g.cmd(0xB000, {12, 10, 6, 10});           // CC (12,10), Pe (6,10), counter-clockwise
        CHECK(g.lit(12, 4), "the start, CP, is drawn");
        CHECK(g.lit(18, 10) && g.lit(12, 16), "through the right and the top");
        CHECK(!g.lit(6, 10), "Pe is not drawn");
        bool lowerLeft = false;
        for (int y = 3; y < 10; ++y)
            for (int x = 5; x < 12; ++x) lowerLeft |= g.lit(x, y);
        CHECK(!lowerLeft, "the lower-left quarter, the way not taken, is empty");
        CHECK(g.c.cpx() == 6 && g.c.cpy() == 10, "CP = Pe");

        Rig k;
        k.screen4bpp();
        k.cmd(0x8000, {12, 4});
        k.cmd(0xB100, {12, 10, 6, 10});           // C = 1: clockwise, the short way
        bool ll = false;
        for (int y = 3; y < 10; ++y)
            for (int x = 5; x < 12; ++x) ll |= k.lit(x, y);
        CHECK(ll && !k.lit(18, 10) && !k.lit(12, 16) && !k.lit(6, 10),
              "clockwise goes through the lower-left quarter only");

        Rig m;
        m.screen4bpp();
        m.cmd(0x8000, {12, 4});
        m.cmd(0xB000, {0x2000 | 12, 10, 0x2000 | 6, 10});   // bit 13 set: "only the low order 13 bits are effective"
        CHECK(m.picture() == g.picture(), "the bits above 13 are ignored");
    }

    SECTION("HD63484 -- RARC: center and end both relative to CP (RARC-2)");
    {
        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {6, 10});
        g.cmd(0xB400, {6, 0, 6, 6});              // CC = CP + (6,0) = (12,10); Pe = CP + (6,6) = (12,16)
        CHECK(g.lit(6, 10) && g.lit(12, 4) && g.lit(18, 10), "left, bottom, right");
        CHECK(!g.lit(12, 16), "Pe (12,16) not drawn");
        bool upperLeft = false;
        for (int y = 11; y <= 16; ++y)
            for (int x = 5; x < 12; ++x) upperLeft |= g.lit(x, y);
        CHECK(!upperLeft, "the upper-left quarter is empty");
        CHECK(g.c.cpx() == 12 && g.c.cpy() == 16, "CP = Pe");
    }

    SECTION("HD63484 -- AEARC/REARC: an arc of the ELPS ellipse, a quarter from (25,10) to (16,16)");
    {
        Rig full;
        full.screen4bpp();
        full.cmd(0x8000, {16, 10});
        full.cmd(0xAC00, {9, 4, 9});

        Rig g;
        g.screen4bpp();
        g.cmd(0x8000, {25, 10});
        g.cmd(0xB800, {9, 4, 16, 10, 16, 16});    // AEARC a b Xc Yc Xe Ye
        bool subset = true, quarter = true;
        for (int y = -16; y <= 16; ++y)
            for (int x = 0; x < 64; ++x)
                if (g.lit(x, y)) {
                    if (!full.lit(x, y)) subset = false;
                    if (x < 16 || y < 10) quarter = false;
                }
        CHECK(subset, "every pixel is a pixel of the whole ellipse");
        CHECK(quarter && g.lit(25, 10) && !g.lit(16, 16), "the upper-right quarter, Pe excluded");
        CHECK(g.c.cpx() == 16 && g.c.cpy() == 16, "CP = Pe");

        Rig r;
        r.screen4bpp();
        r.cmd(0x8000, {25, 10});
        r.cmd(0xBC00, {9, 4, (uint16_t)-9, 0, (uint16_t)-9, 6});   // REARC: the same, relative to CP
        CHECK(r.picture() == g.picture() && r.c.cpx() == 16 && r.c.cpy() == 16, "REARC draws the same arc");
    }

    SECTION("HD63484 -- curves in drawing order: the pattern runs along them, AREA stops where they cross");
    {
        Rig full;
        full.screen4bpp();
        full.cmd(0x8000, {20, 8});
        full.cmd(0xA800, {7});
        const int n = full.count();

        Rig d;
        d.screen4bpp();
        d.cmd(0x1800, {2, 0x0005});
        d.cmd(0x0807, {0x0010});                  // PEX = 1: on, off, on, off...
        d.cmd(0x8000, {20, 8});
        d.cmd(0xA800, {7});
        CHECK(d.lit(27, 8) && d.count() == (n + 1) / 2, "a dashed circle: every other pixel, from the start");

        Rig a;
        a.screen4bpp();
        a.cmd(0x0808, {20});                      // the area: x 20..40, y 0..16 -- the right half
        a.cmd(0x080A, {40});
        a.cmd(0x0809, {0});
        a.cmd(0x080B, {16});
        a.cmd(0x8000, {20, 8});
        a.cmd(0xA820, {7});                       // AREA 001: stop on leaving
        CHECK((a.sr() & 0x60) == 0x60, "ARD and CED");
        CHECK(a.lit(27, 8) && a.lit(20, 15) && !a.lit(19, 15), "drawn up over the top as far as x = 20");
        bool lower = false;
        for (int y = 0; y < 8; ++y)
            for (int x = 12; x <= 28; ++x) lower |= a.lit(x, y);
        CHECK(!lower, "and never round to the lower half");
        CHECK(a.c.cpx() == 19 && a.c.cpy() == 15, "CP where it crossed");
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

    SECTION("HD63484 -- scan-out at the address level: what MAD carries, for a board's own shift register");
    {
        Rig g;
        g.reg(0x04, 0x4030);                      // STR, GAI = 011
        CHECK(g.c.gaiWords() == 8 && g.c.accessMode() == 1, "GAI 011 is +8 words; ACM 0x is single access");
        g.reg(0x04, 0x4018);                      // GAI = 001, ACM = 10
        CHECK(g.c.gaiWords() == 2 && g.c.accessMode() == 2, "GAI 001 is +2; ACM 10 is interleaved (two cycles)");
        g.reg(0x04, 0x4040);                      // GAI = 100: no increment
        CHECK(g.c.gaiWords() == 0, "GAI 1x0 does not advance the display address");
        g.reg(0x04, 0x4070);                      // GAI = 111: every two cycles
        CHECK(g.c.gaiWords() == 1, "GAI 111 is treated as +1");

        g.reg(0x84, 0x0227);                      // HDS = 2 -> 3 cycles from HSYNC rise; HDW = 39 -> 40
        g.reg(0x88, 0x2002);                      // VDS = 32 -> raster 33; VSW = 2
        CHECK(g.c.hds() == 3 && g.c.hdw() == 40 && g.c.vds() == 33, "timing registers decoded with the manual's +1");

        g.reg(0x8A, 0x0004);                      // SP1 = 4
        g.reg(0xCA, 0x0010);                      // MWR1 = 16
        g.reg(0xCC, 0x0001);
        g.reg(0xCE, 0x2000);                      // SAR1 = $12000
        g.reg(0x06, 0x4000);                      // SE1
        uint32_t a = 0;
        CHECK(g.c.backgroundRaster(0, a) && a == 0x12000, "raster 0 of the base screen starts at SAR1");
        CHECK(g.c.backgroundRaster(3, a) && a == 0x12030, "raster 3 is three MW1 on");
        CHECK(!g.c.backgroundRaster(4, a), "raster 4 is past SP1: no screen owns it");
        CHECK(!g.c.backgroundRaster(-1, a), "nor a negative raster");
        g.reg(0x06, 0x0000);                      // SE1 clear: blanked
        CHECK(!g.c.backgroundRaster(0, a), "a blanked screen answers false: black");

        g.reg(0x06, 0x7000);                      // SE1 + SE0 = 11: an upper screen in front
        g.reg(0x8C, 0x0002);                      // SP0 = 2
        g.reg(0xC2, 0x0008);                      // MWR0 = 8
        g.reg(0xC4, 0x0000);
        g.reg(0xC6, 0x0500);                      // SAR0 = $500
        CHECK(g.c.backgroundRaster(1, a) && a == 0x508, "raster 1 is the upper screen's second raster");
        CHECK(g.c.backgroundRaster(2, a) && a == 0x12000, "raster 2 is the base screen's first");

        CHECK(!g.c.windowRaster(40, a), "no window while SE3 != 11");
        g.reg(0x06, 0x4300);                      // SE1 + SE3
        g.reg(0x94, 0x0027);                      // VWS = 39 -> the window starts on VSYNC raster 40
        g.reg(0x96, 0x0002);                      // VWW = 2
        g.reg(0xDA, 0x0004);                      // MWR3 = 4
        g.reg(0xDC, 0x0000);
        g.reg(0xDE, 0x0300);                      // SAR3 = $300
        CHECK(!g.c.windowRaster(39, a), "VSYNC raster 39 is above the window");
        CHECK(g.c.windowRaster(40, a) && a == 0x300, "raster 40 is its first, at SAR3");
        CHECK(g.c.windowRaster(41, a) && a == 0x304, "raster 41 its second, one MW3 on");
        CHECK(!g.c.windowRaster(42, a), "raster 42 is below it");
        CHECK(g.c.hws() == ((0x0000 >> 8) & 0xFF) + 1 && g.c.vws() == 40 && g.c.vww() == 2, "the window registers decode too");
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
