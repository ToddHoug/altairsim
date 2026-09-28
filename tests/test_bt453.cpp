#include "test.h"

#include "chips/bt453.h"
#include "core/statefile.h"

#include <cstdint>

using namespace altair;

// The Bt453 RAMDAC (chips/bt453.h) driven exactly as the data sheet's Tables 1 and 2
// describe, with no board and no bus: C1C0 in, bytes out. Every expected value here is
// derived from reference/Brooktree Bt453 RAMDAC.md.

namespace {

// Load one entry the way a driver does: address, then R, G, B.
void load(Bt453& d, uint8_t addr, uint8_t r, uint8_t g, uint8_t b) {
    d.write(0, addr);
    d.write(1, r);
    d.write(1, g);
    d.write(1, b);
}

bool is(const Color& c, uint8_t r, uint8_t g, uint8_t b) {
    return c.r == r && c.g == g && c.b == b && c.a == 0xFF;
}

} // namespace

void test_bt453() {
    SECTION("Bt453 -- comes up black, address 0, phase red");
    {
        Bt453 d;
        CHECK(d.address() == 0 && d.phase() == 0, "address register 0, next cycle is red");
        CHECK(is(d.lookup(0), 0, 0, 0) && is(d.lookup(0xFF), 0, 0, 0), "every entry is black");
        CHECK(d.takeDirty(), "a fresh chip owes the host its palette once");
        CHECK(!d.takeDirty(), "...and only once");
    }

    SECTION("Bt453 -- a color is three write cycles, and lands on the BLUE one");
    {
        Bt453 d;
        d.takeDirty();
        d.write(0, 0x10);            // address register, C1C0 = 00
        d.write(1, 0x11);            // red
        CHECK(is(d.lookup(0x10), 0, 0, 0), "after red alone the entry is unchanged");
        CHECK(d.phase() == 1, "and the phase moved to green");
        d.write(1, 0x22);            // green
        CHECK(is(d.lookup(0x10), 0, 0, 0), "after green the entry is still unchanged");
        CHECK(!d.takeDirty(), "nothing has changed on the wire yet");
        d.write(1, 0x33);            // blue
        CHECK(is(d.lookup(0x10), 0x11, 0x22, 0x33), "the blue cycle writes all 24 bits at once");
        CHECK(d.takeDirty(), "and the look-up table is now dirty");
        CHECK(d.address() == 0x11 && d.phase() == 0, "the address incremented; phase is back to red");
    }

    SECTION("Bt453 -- consecutive entries need no second address write");
    {
        Bt453 d;
        d.write(0, 0x20);
        d.write(1, 1); d.write(1, 2); d.write(1, 3);
        d.write(1, 4); d.write(1, 5); d.write(1, 6);
        CHECK(is(d.lookup(0x20), 1, 2, 3), "entry $20");
        CHECK(is(d.lookup(0x21), 4, 5, 6), "entry $21, from the auto-increment");
        CHECK(d.address() == 0x22, "and the address is on $22");
    }

    SECTION("Bt453 -- the address wraps: a blue cycle at $FF leaves the address at $00");
    {
        Bt453 d;
        load(d, 0xFF, 9, 9, 9);
        CHECK(d.address() == 0x00, "$FF + 1 -> $00");
        d.write(1, 7); d.write(1, 7); d.write(1, 7);
        CHECK(is(d.lookup(0x00), 7, 7, 7) && is(d.lookup(0xFF), 9, 9, 9),
              "the next sequence wrote entry $00, and $FF kept its color");
    }

    SECTION("Bt453 -- reading a color is three read cycles, then the address increments");
    {
        Bt453 d;
        load(d, 0x40, 0xA1, 0xB2, 0xC3);
        load(d, 0x41, 0xD4, 0xE5, 0xF6);
        d.write(2, 0x40);            // C1C0 = 10 is the SAME address register
        CHECK(d.read(1) == 0xA1, "red");
        CHECK(d.read(1) == 0xB2, "green");
        CHECK(d.read(1) == 0xC3, "blue");
        CHECK(d.address() == 0x41, "following the blue read the address increments");
        CHECK(d.read(1) == 0xD4 && d.read(1) == 0xE5 && d.read(1) == 0xF6,
              "so the next three reads are the next entry");
    }

    SECTION("Bt453 -- a WRITE to the address register resets the phase; a READ does not");
    {
        Bt453 d;
        load(d, 0x05, 0x10, 0x20, 0x30);
        d.write(0, 0x05);
        CHECK(d.read(1) == 0x10, "red...");
        CHECK(d.read(0) == 0x05, "the address reads back mid-sequence");
        CHECK(d.read(2) == 0x05, "...at either alias");
        CHECK(d.phase() == 1, "and the phase is untouched: still green");
        CHECK(d.read(1) == 0x20, "so the next data read is green, not red");
        d.write(0, 0x05);
        CHECK(d.phase() == 0, "a write to the address register starts over at red");
        CHECK(d.read(1) == 0x10, "and the next data read is red again");
    }

    SECTION("Bt453 -- both address-register aliases write the same register");
    {
        Bt453 d;
        d.write(2, 0x77);
        CHECK(d.read(0) == 0x77, "written at C1C0=10, read at 00");
        d.write(0, 0x88);
        CHECK(d.read(2) == 0x88, "written at 00, read at 10");
        // A driver written for a Bt458 that "sets the read mask" at location 2 has
        // just moved the address. That is what the silicon does.
    }

    SECTION("Bt453 -- overlay registers: ADDR1..0 only, and they override the pixel");
    {
        Bt453 d;
        load(d, 0x03, 0x01, 0x01, 0x01);   // palette entry 3, so we can tell it apart
        d.write(0, 0xFD);                  // xxxx xx01 -> overlay color 1; the top bits are ignored
        d.write(3, 0xE1); d.write(3, 0xE2); d.write(3, 0xE3);
        CHECK(is(d.lookup(0x00, 1), 0xE1, 0xE2, 0xE3), "OL=01 selects overlay 1");
        CHECK(is(d.lookup(0x03, 1), 0xE1, 0xE2, 0xE3), "...whatever P0-P7 say");
        CHECK(is(d.lookup(0x03, 0), 0x01, 0x01, 0x01), "OL=00 is the palette entry");
        CHECK(is(d.lookup(0xFD, 0), 0, 0, 0), "and palette entry $FD was NOT written -- only ADDR1..0 counted");
        CHECK(d.address() == 0xFE, "the increment after blue is on the whole 8-bit register");
        d.write(3, 0xF1); d.write(3, 0xF2); d.write(3, 0xF3);   // $FE -> xx10 -> overlay 2
        CHECK(is(d.lookup(0, 2), 0xF1, 0xF2, 0xF3), "OL=10 selects overlay 2");
        d.write(3, 0xA1); d.write(3, 0xA2); d.write(3, 0xA3);   // $FF -> xx11 -> overlay 3
        CHECK(is(d.lookup(0, 3), 0xA1, 0xA2, 0xA3), "OL=11 selects overlay 3");

        d.write(2, 0xFE);
        CHECK(d.read(3) == 0xF1 && d.read(3) == 0xF2 && d.read(3) == 0xF3, "overlays read back R, G, B too");

        // The reserved slot (xx00): accepted, displayed by nothing, dirties nothing.
        d.takeDirty();
        d.write(0, 0x00);
        d.write(3, 1); d.write(3, 2); d.write(3, 3);
        CHECK(!d.takeDirty(), "a write to the reserved overlay location changes no visible color");
        CHECK(is(d.lookup(0, 0), 0, 0, 0), "and palette entry 0 is untouched by it");
    }

    SECTION("Bt453 -- palette() is all 256 entries in order, for Display::setPalette");
    {
        Bt453 d;
        load(d, 0, 0xFF, 0, 0);
        load(d, 255, 0, 0, 0xFF);
        auto pal = d.palette();
        CHECK(pal.size() == 256, "256 entries");
        CHECK(is(pal[0], 0xFF, 0, 0) && is(pal[255], 0, 0, 0xFF) && is(pal[128], 0, 0, 0),
              "each entry is its RAM word, alpha opaque");
    }

    SECTION("Bt453 -- reset blanks the table; a snapshot carries table, address and phase");
    {
        Bt453 d;
        load(d, 0x42, 0x12, 0x34, 0x56);
        d.write(0, 0x42);
        d.write(1, 0x99);          // one red byte pending, phase = green
        d.takeDirty();

        StateWriter w;
        d.serialize(w);
        Bt453 e;
        StateReader r(w.data());
        e.deserialize(r);
        CHECK(r.ok(), "the state reads back cleanly");
        CHECK(is(e.lookup(0x42), 0x12, 0x34, 0x56), "the table travels");
        CHECK(e.address() == 0x42 && e.phase() == 1, "and so do the address and the phase");
        CHECK(e.takeDirty(), "a restored chip owes the host its palette");
        e.write(1, 0x88); e.write(1, 0x77);
        CHECK(is(e.lookup(0x42), 0x99, 0x88, 0x77), "the pending red byte survived too -- the sequence completes");

        d.reset();
        CHECK(is(d.lookup(0x42), 0, 0, 0) && d.address() == 0 && d.phase() == 0,
              "reset: black, address 0, red");
    }
}
