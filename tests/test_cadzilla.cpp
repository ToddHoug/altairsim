#include "test.h"
#include "framecheck.h"

#include "boards/cadzilla.h"
#include "core/machine.h"
#include "core/statefile.h"
#include "host/display_null.h"
#include "host/framedump.h"

#include <cstdint>
#include <string>

using namespace altair;

namespace {

// A machine with a cadzilla and a NullDisplay wired to it -- the SAME injection main()
// does (CadzillaBoard::setDisplay), one backend down. The board builds the monitor's frame
// in memory and the test reads the whole picture back with CHECK_FRAME.
struct Rig {
    Machine        m;
    NullDisplay    disp;
    CadzillaBoard* cad = nullptr;

    static constexpr uint8_t kAcrtc = 0x70;   // RS=0 at BASE+0, RS=1 at BASE+1
    static constexpr uint8_t kMode  = 0x73;   // MODE register, BASE+3, write-only
    static constexpr uint8_t kDac   = 0x74;   // Bt453, BASE+4..+7 -- fixed relative to BASE now

    Rig() {
        std::string err;
        m.bus.setVerify(true);
        cad = dynamic_cast<CadzillaBoard*>(m.add("cadzilla", "cad0", err));
        CadzillaBoard::setDisplay(&disp);
        m.power();
    }
    // The Display is injected statically; a test that builds a second Rig must point the
    // first back at its own before asking it to draw.
    void adopt() { CadzillaBoard::setDisplay(&disp); }

    // ---- the ACRTC through its two ports, 8-bit MPU mode ----
    void    ar(uint8_t a) { m.bus.ioWrite(kAcrtc, a); }
    uint8_t sr() { return m.bus.ioRead(kAcrtc); }
    void    data(uint8_t v) { m.bus.ioWrite(kAcrtc + 1, v); }
    uint8_t data() { return m.bus.ioRead(kAcrtc + 1); }
    void reg(uint8_t even, uint16_t v) {
        ar(even);
        data((uint8_t)(v >> 8));
        if (even < 0x80) ar((uint8_t)(even | 1));
        data((uint8_t)v);
    }
    void word(uint16_t w) {
        ar(0);
        data((uint8_t)(w >> 8));
        data((uint8_t)w);
    }
    void cmd(uint16_t op, std::initializer_list<uint16_t> params = {}) {
        word(op);
        for (uint16_t p : params) word(p);
    }

    // ---- the Bt453 through its four ports: address, then R, G, B ----
    void lut(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
        m.bus.ioWrite(kDac + 0, index);
        m.bus.ioWrite(kDac + 1, r);
        m.bus.ioWrite(kDac + 1, g);
        m.bus.ioWrite(kDac + 1, b);
    }

    // ---- the MODE register: one write-only byte at BASE+3 ----
    void mode_reg(uint8_t v) { m.bus.ioWrite(kMode, v); }

    // Program the ACRTC for the monitor mode the board is strapped to, the way the board's
    // documentation says to: 8 bpp, GAI +8, the mode's timing in memory cycles (doubled in
    // interleaved mode), the picture starting exactly where the porch ends, one raster per
    // MW words, SAR1 = 0. The origin goes on the BOTTOM raster so +Y is up on the screen.
    void programMode(bool interleaved = false) {
        const auto& md  = cad->currentMode();
        const int   acm = interleaved ? 2 : 1;
        const int   mw  = md.width / 2;                       // words per raster at 8 bpp
        reg(0x02, 0x0300);                                    // CCR: GBM = 011 (8 bpp), ABT clear
        reg(0x82, (uint16_t)(((md.hc() * acm - 1) << 8) | (md.hsw * acm)));           // HC, HSW
        reg(0x84, (uint16_t)(((md.hbp * acm - 1) << 8) | (md.width / 16 * acm - 1)));  // HDS, HDW
        reg(0x86, (uint16_t)md.vc());                                                  // VC
        reg(0x88, (uint16_t)(((md.vbp - 1) << 8) | md.vsw));                           // VDS, VSW
        reg(0x8A, (uint16_t)md.height);                                                // SP1
        reg(0xCA, (uint16_t)mw);                                                       // MWR1
        reg(0xCC, 0x0000);
        reg(0xCE, 0x0000);                                                             // SAR1 = 0
        reg(0x04, (uint16_t)(0x4030 | (interleaved ? 0x08 : 0)));   // OMR: STR, GAI = 011, ACM
        reg(0x06, 0x4000);                                          // DCR: SE1
        // The board's OWN glue reads MODE.AMODE for single/interleaved, not the ACRTC's OMR
        // ACM bit -- both must be set in agreement, or `wiring()` says so.
        mode_reg(interleaved ? 0x04 : 0x00);
        const uint32_t org = (uint32_t)(md.height - 1) * (uint32_t)mw;
        cmd(0x0400, {(uint16_t)(0x4000 | ((org >> 12) & 0xFF)), (uint16_t)((org & 0xFFF) << 4)});
        cmd(0x1800, {2, 0xFFFF});                             // pattern row 0 all ones
        cmd(0x0806, {0x0000});                                // PRC: PSX = PSY = 0
        cmd(0x0807, {0x00F0});                                // PEX = 15, PEY = 0
        cmd(0x0805, {0x0000});                                // PPX = PPY = 0
    }
    void color(uint8_t idx) { cmd(0x0801, {(uint16_t)((idx << 8) | idx)}); }   // CL1, both bytes

    uint8_t px(int x, int y) const {
        const Surface* s = disp.surface(cad);
        return s->pixels()[(size_t)y * (size_t)s->pitch() + (size_t)x];
    }
};

} // namespace

void test_cadzilla() {
    SECTION("cadzilla -- one 8-port block: the ACRTC, MODE (write-only), the Bt453, the BASE+2 gap");
    {
        Rig g;
        BusCycle c;
        for (int dir = 0; dir < 2; ++dir) {
            c.type = dir ? Cycle::IoRead : Cycle::IoWrite;
            c.addr = 0x70;
            CHECK(g.cad->decodes(c), "ACRTC RS=0 at BASE+0, both directions (AR out, SR in)");
            c.addr = 0x71;
            CHECK(g.cad->decodes(c), "ACRTC RS=1 at BASE+1, both directions");
            c.addr = 0x6F;
            CHECK(!g.cad->decodes(c), "not the port below BASE");
            c.addr = 0x72;
            CHECK(!g.cad->decodes(c), "BASE+2 is not decoded at all -- nobody answers it");
            for (uint16_t p = 0x74; p <= 0x77; ++p) {
                c.addr = p;
                CHECK(g.cad->decodes(c), "Bt453: all four of BASE+4..+7, both directions");
            }
            c.addr = 0x78;
            CHECK(!g.cad->decodes(c), "and not BASE+8 -- the block is eight ports");
        }
        // BASE+3, MODE, is write-only -- like the Dazzler's format port floats on a read.
        c.addr = 0x73;
        c.type = Cycle::IoWrite;
        CHECK(g.cad->decodes(c), "BASE+3 (MODE) decodes OUT");
        c.type = Cycle::IoRead;
        CHECK(!g.cad->decodes(c), "...but not IN");

        c.type = Cycle::MemRead;
        c.addr = 0x0000;
        CHECK(!g.cad->decodes(c), "no memory: the frame memory is the ACRTC's own");
        c.addr = 0xC000;
        CHECK(!g.cad->decodes(c), "nowhere");
    }

    SECTION("cadzilla -- the MODE register (BASE+3): write-only glue, decoded into live status");
    {
        Rig g;
        auto prop = [&](const char* name) -> Property {
            for (auto& p : g.cad->properties())
                if (p.name == name) return p;
            return Property{};
        };
        auto val = [&](const char* name) -> std::string {
            Property p = prop(name);
            return p.get ? p.get().text(p.radix) : std::string("<missing>");
        };
        CHECK(val("hspol") == "positive" && val("vspol") == "positive" && val("amode") == "single",
              "the register comes up all zero");
        CHECK(prop("olen").get().b() == false, "OLEN off");
        CHECK(!prop("hspol").set && !prop("vspol").set && !prop("amode").set && !prop("olen").set,
              "all four report read-only -- the register itself is write-only on the wire");

        g.mode_reg(0x01);                          // HSPOL alone
        CHECK(val("hspol") == "negative" && val("vspol") == "positive" && val("amode") == "single",
              "HSPOL decodes on its own");
        g.mode_reg(0x0F);                           // every bit the spec names
        CHECK(val("hspol") == "negative" && val("vspol") == "negative" && val("amode") == "interleaved",
              "all four bits decode independently");
        CHECK(prop("olen").get().b() == true, "OLEN on -- TBD, wired to nothing else yet");

        BusCycle c;
        c.type = Cycle::IoRead;
        c.addr = Rig::kMode;
        CHECK(!g.cad->decodes(c), "and it cannot be read back on the bus");
    }

    SECTION("cadzilla -- the ports reach the chips: ACRTC status and registers, DAC palette");
    {
        Rig g;
        CHECK(g.sr() == 0x23, "IN 70 is the ACRTC status register, $23 after power");
        g.reg(0x84, 0x1234);
        CHECK(g.cad->acrtc().reg(0x84) == 0x1234, "OUT 70/71 wrote HDR through AR");
        g.lut(1, 0x10, 0x20, 0x30);
        Color c = g.cad->dac().lookup(1);
        CHECK(c.r == 0x10 && c.g == 0x20 && c.b == 0x30, "OUT 74 then 75 x3 loaded LUT entry 1");
        g.m.bus.ioWrite(0x76, 1);                  // the address register's alias
        CHECK(g.m.bus.ioRead(0x75) == 0x10 && g.m.bus.ioRead(0x75) == 0x20 && g.m.bus.ioRead(0x75) == 0x30,
              "and IN 75 x3 reads it back R, G, B");
    }

    SECTION("cadzilla -- the monitor is there from power-on: a black frame before the ACRTC starts");
    {
        Rig g;
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 1, "the first pump presents a frame -- the window opens at the prompt");
        const Surface* s = g.disp.surface(g.cad);
        CHECK(s && s->width() == 640 && s->height() == 480, "at the mode's size");
        bool black = s != nullptr;
        for (size_t i = 0; black && i < s->pixels().size(); i += 97)
            if (s->pixels()[i] != 0) black = false;
        CHECK(black, "and black: no signal");
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 1, "and nothing repaints it until something changes");
    }

    SECTION("cadzilla -- the default monitor is 640x480: LUT, ORG, a rectangle, a line, a dot");
    {
        Rig g;
        CHECK(std::string(g.cad->currentMode().name) == "640x480", "the default mode");
        g.programMode();
        g.lut(0, 0, 0, 0);
        g.lut(1, 0xFF, 0, 0);                      // 1 = red
        g.lut(2, 0, 0xFF, 0);                      // 2 = green
        g.lut(3, 0, 0, 0xFF);                      // 3 = blue

        // Everything on the 32-pixel sampling grid the frame check reads: frame y = 479 - Y.
        g.color(1);
        g.cmd(0x8000, {0, 31});
        g.cmd(0x9000, {608, 479});                 // ARCT: corners (0,31) and (608,479)
        g.color(3);
        g.cmd(0x8000, {64, 255});
        g.cmd(0x8800, {545, 255});                 // ALINE: x 64..544 on Y = 255 (frame row 224)
        g.color(2);
        g.cmd(0x8000, {320, 127});
        g.cmd(0xCC00);                             // DOT at (320,127): frame (320, 352)

        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 1, "one frame presented");
        const Surface* s = g.disp.surface(g.cad);
        CHECK(s && s->width() == 640 && s->height() == 480, "the frame is the MONITOR's: 640x480");
        CHECK(g.cad->wiring() == "ok", "GBM 8 bpp, GAI +8, single access: wired as the board is");
        CHECK(g.cad->programmedWidth() == 640 && g.cad->programmedHeight() == 480 &&
                  g.cad->programmedX() == 0 && g.cad->programmedY() == 0,
              "the programmed picture fills the frame from (0,0)");

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

        // EVERY PIXEL, against what the three commands must have drawn (frame y = 479 - Y).
        // The rectangle's perimeter: x 0..608 on rows 0 and 448, x = 0 and 608 on rows 0..448.
        // The line: x 64..544 on row 224 (its end point 545 not drawn). The dot: (320, 352).
        auto oracle = [](int x, int y) -> uint8_t {
            if (x == 320 && y == 352) return 2;
            if (y == 224 && x >= 64 && x <= 544) return 3;
            bool onRect = (y == 0 || y == 448) ? (x >= 0 && x <= 608)
                                                : ((x == 0 || x == 608) && y > 0 && y < 448);
            return onRect ? 1 : 0;
        };
        CHECK_FRAME_PIXELS(g.disp, g.cad, oracle, "all 307,200 pixels match the oracle for the three commands");

        // THE RAMDAC IS THE PALETTE: what the wire carries is the LUT, not the index.
        const auto& pal = g.disp.palette(g.cad);
        CHECK(pal.size() == 256 && pal[1].r == 0xFF && pal[3].b == 0xFF, "the 256 LUT entries, as loaded");
        std::vector<uint8_t> rgb = frameRgb(*s, pal);
        size_t dot = ((size_t)352 * 640 + 320) * 3;
        CHECK(rgb[dot] == 0 && rgb[dot + 1] == 0xFF && rgb[dot + 2] == 0, "the dot resolves to green");

        // A palette-only change: no drawing command, yet the picture on the wire moves.
        uint32_t before = frameCrc(*s, pal);
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 1, "nothing changed: no new frame");
        g.lut(1, 0xFF, 0xFF, 0xFF);
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 2, "a LUT write is a change the host must see");
        CHECK(frameCrc(*g.disp.surface(g.cad), g.disp.palette(g.cad)) != before, "and the resolved frame differs");
        CHECK(g.px(0, 0) == 1, "while the pixel values are what they were");
    }

    SECTION("cadzilla -- the monitor places the picture by HDS/VDS against its porches");
    {
        Rig g;
        g.programMode();
        g.color(1);
        g.cmd(0x8000, {0, 479});
        g.cmd(0x8800, {640, 479});                 // the whole top raster in 1
        g.cad->pump();
        CHECK(g.px(0, 0) == 1 && g.px(639, 0) == 1 && g.px(0, 1) == 0, "flush with the frame");

        // HDS one memory cycle later than the back porch: the picture shifts 16 pixels right
        // and its last cycle falls off the right edge -- the blanking on the left is black.
        const uint16_t hdr = g.cad->acrtc().reg(0x84);
        g.reg(0x84, (uint16_t)(hdr + 0x0100));
        g.cad->pump();
        CHECK(g.px(15, 0) == 0 && g.px(16, 0) == 1 && g.px(639, 0) == 1, "shifted right by one cycle");
        CHECK(g.cad->programmedX() == 16, "and SHOW says so");
        g.reg(0x84, (uint16_t)(hdr - 0x0100));     // one cycle EARLY: the first cycle is clipped
        g.cad->pump();
        CHECK(g.px(0, 0) == 1 && g.cad->programmedX() == -16, "shifted left, clipped, reported negative");
        g.reg(0x84, hdr);

        // VDS one raster later: frame row 0 is blanking, the raster is on row 1.
        const uint16_t vdr = g.cad->acrtc().reg(0x88);
        g.reg(0x88, (uint16_t)(vdr + 0x0100));
        g.cad->pump();
        CHECK(g.px(0, 0) == 0 && g.px(0, 1) == 1 && g.cad->programmedY() == 1, "shifted down one raster");
        g.reg(0x88, vdr);

        // A picture narrower than the monitor: HDW halved leaves the right half black.
        g.reg(0x84, (uint16_t)((hdr & 0xFF00) | 19));   // 20 cycles = 320 px
        g.cad->pump();
        CHECK(g.px(319, 0) == 1 && g.px(320, 0) == 0 && g.cad->programmedWidth() == 320, "320 wide, then blanking");
    }

    SECTION("cadzilla -- the board is wired for 8 bpp and GAI +8; anything else is what the hardware would show");
    {
        Rig g;
        g.programMode();
        // Distinct bytes in the top raster's first 16 words (SAR1 = 0: memory raster 0 is the
        // top of the screen; the ORG sits on the bottom one).
        const uint32_t top = 0;
        for (uint32_t i = 0; i < 16; ++i) g.cad->acrtc().pokeWord(top + i, (uint16_t)(0x2010 + i * 0x0101));
        g.cad->pump();
        CHECK(g.px(0, 0) == 0x10 && g.px(1, 0) == 0x20 && g.px(2, 0) == 0x11 && g.px(16, 0) == 0x18,
              "GAI +8: the second fetch starts eight words on -- a continuous raster");

        g.reg(0x04, 0x4000);                       // GAI = 000: the ACRTC steps ONE word per cycle
        g.cad->pump();
        CHECK(g.cad->wiring() == "OMR GAI is +1 words, the board fetches 8", "SHOW names the mismatch");
        CHECK(g.px(0, 0) == 0x10 && g.px(16, 0) == 0x11 && g.px(17, 0) == 0x21,
              "the board still fetches eight words per cycle, so the picture repeats itself, as the hardware would");
        g.reg(0x04, 0x4030);

        g.reg(0x02, 0x0200);                       // GBM = 010: 4 bpp
        CHECK(g.cad->wiring() == "CCR GBM is 4 bpp, the board is wired for 8", "a wrong GBM is named too");
        g.cmd(0x0801, {0xFFFF});                   // CL1 = $FFFF: F in every 4-bit field
        g.cmd(0x8000, {1, 479});
        g.cmd(0xCC00);                             // the drawing engine believes 4 bpp: dot 1 is bits 4-7
        g.cad->pump();
        CHECK(g.px(0, 0) == 0xF0, "so the byte the shift register sees has the high nibble lit: scrambled, faithfully");
        g.reg(0x02, 0x0300);
        CHECK(g.cad->wiring() == "ok", "put right, the wiring line clears");

        // The board's OWN glue (MODE AMODE) is what the picture actually uses -- not the
        // ACRTC's OMR ACM bit, which must simply agree with it or `wiring` says so.
        g.reg(0x04, 0x4038);                        // OMR ACM = interleaved; MODE AMODE still single
        CHECK(g.cad->wiring() == "OMR ACM is interleaved, MODE AMODE says single",
              "the two straps disagreeing is named");
        CHECK(g.cad->programmedWidth() == 640, "and the picture still runs on MODE's single, unmoved");
        g.mode_reg(0x04);                           // bring MODE AMODE into agreement: interleaved
        CHECK(g.cad->wiring() == "ok", "agreeing again clears it");
        CHECK(g.cad->programmedWidth() == 320, "and now the SAME HDW register reads as half the pixels");
        g.mode_reg(0x00);
        g.reg(0x04, 0x4030);

        g.reg(0x04, 0x403C);                       // ACM = 11: superimposed
        CHECK(g.cad->wiring() == "OMR ACM is superimposed, the board wires single and interleaved only",
              "superimposed mode is not wired");
    }

    SECTION("cadzilla -- 1024x768 in interleaved mode: doubled horizontal registers, 8 pixels a cycle");
    {
        Rig g;
        std::string err;
        CHECK(setProperty(*g.cad, "mode", "1024x768", err), "the mode strap takes 1024x768");
        g.programMode(true);
        CHECK((g.cad->acrtc().reg(0x84) & 0xFF) == 127 && g.cad->acrtc().accessMode() == 2,
              "HDW = 127: 128 memory cycles of 8 pixels, ACM interleaved");
        g.color(1);
        g.cmd(0x8000, {0, 63});
        g.cmd(0x9000, {960, 767});                 // corners on the 64-pixel sampling grid
        g.cad->pump();
        const Surface* s = g.disp.surface(g.cad);
        CHECK(s && s->width() == 1024 && s->height() == 768, "the frame is 1024x768");
        CHECK(g.cad->wiring() == "ok" && g.cad->programmedWidth() == 1024 &&
                  g.cad->programmedHeight() == 768 && g.cad->programmedX() == 0 && g.cad->programmedY() == 0,
              "the doubled registers describe the same full-frame picture");
        TextGridOpts every64;
        every64.xStep = 64;
        every64.yStep = 64;
        CHECK_FRAME_OPTS(g.disp, g.cad, R"(
1111111111111111
1..............1
1..............1
1..............1
1..............1
1..............1
1..............1
1..............1
1..............1
1..............1
1..............1
1111111111111111
)", every64, "sampled every 64th pixel: the border, at 1024x768 through the interleaved fetch");
        // Every pixel: the perimeter of (0,63)-(960,767) is rows 0 and 704, columns 0 and 960.
        CHECK_FRAME_PIXELS(g.disp, g.cad, [](int x, int y) -> uint8_t {
            bool on = (y == 0 || y == 704) ? (x <= 960) : ((x == 0 || x == 960) && y < 704);
            return on ? 1 : 0;
        }, "all 786,432 pixels of the interleaved 1024x768 frame match");

        // In interleaved mode HDS is in doubled units: +2 memory cycles is one display cycle.
        const uint16_t hdr = g.cad->acrtc().reg(0x84);
        g.reg(0x84, (uint16_t)(hdr + 0x0200));
        g.cad->pump();
        CHECK(g.px(15, 0) == 0 && g.px(16, 0) == 1 && g.cad->programmedX() == 16, "shifted one display cycle: 16 pixels");
    }

    SECTION("cadzilla -- every mode: the frame is the strap's size and a full picture fills it");
    {
        for (int i = 0; i < CadzillaBoard::modeCount(); ++i) {
            Rig g;
            std::string err;
            const auto& md = CadzillaBoard::mode(i);
            CHECK(setProperty(*g.cad, "mode", md.name, err), "the strap takes every listed mode");
            g.programMode();
            g.color(1);
            g.cmd(0x8000, {0, 0});
            g.cmd(0xC000, {(uint16_t)(md.width - 1), (uint16_t)(md.height - 1)});   // AFRCT: fill it all
            g.cad->pump();
            const Surface* s = g.disp.surface(g.cad);
            CHECK(s && s->width() == md.width && s->height() == md.height, "the frame is the mode's size");
            std::string what = std::string(md.name) + ": every pixel of the frame is the fill";
            CHECK_FRAME_PIXELS(g.disp, g.cad, [](int, int) -> uint8_t { return 1; }, what.c_str());
        }
    }

    SECTION("cadzilla -- the window screen lands in the frame at HWS/VWS");
    {
        Rig g;
        g.programMode();
        // A 16 x 2 window one display cycle in and three rasters down, on screen 3 at $70000.
        const auto& md = g.cad->currentMode();
        g.reg(0x92, (uint16_t)((md.hbp << 8) | 0));              // HWS = hbp: one cycle after the display start; HWW = 1 cycle
        g.reg(0x94, (uint16_t)(md.vbp + 3 - 1));                  // VWS: raster 3 of the picture
        g.reg(0x96, 2);                                           // VWW
        g.reg(0xDA, 8);                                           // MWR3 = 8 words
        g.reg(0xDC, 0x0007);
        g.reg(0xDE, 0x0000);                                      // SAR3 = $70000
        g.reg(0x06, 0x4300);                                      // SE1 + SE3 = 11
        for (uint32_t i = 0; i < 16; ++i) g.cad->acrtc().pokeWord(0x70000 + i, 0x0505);
        g.cad->pump();
        CHECK(g.px(15, 3) == 0 && g.px(16, 3) == 5 && g.px(31, 3) == 5 && g.px(32, 3) == 0,
              "raster 3: the window's 16 pixels at x = 16..31");
        CHECK(g.px(16, 4) == 5 && g.px(16, 2) == 0 && g.px(16, 5) == 0, "two rasters tall, from raster 3");
    }

    SECTION("cadzilla -- SHOW: the straps validate, the live lines are read-only");
    {
        Rig g;
        auto prop = [&](const char* name) -> Property {
            for (auto& p : g.cad->properties())
                if (p.name == name) return p;
            return Property{};
        };
        auto val = [&](const char* name) -> std::string {
            Property p = prop(name);
            return p.get ? p.get().text(p.radix) : std::string("<missing>");
        };
        CHECK(val("mode") == "640x480" && val("vram") == "2048", "defaults: 640x480, 2 MB of frame memory");
        CHECK(g.cad->acrtc().vram().size() == 1u << 20, "2048 KB is the ACRTC's whole 1 M-word address space");
        CHECK(val("video") == "off" && val("status") == "0x23", "comes up stopped");
        CHECK(!prop("video").set && !prop("picture").set && !prop("wiring").set && !prop("status").set,
              "live status is read-only");
        CHECK((bool)prop("mode").set && (bool)prop("port").set && (bool)prop("vram").set,
              "the straps are settable");
        CHECK(!prop("hspol").set && !prop("vspol").set && !prop("amode").set && !prop("olen").set,
              "the MODE register's four decoded fields are read-only, like the rest of live status");
        g.programMode();
        CHECK(val("video") == "on" && val("picture") == "640x480 at (0,0)" && val("wiring") == "ok",
              "live values follow the registers");

        std::string err;
        CHECK(!setProperty(*g.cad, "mode", "320x200", err), "an unlisted mode is refused");
        CHECK(!setProperty(*g.cad, "port", "71", err), "a BASE that is not a multiple of 8 is refused");
        CHECK(!setProperty(*g.cad, "port", "74", err), "not even a multiple of 4 is enough now -- the whole block moves together");
        CHECK(!setProperty(*g.cad, "vram", "100", err), "a non-power-of-two vram is refused");
        CHECK(!setProperty(*g.cad, "vram", "4096", err), "and more than the ACRTC can address");
        CHECK(setProperty(*g.cad, "port", "E0", err), "a multiple of 8 is taken");
        BusCycle c;
        c.type = Cycle::IoWrite;
        c.addr = 0xE1;
        CHECK(g.cad->decodes(c), "and the ACRTC's data port now decodes there");
        c.addr = 0xE4;
        CHECK(g.cad->decodes(c), "with the Bt453 four ports along, still at BASE+4 -- one strap moves both chips");
        c.addr = 0x74;
        CHECK(!g.cad->decodes(c), "and nothing answers at the old Bt453 address any more");
        CHECK(setProperty(*g.cad, "vram", "16", err) && g.cad->acrtc().vram().size() == 16 * 512, "vram refits the frame memory: 16 KB is 8 K words");

        // Changing the mode re-opens the window at the new size on the next frame.
        Rig h;
        h.programMode();
        h.cad->pump();
        CHECK(setProperty(*h.cad, "mode", "800x600", err), "SET mode=800x600");
        h.cad->pump();
        CHECK(h.disp.surface(h.cad)->width() == 800 && h.disp.surface(h.cad)->height() == 600,
              "the frame is 800x600 now -- the 640x480 picture sits in its top-left corner");
        CHECK(h.px(639, 479) == 0 && h.cad->programmedWidth() == 640, "the programmed picture did not change");
    }

    SECTION("cadzilla -- RESET* resets the ACRTC and keeps the palette; a snapshot restores the picture");
    {
        Rig g;
        g.programMode();
        g.lut(1, 0xFF, 0, 0);
        g.color(1);
        g.cmd(0x8000, {0, 31});
        g.cmd(0x9000, {608, 479});
        g.cad->pump();

        StateWriter w;
        g.cad->serialize(w);

        Rig h;
        StateReader r(w.data());
        h.cad->deserialize(r);
        CHECK(r.ok(), "the state reads back");
        h.cad->pump();
        TextGridOpts every32;
        every32.xStep = 32;
        every32.yStep = 32;
        CHECK_FRAME_OPTS(h.disp, h.cad, R"(
11111111111111111111
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
1..................1
11111111111111111111
)", every32, "the restored board repaints the same picture from its own frame memory");
        CHECK(h.disp.palette(h.cad)[1].r == 0xFF, "with the same palette");

        g.adopt();
        g.m.reset(Reset::Bus);                     // RESET*: the ACRTC stops (STR clears)
        CHECK(!g.cad->acrtc().displayOn(), "RESET* stops the display");
        CHECK(g.cad->dac().lookup(1).r == 0xFF, "the Bt453 has no reset pin: the LUT survives");
        g.cad->pump();
        const Surface* s = g.disp.surface(g.cad);
        bool black = s && s->width() == 640 && s->height() == 480;
        for (size_t i = 0; black && i < s->pixels().size(); i += 97)
            if (s->pixels()[i] != 0) black = false;
        CHECK(black, "and the monitor shows a black 640x480 frame -- the window stays, the picture is gone");
    }
}
