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
// does (CadzillaBoard::setDisplay), one backend down. The board scans its ACRTC's frame
// memory out into memory and the test reads the whole picture back with CHECK_FRAME.
struct Rig {
    Machine        m;
    NullDisplay    disp;
    CadzillaBoard* cad = nullptr;

    static constexpr uint8_t kAcrtc = 0x70;   // RS=0 at +0, RS=1 at +1
    static constexpr uint8_t kDac   = 0x74;   // C1C0 at +0..+3

    Rig() {
        std::string err;
        m.bus.setVerify(true);
        cad = dynamic_cast<CadzillaBoard*>(m.add("cadzilla", "cad0", err));
        CadzillaBoard::setDisplay(&disp);
        m.power();
    }

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

    // A 32 x 8 picture at 8 bpp: HDW+1 = 16 memory cycles of two pixels; SP1 = 8 rasters;
    // MW1 = 16 words per raster; SAR1 = 0. The origin is put on the BOTTOM raster
    // (word 7 * 16 = 112) so the ACRTC's +Y (up in memory) is up on the screen too.
    void screen32x8() {
        reg(0x02, 0x0300);            // CCR: GBM = 011, 8 bpp; ABT clear
        reg(0x84, 0x000F);            // HDR: HDS = 0, HDW = 15
        reg(0x8A, 0x0008);            // SP1 = 8 rasters
        reg(0xCA, 0x0010);            // MWR1 = 16 words
        reg(0xCC, 0x0000);            // SAR1 = 0
        reg(0xCE, 0x0000);
        reg(0x04, 0x4000);            // OMR: STR
        reg(0x06, 0x4000);            // DCR: SE1 -- the base screen displays
        cmd(0x0400, {0x4000, (uint16_t)(112 << 4)});   // ORG: base screen, word 112, dot 0
        cmd(0x1800, {2, 0xFFFF});     // pattern row 0 all ones (n = 1 word x 2 bytes)
        cmd(0x0806, {0x0000});        // PRC: PSX = PSY = 0
        cmd(0x0807, {0x00F0});        // PEX = 15, PEY = 0
        cmd(0x0805, {0x0000});        // PPX = PPY = 0
    }
};

} // namespace

void test_cadzilla() {
    SECTION("cadzilla -- six I/O ports, IN and OUT, and no memory of its own");
    {
        Rig g;
        BusCycle c;
        for (int dir = 0; dir < 2; ++dir) {
            c.type = dir ? Cycle::IoRead : Cycle::IoWrite;
            c.addr = 0x70;
            CHECK(g.cad->decodes(c), "ACRTC RS=0 at 70, both directions (AR out, SR in)");
            c.addr = 0x71;
            CHECK(g.cad->decodes(c), "ACRTC RS=1 at 71, both directions");
            c.addr = 0x6F;
            CHECK(!g.cad->decodes(c), "not the port below");
            c.addr = 0x72;
            CHECK(!g.cad->decodes(c), "nor 72-73: the DAC starts at 74");
            for (uint16_t p = 0x74; p <= 0x77; ++p) {
                c.addr = p;
                CHECK(g.cad->decodes(c), "Bt453: all four of 74-77, both directions");
            }
            c.addr = 0x78;
            CHECK(!g.cad->decodes(c), "and not 78");
        }
        c.type = Cycle::MemRead;
        c.addr = 0x0000;
        CHECK(!g.cad->decodes(c), "no memory: the frame memory is the ACRTC's own");
        c.addr = 0xC000;
        CHECK(!g.cad->decodes(c), "nowhere");
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

    SECTION("cadzilla -- nothing to show, no window: a chip that is not displaying draws nothing");
    {
        Rig g;
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 0, "no frame presented while the ACRTC is stopped");
        CHECK(g.disp.surface(g.cad) == nullptr, "and no surface acquired -- no window opens");
    }

    SECTION("cadzilla -- end to end: LUT, ORG, a rectangle, a line and a dot, as one picture");
    {
        Rig g;
        g.screen32x8();
        g.lut(0, 0, 0, 0);
        g.lut(1, 0xFF, 0, 0);                      // 1 = red
        g.lut(2, 0, 0xFF, 0);                      // 2 = green
        g.lut(3, 0, 0, 0xFF);                      // 3 = blue

        g.cmd(0x0801, {0x0101});                   // CL1 = pixel value 1 in both bytes
        g.cmd(0x8000, {2, 1});                     // AMOVE (2,1)
        g.cmd(0x9000, {29, 6});                    // ARCT to (29,6): the frame, in red
        g.cmd(0x0801, {0x0303});                   // 3 = blue
        g.cmd(0x8000, {5, 3});
        g.cmd(0x8800, {12, 3});                    // ALINE to (12,3): pixels 5..11
        g.cmd(0x0801, {0x0202});                   // 2 = green
        g.cmd(0x8000, {16, 3});
        g.cmd(0xCC00);                             // DOT at (16,3)

        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 1, "one frame presented");
        const Surface* s = g.disp.surface(g.cad);
        CHECK(s && s->width() == 32 && s->height() == 8, "32 x 8: (HDW+1) cycles x 2 pixels at 8 bpp, SP1 rasters");

        // Row 0 is the TOP raster (y = 7 in ACRTC coordinates, since the origin is the
        // bottom raster and +Y is up). Every character is one pixel's LUT index.
        CHECK_FRAME(g.disp, g.cad, R"(
................................
..1111111111111111111111111111..
..1..........................1..
..1..........................1..
..1..3333333....2............1..
..1..........................1..
..1111111111111111111111111111..
................................
)", "the rectangle, the line and the dot land where the ACRTC put them, right way up");

        // THE RAMDAC IS THE PALETTE: what the wire carries is the LUT, not the index.
        const auto& pal = g.disp.palette(g.cad);
        CHECK(pal.size() == 256, "all 256 LUT entries handed to the host");
        CHECK(pal[1].r == 0xFF && pal[1].g == 0 && pal[3].b == 0xFF, "in the colors the guest loaded");
        std::vector<uint8_t> rgb = frameRgb(*s, pal);
        size_t dot = ((size_t)4 * 32 + 16) * 3;   // row 4 (y = 3 from the bottom), x = 16
        CHECK(rgb[dot] == 0 && rgb[dot + 1] == 0xFF && rgb[dot + 2] == 0, "the dot resolves to green");

        // A palette-only change: no drawing command, yet the picture on the wire moves.
        uint32_t before = frameCrc(*s, pal);
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 1, "nothing changed: no new frame");
        g.lut(1, 0xFF, 0xFF, 0xFF);                // the frame turns white
        g.cad->pump();
        CHECK(g.disp.frames(g.cad) == 2, "a LUT write is a change the host must see");
        CHECK(frameCrc(*g.disp.surface(g.cad), g.disp.palette(g.cad)) != before, "and the resolved frame differs");
        CHECK(g.disp.surface(g.cad)->pixels()[(size_t)1 * 32 + 2] == 1, "while the pixel values are what they were");
    }

    SECTION("cadzilla -- SHOW: live status is read-only, the straps are validated");
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
        CHECK(val("video") == "off", "comes up not displaying");
        CHECK(val("status") == "0x23", "status is the ACRTC SR");
        CHECK(val("depth") == "1", "GBM 000 is 1 bpp");
        CHECK(!prop("video").set && !prop("resolution").set && !prop("status").set, "status is read-only");
        CHECK((bool)prop("port").set && (bool)prop("dac").set && (bool)prop("vram").set, "the straps are settable");

        g.screen32x8();
        CHECK(val("video") == "on" && val("resolution") == "32x8" && val("depth") == "8", "live values follow the registers");

        std::string err;
        CHECK(!setProperty(*g.cad, "port", "71", err), "an odd ACRTC base is refused");
        CHECK(!setProperty(*g.cad, "dac", "76", err), "a DAC base that is not a multiple of 4 is refused");
        CHECK(!setProperty(*g.cad, "vram", "100", err), "a non-power-of-two vram is refused");
        CHECK(setProperty(*g.cad, "port", "E0", err) && setProperty(*g.cad, "dac", "E4", err), "even and 4-aligned are taken");
        BusCycle c;
        c.type = Cycle::IoWrite;
        c.addr = 0xE1;
        CHECK(g.cad->decodes(c), "and the board now decodes there");
        c.addr = 0x71;
        CHECK(!g.cad->decodes(c), "and not where it was");
        CHECK(setProperty(*g.cad, "vram", "16", err) && g.cad->acrtc().vram().size() == 16 * 1024, "vram refits the frame memory");
    }

    SECTION("cadzilla -- RESET* resets the ACRTC and keeps the palette; a snapshot restores the picture");
    {
        Rig g;
        g.screen32x8();
        g.lut(1, 0xFF, 0, 0);
        g.cmd(0x0801, {0x0101});
        g.cmd(0x8000, {0, 0});
        g.cmd(0x9000, {31, 7});                    // a border around the whole screen
        g.cad->pump();

        StateWriter w;
        g.cad->serialize(w);

        Rig h;
        StateReader r(w.data());
        h.cad->deserialize(r);
        CHECK(r.ok(), "the state reads back");
        h.cad->pump();
        CHECK_FRAME(h.disp, h.cad, R"(
11111111111111111111111111111111
1..............................1
1..............................1
1..............................1
1..............................1
1..............................1
1..............................1
11111111111111111111111111111111
)", "the restored board repaints the same picture from its own frame memory");
        CHECK(h.disp.palette(h.cad)[1].r == 0xFF, "with the same palette");

        // The Display is injected statically, so building `h` pointed every cadzilla at
        // h.disp; point g's back at its own before asking it to draw.
        CadzillaBoard::setDisplay(&g.disp);
        g.m.reset(Reset::Bus);                     // RESET*: the ACRTC stops (STR clears)
        CHECK(!g.cad->acrtc().displayOn(), "RESET* stops the display");
        CHECK(g.cad->dac().lookup(1).r == 0xFF, "the Bt453 has no reset pin: the LUT survives");
        g.cad->pump();
        CHECK_FRAME(g.disp, g.cad, R"(
................................
................................
................................
................................
................................
................................
................................
................................
)", "and the board paints its last geometry black");
    }
}
