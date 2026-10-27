#include "test.h"

#include "boards/bankmem.h"
#include "core/board.h"
#include "core/machine.h"
#include "core/statefile.h"

using namespace altair;

static MemBankBoard* addBank(Machine& m, const std::string& id, const char* card) {
    std::string err;
    auto* b = dynamic_cast<MemBankBoard*>(m.add("bankmem", id, err));
    std::string e2;
    setProperty(*b, "card", card, e2);
    return b;
}

// Bit i set for each live bank -- see MemBankBoard::activeMask().
static uint32_t active(MemBankBoard* b) { return b->activeMask(); }

// A property's text, as SHOW prints it.
static std::string prop(MemBankBoard* b, const std::string& name) {
    for (const auto& p : b->properties())
        if (p.name == name && p.get) return p.get().text(p.radix);
    return "";
}

static bool answers(MemBankBoard* b, uint16_t addr) {
    return b->decodes(BusCycle{Cycle::MemRead, addr, 0, false});
}

// An ExpandoRAM III subsystem with zeroed RAM, powered.
static MemBankBoard* addEram3(Machine& m, const char* ram = "256", const char* config = "cpm") {
    auto* b = addBank(m, "mem0", "expandoram3");
    std::string e2;
    setProperty(*b, "config", config, e2);
    setProperty(*b, "ram", ram, e2);
    setProperty(*b, "fill", "zero", e2);
    m.power();
    return b;
}

void test_bankmem() {
    SECTION("bankmem -- one board, five decoders, each owning its own decode");

    {
        // Vector Graphic: port 40, ONE-HOT select-one. 0x04 -> bank 2, not bank 4.
        Machine m;
        auto* b = addBank(m, "mem0", "vector");
        m.power();
        CHECK(active(b) == 0x01, "vector: reset forces bank 0 live (POC latch clear ~ 0x01)");
        m.bus.ioWrite(0x40, 0x04);
        CHECK(active(b) == 0x04, "vector: OUT 40,04 selects bank TWO (one-hot), nothing else live");
        m.bus.ioWrite(0x40, 0x80);
        CHECK(active(b) == 0x80, "vector: OUT 40,80 selects bank 7");

        // The OASIS quirk: bit 6 is ignored, so 0x41/0x42 land on banks 0/1.
        m.bus.ioWrite(0x40, 0x41);
        CHECK(active(b) == 0x01, "vector: 0x41 -> bank 0 (bit 6 ignored: OASIS)");
        m.bus.ioWrite(0x40, 0x42);
        CHECK(active(b) == 0x02, "vector: 0x42 -> bank 1 (OASIS)");

        // A select the card cannot decode is not silently swallowed.
        m.drainBoardLog();
        m.bus.ioWrite(0x40, 0x03);
        CHECK(active(b) == 0x02, "vector: an undecodable (non-one-hot) select leaves the bank alone");
        CHECK(!m.drainBoardLog().empty(), "vector: ...and SAYS SO");
    }

    {
        // Vector: the plane really swaps -- same address, different bytes.
        Machine m;
        auto* b = addBank(m, "mem0", "vector");
        std::string e2;
        setProperty(*b, "fill", "zero", e2);
        m.power();
        m.bus.ioWrite(0x40, 0x01);          // bank 0
        m.bus.memWrite(0x1000, 0xA0);
        m.bus.ioWrite(0x40, 0x08);          // bank 3
        m.bus.memWrite(0x1000, 0xB3);
        m.bus.ioWrite(0x40, 0x01);
        CHECK(m.bus.memRead(0x1000) == 0xA0, "vector: bank 0 still holds A0");
        m.bus.ioWrite(0x40, 0x08);
        CHECK(m.bus.memRead(0x1000) == 0xB3, "vector: bank 3 holds B3 -- the plane swapped");
        (void)b;
    }

    {
        // Cromemco 64KZ: port 40, 8-bit MASK. Several banks live AT ONCE -- this is the
        // mechanism the SIMH-derived model got wrong (it thought Cromemco was select-one).
        Machine m;
        auto* b = addBank(m, "mem0", "cromemco64kz");
        m.power();
        m.bus.ioWrite(0x40, 0x28);
        CHECK(active(b) == 0x28, "cromemco: OUT 40,28 activates banks 3 AND 5 at once");
        m.bus.ioWrite(0x40, 0x48);
        CHECK(active(b) == 0x48, "cromemco: OUT 40,48 activates banks 3 and 6");
        m.bus.ioWrite(0x40, 0x20);
        CHECK(active(b) == 0x20, "cromemco: OUT 40,20 activates bank 5 only");
        m.bus.ioWrite(0x40, 0x00);
        CHECK(active(b) == 0x00, "cromemco: OUT 40,00 deactivates every bank");

        // Two banks live over the same 64K window is a bus fight, and it is reported.
        m.drainBoardLog();
        m.bus.ioWrite(0x40, 0x28);
        (void)m.bus.memRead(0x1000);
        CHECK(!m.drainBoardLog().empty(),
              "cromemco: overlapping live banks are reported as a bus fight");
    }

    {
        // North Star HRAM: port C0. The byte is NOT a bank number -- bit 0 is an
        // on/off command and bits 1-7 are a one-hot address of the bank to toggle.
        Machine m;
        auto* b = addBank(m, "mem0", "northstar");
        m.power();
        CHECK(b->banks() == 6, "northstar: <=6 usable banks (one of bits 5/6/7 is parity)");
        CHECK(active(b) == 0x01, "northstar: reset leaves the first bank on (JP1 default)");

        // Old off, then new on -- the documented protocol.
        m.bus.ioWrite(0xC0, 0x03);          // bit1 set, bit0=1 -> turn OFF bank at bit1
        CHECK(active(b) == 0x00, "northstar: OUT C0,03 turns the bit-1 bank OFF");
        m.bus.ioWrite(0xC0, 0x04);          // bit2 set, bit0=0 -> turn ON bank at bit2
        CHECK(active(b) == 0x02, "northstar: OUT C0,04 turns the bit-2 bank ON");

        // Skip the "old off" step and two banks are on at once -- the trap the manual warns of.
        m.bus.ioWrite(0xC0, 0x02);          // turn the bit-1 bank on WITHOUT turning bit-2 off
        CHECK(active(b) == 0x03, "northstar: banks toggle individually (both on if you skip old-off)");

        // A byte with no single bank bit, and a bank the board does not carry, both complain.
        m.drainBoardLog();
        m.bus.ioWrite(0xC0, 0x06);          // bits 1 and 2 -> not one-hot
        CHECK(!m.drainBoardLog().empty(), "northstar: a non-one-hot byte is reported");
        m.bus.ioWrite(0xC0, 0x80);          // bit 7 -> a bank this 6-bank board lacks
        CHECK(!m.drainBoardLog().empty(), "northstar: a bank the board does not carry is reported");
    }

    {
        // ExpandoRAM II: port FF, binary page select (our documented approximation of the
        // real 82S130 PROM decode).
        Machine m;
        auto* b = addBank(m, "mem0", "expandoram2");
        std::string e2;
        setProperty(*b, "fill", "zero", e2);
        m.power();
        m.bus.ioWrite(0xFF, 0x00);
        m.bus.memWrite(0x1000, 0xA0);
        m.bus.ioWrite(0xFF, 0x03);
        CHECK(active(b) == 0x08, "expandoram2: OUT FF,03 selects page 3 (binary)");
        m.bus.memWrite(0x1000, 0xB3);
        m.bus.ioWrite(0xFF, 0x00);
        CHECK(m.bus.memRead(0x1000) == 0xA0, "expandoram2: page 0 still holds A0");
        m.bus.ioWrite(0xFF, 0x03);
        CHECK(m.bus.memRead(0x1000) == 0xB3, "expandoram2: page 3 holds B3 -- the page swapped");
    }

    {
        // ExpandoRAM II with a common-memory partition (EX-48): the top 16K is shared by
        // every bank -- what a banked CP/M's resident OS and ?bank routine live in -- and
        // only 0000-BFFF swaps. This is what lets SD Systems banked CP/M 3 (bank number
        // straight to port FF, common at C000) run.
        Machine m;
        auto* b = addBank(m, "mem0", "expandoram2");
        std::string e2;
        setProperty(*b, "partition", "ex48", e2);
        setProperty(*b, "ram", "256", e2);      // 16K common + 5x48K banked
        setProperty(*b, "fill", "zero", e2);
        m.power();
        CHECK(e2.empty(), "eram2/ex48: partition+ram set without error");
        CHECK(b->banks() == 5, "eram2/ex48: 256K -> 5 banks of 48K (plus 16K common)");

        // The banked window swaps between pages...
        m.bus.ioWrite(0xFF, 0x00);
        m.bus.memWrite(0x1000, 0xA0);           // banked region, page 0
        m.bus.ioWrite(0xFF, 0x03);
        m.bus.memWrite(0x1000, 0xB3);           // banked region, page 3
        m.bus.ioWrite(0xFF, 0x00);
        CHECK(m.bus.memRead(0x1000) == 0xA0, "eram2/ex48: page 0 keeps its own banked byte");
        m.bus.ioWrite(0xFF, 0x03);
        CHECK(m.bus.memRead(0x1000) == 0xB3, "eram2/ex48: page 3 keeps its own banked byte");

        // ...but the common region (C000-FFFF) is the SAME bytes in every bank.
        m.bus.ioWrite(0xFF, 0x00);
        m.bus.memWrite(0xE000, 0x5A);           // write common while page 0 is in
        m.bus.ioWrite(0xFF, 0x04);
        CHECK(m.bus.memRead(0xE000) == 0x5A, "eram2/ex48: common survives a bank switch (page 4 sees it)");
        m.bus.memWrite(0xE000, 0x99);           // change it from page 4
        m.bus.ioWrite(0xFF, 0x01);
        CHECK(m.bus.memRead(0xE000) == 0x99, "eram2/ex48: common is one shared region, not per-bank");
    }

    {
        // honors_phantom: an ExpandoRAM II sitting UNDER the SBC-200 boot PROM. With
        // honors=read the card steps off the bus for a phantomed READ (the PROM drives it)
        // but STILL takes the WRITE -- so the cold-boot loader writes the system into the
        // RAM the PROM is shadowing. This is what makes the banked CP/M 3 machine boot.
        Machine m;
        auto* b = addBank(m, "mem0", "expandoram2");
        std::string e2;
        setProperty(*b, "partition", "ex48", e2);
        setProperty(*b, "honors_phantom", "read", e2);
        setProperty(*b, "fill", "zero", e2);
        m.power();
        CHECK(e2.empty(), "phantom: partition+honors_phantom set without error");

        BusCycle rd{Cycle::MemRead, 0xE000, 0, false};       // common region, no phantom
        CHECK(b->decodes(rd), "phantom: with no PHANTOM* the card answers reads normally");
        rd.phantom = true;
        CHECK(!b->decodes(rd), "phantom(read): a phantomed READ is left to the PROM");
        BusCycle wr{Cycle::MemWrite, 0xE000, 0x5A, false};
        wr.phantom = true;
        CHECK(b->decodes(wr), "phantom(read): a phantomed WRITE still falls through to RAM");

        // honors=none (the default) never steps aside -- backward compatible.
        setProperty(*b, "honors_phantom", "none", e2);
        BusCycle rd2{Cycle::MemRead, 0xE000, 0, false};
        rd2.phantom = true;
        CHECK(b->decodes(rd2), "phantom(none): the card ignores PHANTOM* entirely");
    }

    {
        // partition is an ExpandoRAM II concept; the other cards are whole-64K planes.
        Machine m;
        auto* b = addBank(m, "mem0", "vector");
        std::string err;
        bool ok = setProperty(*b, "partition", "ex48", err);
        CHECK(!ok && !err.empty(), "partition is refused on non-expandoram2 cards");

        // ram in KB derives the bank count; whole-plane (none) -> 64K per bank.
        setProperty(*b, "ram", "256", err);
        CHECK(b->banks() == 4, "vector: ram=256K -> 4 whole-64K planes");
    }

    {
        // The select port is per-card, and two banked cards on the same port are a real
        // I/O collision the bus reports by name -- exactly the old vram/b810 test, now
        // between two bankmem boards.
        Machine m;
        auto* a = addBank(m, "mem0", "vector");
        auto* c = addBank(m, "mem1", "cromemco64kz");
        m.power();
        BusCycle cyc{Cycle::IoWrite, 0x40, 0, false};
        CHECK(m.bus.respondersTo(cyc).size() == 2, "two banked cards both claim port 40");
        m.bus.clearLog();
        m.bus.ioWrite(0x40, 0x01);
        CHECK(!m.bus.drain().empty(), "and the I/O collision is REPORTED, naming both");
        (void)a; (void)c;
    }

    {
        // ExpandoRAM III, CP/M map: reset is page 0, the 48K window swaps, C000 is common.
        Machine m;
        auto* b = addEram3(m);
        CHECK(active(b) == 0x01, "eram3: reset puts page 0 in (U13 cleared)");
        CHECK(b->banks() == 5, "eram3: one board carries pages 0-4");
        CHECK(prop(b, "active") == "page 0 + common", "eram3: active reads page 0 + common");
        m.bus.memWrite(0x1000, 0xA0);
        m.bus.memWrite(0x9000, 0xA2);
        m.bus.ioWrite(0xFF, 0x03);
        CHECK(active(b) == 0x08, "eram3: OUT FF,03 selects page 3");
        m.bus.memWrite(0x1000, 0xB3);
        m.bus.memWrite(0xE000, 0x5A);
        m.bus.ioWrite(0xFF, 0x04);           // page 4 lives in bank 3, beside common
        m.bus.memWrite(0x9000, 0xC4);
        m.bus.ioWrite(0xFF, 0x00);
        CHECK(m.bus.memRead(0x1000) == 0xA0, "eram3: page 0 keeps its own byte at 1000");
        CHECK(m.bus.memRead(0x9000) == 0xA2, "eram3: page 4's 8000-BFFF is not page 0's");
        CHECK(m.bus.memRead(0xE000) == 0x5A, "eram3: C000-FFFF is common to every page");
        m.bus.ioWrite(0xFF, 0x03);
        CHECK(m.bus.memRead(0x1000) == 0xB3, "eram3: page 3 holds B3");
        m.bus.ioWrite(0xFF, 0x04);
        CHECK(m.bus.memRead(0x9000) == 0xC4, "eram3: page 4 holds C4");
        CHECK(answers(b, 0x0000) && answers(b, 0x4000), "eram3: page 4 answers all of 0000-BFFF");

        // Only D0-D3 are the page: the high nibble goes nowhere.
        m.bus.ioWrite(0xFF, 0xF3);
        CHECK(active(b) == 0x08, "eram3: OUT FF,F3 is page 3 (low nibble only)");
        CHECK(prop(b, "active") == "page 3 + common", "eram3: active reads page 3");
    }

    {
        // OASIS map: common is 0000-3FFF and the pages swap 4000-FFFF.
        Machine m;
        auto* b = addEram3(m, "256", "oasis");
        m.bus.memWrite(0x2000, 0x11);
        m.bus.memWrite(0xF000, 0xA0);
        m.bus.ioWrite(0xFF, 0x02);
        CHECK(m.bus.memRead(0x2000) == 0x11, "eram3/oasis: 0000-3FFF is common");
        m.bus.memWrite(0xF000, 0xB2);
        m.bus.ioWrite(0xFF, 0x04);
        CHECK(answers(b, 0x4000) && answers(b, 0xF000), "eram3/oasis: page 4 answers 4000-FFFF");
        m.bus.memWrite(0xF000, 0xC4);
        m.bus.ioWrite(0xFF, 0x00);
        CHECK(m.bus.memRead(0xF000) == 0xA0, "eram3/oasis: page 0 keeps its F000");
        m.bus.ioWrite(0xFF, 0x02);
        CHECK(m.bus.memRead(0xF000) == 0xB2, "eram3/oasis: page 2 keeps its F000");
        m.bus.ioWrite(0xFF, 0x04);
        CHECK(m.bus.memRead(0xF000) == 0xC4, "eram3/oasis: page 4 keeps its F000");
    }

    {
        // A page with no cell on this subsystem: nothing answers the window, common does.
        Machine m;
        auto* b = addEram3(m);
        m.drainBoardLog();
        m.bus.ioWrite(0xFF, 0x07);
        CHECK(active(b) == 0x00, "eram3: one board has no page 7");
        CHECK(!answers(b, 0x0000) && !answers(b, 0xB000), "eram3: nothing answers 0000-BFFF");
        CHECK(answers(b, 0xC000), "eram3: common still answers C000");
        CHECK(prop(b, "active") == "page 7 (no RAM) + common", "eram3: active says no RAM");
        CHECK(m.drainBoardLog().size() == 1, "eram3: ...and the board says so once");

        m.bus.ioWrite(0xFF, 0x0A);
        CHECK(!answers(b, 0x0000) && !m.drainBoardLog().empty(), "eram3: page A has no RAM");
    }

    {
        // Two boards carry pages 0-9; pages A-F are still empty.
        Machine m;
        auto* b = addEram3(m, "512");
        std::string e2;
        CHECK(b->banks() == 10, "eram3: two boards carry pages 0-9");
        CHECK(prop(b, "ram") == "512", "eram3: ram reads back 512");
        m.bus.ioWrite(0xFF, 0x07);
        CHECK(active(b) == 0x80, "eram3: two boards: page 7 is in");
        m.bus.ioWrite(0xFF, 0x09);
        CHECK(answers(b, 0x0000) && answers(b, 0x4000) && answers(b, 0x8000),
              "eram3: page 9 answers all of 0000-BFFF");
        m.bus.memWrite(0x8000, 0x99);
        m.bus.ioWrite(0xFF, 0x08);
        m.bus.memWrite(0x8000, 0x88);
        m.bus.ioWrite(0xFF, 0x09);
        CHECK(m.bus.memRead(0x8000) == 0x99, "eram3: pages 8 and 9 are separate RAM");

        // Every page, in both maps, fills its whole 48K window with exactly one cell per
        // quarter, and no page cell sits over common (that would be a bus fight).
        for (const char* cfg : {"cpm", "oasis"}) {
            std::string err;
            setProperty(*b, "config", cfg, err);
            bool full = true;
            m.drainBoardLog();
            for (int p = 0; p <= 9; ++p) {
                m.bus.ioWrite(0xFF, (uint8_t)p);
                for (uint16_t q = 0; q < 4; ++q) {
                    if (!answers(b, (uint16_t)(q * 0x4000))) full = false;
                    (void)m.bus.memRead((uint16_t)(q * 0x4000));
                }
            }
            const std::string fill = std::string("eram3/") + cfg + ": pages 0-9 each fill all 64K";
            const std::string fight = std::string("eram3/") + cfg + ": no page overlaps common";
            CHECK(full, fill.c_str());
            CHECK(m.drainBoardLog().empty(), fight.c_str());
        }
        setProperty(*b, "config", "cpm", e2);
        m.drainBoardLog();
        for (int p = 0x0A; p <= 0x0F; ++p) m.bus.ioWrite(0xFF, (uint8_t)p);
        CHECK(m.drainBoardLog().size() == 6, "eram3: pages A-F have no RAM on two boards");
        CHECK(!answers(b, 0x0000), "eram3: ...and nothing answers the window");
    }

    {
        // The S3 bank enables: a bank switched off answers nothing.
        Machine m;
        auto* b = addEram3(m);
        std::string err;
        CHECK(setProperty(*b, "bank_enable", "FE", err), "eram3: bank_enable takes FE");
        CHECK(!answers(b, 0x0000), "eram3: bank 0 off: page 0's 0000-3FFF is gone");
        CHECK(answers(b, 0x4000) && answers(b, 0x8000) && answers(b, 0xC000),
              "eram3: bank 0 off: 4000-BFFF and common stay");
        CHECK(setProperty(*b, "bank_enable", "F7", err), "eram3: bank_enable takes F7");
        CHECK(answers(b, 0x0000) && !answers(b, 0xC000), "eram3: bank 3 off: common is gone");
        m.bus.ioWrite(0xFF, 0x04);
        CHECK(!answers(b, 0x0000) && !answers(b, 0x8000), "eram3: bank 3 off: page 4 is gone");
    }

    {
        // What the III refuses, and what the other cards refuse of the III's switches.
        Machine m;
        auto* b = addEram3(m);
        std::string err;
        CHECK(!setProperty(*b, "partition", "ex48", err) && err.find("config=") != std::string::npos,
              "eram3: partition is refused, pointing to config");
        CHECK(setProperty(*b, "partition", "none", err), "eram3: partition=none is accepted");
        CHECK(!setProperty(*b, "ram", "300", err), "eram3: ram=300 is refused");
        CHECK(!setProperty(*b, "banks", "7", err), "eram3: banks=7 is refused");
        CHECK(setProperty(*b, "banks", "10", err) && prop(b, "ram") == "512",
              "eram3: banks=10 is two boards (CONFIG SAVE writes banks before ram)");

        auto* v = addBank(m, "mem1", "vector");
        CHECK(!setProperty(*v, "config", "oasis", err), "vector: config=oasis is refused");
        CHECK(!setProperty(*v, "bank_enable", "FE", err), "vector: bank_enable=FE is refused");
        CHECK(setProperty(*v, "config", "cpm", err) && setProperty(*v, "bank_enable", "FF", err),
              "vector: the factory values load back (CONFIG SAVE writes them)");
    }

    {
        // SNAPSHOT / RESTORE of the III: the page and the RAM travel.
        MemBankBoard src, dst;
        src.id = "mem0";
        dst.id = "mem0";
        std::string e2;
        for (MemBankBoard* b : {&src, &dst}) {
            setProperty(*b, "card", "expandoram3", e2);
            setProperty(*b, "ram", "512", e2);
            setProperty(*b, "fill", "zero", e2);
            b->power();
        }
        src.write(BusCycle{Cycle::IoWrite, 0xFF, 0x06, false});
        src.write(BusCycle{Cycle::MemWrite, 0x5000, 0x66, false});
        StateWriter w;
        src.serialize(w);
        StateReader r(w.data());
        dst.deserialize(r);
        CHECK(r.ok(), "eram3 snapshot: restore consumed the blob cleanly");
        CHECK(active(&dst) == 0x40, "eram3 snapshot: page 6 travelled");
        CHECK(prop(&dst, "active") == "page 6 + common", "eram3 snapshot: the latch travelled");
        uint8_t v = 0;
        CHECK(dst.peek(0x5000, v) && v == 0x66, "eram3 snapshot: the stored byte travelled");
    }

    {
        // SNAPSHOT / RESTORE: the store and the live enables travel; the geometry is config.
        MemBankBoard src, dst;
        src.id = "mem0";
        dst.id = "mem0";
        std::string e2;
        setProperty(src, "card", "cromemco64kz", e2);
        setProperty(src, "fill", "zero", e2);
        setProperty(dst, "card", "cromemco64kz", e2);
        setProperty(dst, "fill", "zero", e2);
        src.power();
        dst.power();

        src.write(BusCycle{Cycle::IoWrite, 0x40, 0x08, false});   // bank 3 live
        src.write(BusCycle{Cycle::MemWrite, 0x2000, 0x5A, false});
        CHECK(active(&src) == 0x08, "snapshot: source has bank 3 live");

        StateWriter w;
        src.serialize(w);
        StateReader r(w.data());
        dst.deserialize(r);
        CHECK(r.ok(), "snapshot: restore consumed the blob cleanly");
        CHECK(active(&dst) == 0x08, "snapshot: the live bank travelled");
        uint8_t v = 0;
        CHECK(dst.peek(0x2000, v) && v == 0x5A, "snapshot: the stored byte travelled");
    }
}
