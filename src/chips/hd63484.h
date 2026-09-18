#pragma once
//
// Hitachi HD63484 ACRTC -- Advanced CRT Controller. A CHIP, NOT A CARD, exactly as the
// FD1771 and the 8257 next door are. The cadzilla video board has one; the next board
// that turns up with an ACRTC gets it for free.
//
// A CRT controller with a DRAWING PROCESSOR and its OWN frame memory: up to 1 M words
// of 16 bits that the host never addresses directly. The host talks to it through two
// locations and a pair of FIFOs -- it sets the raster timing in ~30 control registers,
// then draws by issuing COMMANDS ("line to (x, y)", "clear this rectangle") that the
// chip executes against the frame memory in logical X-Y pixel coordinates at 1, 2, 4,
// 8 or 16 bits per pixel. The picture is whatever is in that memory, scanned out through
// the display-control registers.
//
// SOURCES (docs/sources.md). Modeled from the Hitachi datasheet (reference/Hitachi
// HD63484 ACRTC.md) and the Hitachi HD63484 User's Manual -- every register field, every
// status rule and every opcode below cites one of them. MAME's hd63484.cpp (BSD-3-Clause,
// (c) Angelo Salese, Sandro Ronco; LICENSE-MAME-HD63484) was read as the STRUCTURAL
// basis -- the shape of the register file, the FIFO, the command dispatch and the
// scan-out -- and where it and the datasheet disagree the datasheet won (the FIFO is 8
// words deep, not 16; a line's pattern scan starts at PPX, not PSX). Nothing here was
// derived by running it.
//
// It knows nothing about S-100 and nothing about the board's ports. The CARD decodes two
// consecutive I/O locations onto some base and forwards each as RS (0 or 1); the card
// also decided how much DRAM it fitted (the constructor) and how the 16-bit frame-memory
// word reaches its shift register (see scanline()).
//
// ---------------------------------------------------------------------------
// THE HOST INTERFACE (datasheet "Hardware Access"; manual 5.1-5.4)
//
//   RS=0  write -> ADDRESS REGISTER (AR), 8 bits: which control register RS=1 reaches.
//         read  -> STATUS REGISTER (SR):  CER ARD CED LPD RFF RFR WFR WFE  (D7..D0)
//   RS=1  the 16-bit control register AR names, or the FIFOs when AR = 0.
//
// 8-BIT MPU MODE (manual 5.2, 5.4, 6.2), which is the only mode an 8-bit S-100 host can
// use and the only one modeled: every control register is 16 bits, so each is TWO byte
// locations -- an EVEN AR reaches the high byte, an ODD AR the low byte. Accesses to
// r80-rFF (the timing and display-control RAM) auto-increment AR by one so a block load
// needs one AR write; accesses to r00-r7F do not. The FIFOs sit at AR = 0 and pair bytes
// HIGH THEN LOW into 16-bit words with AR left alone.
//
// THE FIFOs (manual 5.3, 5.4): a write FIFO and a read FIFO, EACH 8 WORDS (16 bytes).
// Commands and their parameters go in through the write FIFO; RPR/RPTN/RD results come
// back through the read FIFO. The status bits are the flow control a driver polls:
//   WFE  write FIFO empty (safe to write up to 8 words)     WFR  not full (1 word ok)
//   RFR  read FIFO has a word                               RFF  read FIFO full
//   CED  the chip can take a NEW command (cleared when one is written, set when it ends)
//   CER  an undefined command or a bad parameter; cleared only by ABT
//   ARD  a drawing crossed the defined area (AREA modes); cleared by RPR or ABT
// RES* and ABT (CCR bit 15) both put SR at $23 = CED|WFR|WFE with both FIFOs cleared.
//
// ---------------------------------------------------------------------------
// COORDINATES AND THE FRAME MEMORY (manual 5.10, 6.6-6.8, ORG)
//
// The frame memory is sequential 16-bit words. A logical pixel of bpp bits lives in one
// word at DOT ADDRESS d, occupying bits [d*bpp .. d*bpp+bpp-1] -- pixel 0 is the LOW
// bits (manual ORG-3: "bit position 4-7 at word $25" is DPD=4 at 4 bpp). X increases
// through the dot addresses of a word and then into the next word; a raster is MW words
// wide (the screen's Memory Width register), and +Y is the raster ABOVE, i.e. MW words
// LOWER in memory -- ORG sets the origin's physical word address and the axes point
// right and UP from it. Scan-out reads rasters from the Start Address upward in memory,
// so the top raster on the screen is the lowest address.
//
// WHICH PIXEL IS LEFTMOST is not the chip's decision: it puts a word on its MAD bus and
// the BOARD's shift register serializes it. For the drawing engine's +X to be "to the
// right", the board must shift the LOW dot address out first, and that is the only
// wiring under which the ACRTC's own manual examples make sense; scanline() assumes it.
//
// ---------------------------------------------------------------------------
// WHAT IS MODELED, AND WHAT IS NOT (docs/boards/cadzilla.md has the honest list)
//
//   Register file: all of it, with the auto-increment rule.       FIFOs and SR: all.
//   Commands: ORG WPR RPR WPTN RPTN / RD WT MOD DRD DWT DMOD CLR SCLR / AMOVE RMOVE
//             ALINE RLINE ARCT RRCT APLL RPLL APLG RPLG AFRCT RFRCT DOT, with every
//             OPM, every COL and every AREA mode. The remaining opcodes (CPY SCPY CRCL
//             ELPS arcs PAINT PTN AGCPY RGCPY) are RECOGNIZED -- their parameters are
//             consumed so the stream stays in step -- but not executed, and they set
//             CER so a guest can tell.
//   Scan-out: the three background screens (upper/base/lower) stacked by SP0/SP1/SP2 and
//             the window over them, graphic screens only, non-interlaced, GAI +1/+2/+4/+8.
//   Timing:   NONE. Drawing is instantaneous at the moment the last parameter lands, the
//             raster counter is not modeled, and DTACK/wait states do not exist. A guest
//             that times a command sees an infinitely fast ACRTC.
//   Not modeled: zoom, character screens, block/graphic cursors, light pen, blink
//             attributes, DMA handshaking (DRD/DWT/DMOD run in the manual's "under
//             program control" mode), interlace, master/slave sync.
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace altair {

class StateWriter;  // core/statefile.h -- SNAPSHOT/RESTORE
class StateReader;

class Hd63484 {
public:
    // `vramWords` is how much frame memory the BOARD fitted, in 16-bit words. The chip
    // emits a 20-bit word address; a smaller memory aliases (the DRAM does not decode
    // the high bits), which is what the silicon on a real board does too.
    explicit Hd63484(size_t vramWords = 1u << 20);

    // ---- THE TWO HOST LOCATIONS, 8-bit MPU mode. rs is the RS pin. ----
    uint8_t read(bool rs);
    void    write(bool rs, uint8_t v);

    // RES*: SR = $23, CCR ABT = 1 and every other CCR bit 0, OMR M/S and STR = 0, both
    // FIFOs cleared. Every other register keeps its value (manual 5.5, 5.6; App Note
    // Table 7-1) -- "after power-up neither the registers nor the RAM hold defined
    // values", so power() below is what zeroes them.
    void reset();
    // Power-on: everything zero, then reset(). The frame memory comes up zero.
    void power();

    // ---- THE SCREEN, for the board's render loop ----

    // Is the chip displaying? OMR STR (start) and DCR SE1 (the base screen, which "must
    // always be defined"). Off, the board paints black.
    bool displayOn() const;

    // The visible raster in pixels: (HDW+1) memory cycles wide, each cycle one word (or
    // 2/4/8 with GAI) at the current bits per pixel; SP0+SP1+SP2 rasters tall for the
    // screens DCR enables. Zero if the timing registers describe nothing.
    int displayWidth() const;
    int displayHeight() const;
    int bitsPerPixel() const;  // 1, 2, 4, 8 or 16 from CCR GBM

    // Visible raster `y` (0 = top) as pixel values, LOW dot address first, at most
    // out.size() of them; pixels past displayWidth() are left untouched. A pixel is the
    // bpp-bit field, so at 16 bpp it is the whole word.
    void scanline(int y, std::span<uint16_t> out) const;

    // Has anything the picture depends on changed since last asked? Consuming.
    bool takeDirty() {
        bool d = dirty_;
        dirty_ = false;
        return d;
    }

    // IRQ* as the chip would drive it: any SR flag whose CCR enable bit is set. The
    // board may wire it to a bus interrupt; nothing here does.
    bool irq() const { return (sr_ & (ccr() & 0xFF)) != 0; }

    // ---- For tests: registers, memory and the drawing state, straight ----
    uint8_t  status() const { return sr_; }
    uint8_t  ar() const { return ar_; }
    uint16_t reg(uint8_t even) const;      // control register `even` (r02, r84, ...)
    uint16_t ccr() const { return reg(0x02); }
    uint16_t omr() const { return reg(0x04); }
    uint16_t dcr() const { return reg(0x06); }
    uint16_t param(uint8_t rn) const;      // drawing parameter register Pr00..Pr13
    uint16_t pattern(uint8_t pra) const { return pram_[pra & 0x0F]; }
    int16_t  cpx() const { return cpx_; }
    int16_t  cpy() const { return cpy_; }
    std::span<const uint16_t> vram() const { return vram_; }
    uint16_t peekWord(uint32_t addr) const { return vram_[addr & vmask_]; }
    void     pokeWord(uint32_t addr, uint16_t v) { vram_[addr & vmask_] = v; dirty_ = true; }
    int      writeFifoWords() const { return wfifoN_ / 2; }
    int      readFifoWords() const { return rfifoN_ / 2; }

    // SNAPSHOT/RESTORE (DESIGN.md 13): registers, FIFOs, the command in flight, the
    // drawing state, the pattern RAM and the whole frame memory (nothing else holds it).
    void serialize(StateWriter& w) const;
    void deserialize(StateReader& r);

private:
    // ---- register file ----
    uint16_t regWord(uint8_t even) const { return (uint16_t)((reg_[even & 0xFE] << 8) | reg_[(even & 0xFE) | 1]); }
    void     regByteWritten(uint8_t addr);   // side effects of a control-register byte write
    uint8_t  regByteRead(uint8_t addr) const;

    // ---- FIFOs (bytes; a word is two, high then low) ----
    void pushWrite(uint8_t b);
    bool popWrite(uint8_t& b);
    void pushRead(uint8_t b);
    bool popRead(uint8_t& b);
    void updateFifoStatus();
    void abort();

    // ---- the command engine ----
    void processFifo();                      // consume words from the write FIFO
    void commandWord(uint16_t w);            // one word of command or parameter
    int  paramsFor(uint16_t opcode) const;   // how many parameter words (-1 = undefined)
    void execute();                          // all parameters are in
    void commandEnd() { sr_ |= kCED; }
    void commandError() { sr_ |= kCER; }

    // ---- drawing (manual 6.6-6.8) ----
    void     drawLine(int x0, int y0, int x1, int y1);          // excludes (x1, y1)
    void     drawPixel(int x, int y, int patX, int patY);       // through COL/OPM/AREA
    bool     areaAllows(int x, int y);                          // false = suppressed/stopped
    bool     stopped_ = false;                                  // AREA 001/101 fired
    void     wordAddress(int x, int y, uint32_t& addr, int& shift) const;
    uint16_t colorFor(int x, int patX, int patY, bool& draw) const;
    uint16_t applyOpm(uint16_t data, uint16_t color, uint32_t addr, int shift, int bpp) const;
    void     fillRect(int x1, int y1);                          // AFRCT/RFRCT body
    void     clearBlock(uint16_t d, int16_t ax, int16_t ay, bool masked, int mm);
    uint16_t modify(uint16_t data, uint16_t d, int mm) const;   // MM under MASK

    // RWP / DP as 20-bit word addresses plus the screen number they carry.
    uint32_t rwp() const { return rwp_[rwpDn_]; }
    void     setRwp(uint32_t a) { rwp_[rwpDn_] = a & 0xFFFFF; }
    uint32_t mw(int dn) const { return regWord((uint8_t)(0xC2 + dn * 8)) & 0x0FFF; }
    uint32_t sar(int dn) const;

    void     vramWrite(uint32_t addr, uint16_t v) { vram_[addr & vmask_] = v; dirty_ = true; }
    uint16_t vramRead(uint32_t addr) const { return vram_[addr & vmask_]; }

    // Status bits.
    static constexpr uint8_t kCER = 0x80, kARD = 0x40, kCED = 0x20, kLPD = 0x10;
    static constexpr uint8_t kRFF = 0x08, kRFR = 0x04, kWFR = 0x02, kWFE = 0x01;
    static constexpr int     kFifoBytes = 16;  // 8 words

    // ---- state ----
    uint8_t  reg_[256] = {};        // the control registers, byte-addressed, big-endian pairs
    uint8_t  ar_ = 0;
    uint8_t  sr_ = 0;

    uint8_t  wfifo_[kFifoBytes] = {};
    int      wfifoN_ = 0;
    uint8_t  rfifo_[kFifoBytes] = {};
    int      rfifoN_ = 0;
    uint8_t  fifoHi_ = 0;           // the high byte of a word in flight (8-bit pairing)
    bool     fifoHalf_ = false;     // true = a high byte has been written, low is next

    // The command in flight: its opcode, its parameters so far, how many it wants.
    bool     inCommand_ = false;
    uint16_t cmd_ = 0;
    std::vector<uint16_t> params_;
    int      paramsWanted_ = 0;
    // DWT/DMOD/DRD under program control: words still to move after the parameters.
    int      xferLeft_ = 0;
    int      xferAx_ = 0, xferAy_ = 0, xferX_ = 0, xferY_ = 0;
    uint32_t xferBase_ = 0;
    bool     xferRead_ = false;

    // Drawing parameter registers (Pr00..Pr13) and the pattern RAM.
    uint16_t cl0_ = 0, cl1_ = 0, ccmp_ = 0, edg_ = 0, mask_ = 0xFFFF;
    uint8_t  ppx_ = 0, ppy_ = 0, pzcx_ = 0, pzcy_ = 0;
    uint8_t  psx_ = 0, psy_ = 0, pex_ = 0, pey_ = 0, pzx_ = 0, pzy_ = 0;
    int16_t  xmin_ = 0, ymin_ = 0, xmax_ = 0, ymax_ = 0;
    uint32_t rwp_[4] = {};
    uint8_t  rwpDn_ = 0;
    uint32_t orgDpa_ = 0;           // ORG: the origin's word address...
    uint8_t  orgDpd_ = 0;           // ...and dot address
    uint8_t  orgDn_ = 0;            // ...on which screen
    int16_t  cpx_ = 0, cpy_ = 0;    // the current pointer
    uint16_t pram_[16] = {};

    std::vector<uint16_t> vram_;
    uint32_t vmask_;
    bool     dirty_ = true;
};

} // namespace altair
