#include "boards/cadzilla.h"

#include "core/statefile.h"
#include "host/display.h"

#include <string>
#include <vector>

namespace altair {
namespace {

// The injected host video service (setDisplay), borrowed. Null on the bench and in a
// headless build with nothing wired -- pump() then simply does not draw.
Display* g_display = nullptr;

// The board's shift register: 8 words per display fetch, 16 pixels of 8 bits each.
constexpr int kWordsPerFetch  = 8;
constexpr int kPixelsPerFetch = 16;

// THE MONITOR MODES. VESA / industry-standard timings reduced to the board's units: memory
// cycles of 16 pixels horizontally, rasters vertically. Where a VESA porch is not a whole
// number of cycles it is rounded and the line total kept (800x600: 88 px back porch -> 6
// cycles = 96, 40 px front porch -> 2 = 32; 1024x768: 136 px sync -> 8 cycles, 24 px front
// porch -> 2), which is what a timing PROM on a real card would have done.
//
//                name         w     h   hsw hbp hfp  vsw vbp vfp     pixel clock
const CadzillaBoard::Mode kModes[] = {
    {"640x400",   640,  400,   6,  3,  1,   2, 35, 12},   // 25.175 MHz, 70 Hz (VGA text timing)
    {"640x480",   640,  480,   6,  3,  1,   2, 33, 10},   // 25.175 MHz, 60 Hz
    {"800x600",   800,  600,   8,  6,  2,   4, 23,  1},   // 40.000 MHz, 60 Hz
    {"1024x768", 1024,  768,   8, 10,  2,   6, 29,  3},   // 65.000 MHz, 60 Hz
};

} // namespace

const CadzillaBoard::Mode& CadzillaBoard::mode(int i) {
    if (i < 0 || i >= modeCount()) i = 1;
    return kModes[i];
}
int CadzillaBoard::modeCount() { return (int)(sizeof kModes / sizeof kModes[0]); }

void CadzillaBoard::setDisplay(Display* d) { g_display = d; }

// 2 MB (1 M sixteen-bit words) fixed -- the reference design's SRAM fit, not a strap.
CadzillaBoard::CadzillaBoard() : acrtc_(kVramWords) {}

// ---------------------------------------------------------------------------
// Bus: one 8-port block from BASE, no memory. BASE+3 is not decoded; BASE+1 (MODE) is
// write-only -- both exactly like the Dazzler's format port floats on a read. The ACRTC's
// own RS=0/RS=1 are NOT adjacent: RS=0 is BASE+0, RS=1 is BASE+2, with MODE between them
// (see the header comment for the decode this reflects). Every ACRTC access ends by
// calling intChanged(), since acrtc_.irq() -- SR against CCR's enables -- can move on any
// register write, any FIFO push, or any FIFO pop.
// ---------------------------------------------------------------------------
bool CadzillaBoard::decodes(const BusCycle& c) const {
    if (!enabled_) return false;
    if (c.type != Cycle::IoRead && c.type != Cycle::IoWrite) return false;
    uint8_t off = (uint8_t)(c.port() - port_);
    if (off > 7) return false;
    if (off == 3) return false;                                 // undecoded gap
    if (off == 1) return c.type == Cycle::IoWrite;               // MODE: write-only
    return true;                                                 // 0,2: ACRTC; 4-7: Bt453
}

uint8_t CadzillaBoard::read(const BusCycle& c) {
    uint8_t off = (uint8_t)(c.port() - port_);
    uint8_t v;
    if (off == 0 || off == 2) {
        v = acrtc_.read(off == 2);      // RS: BASE+0 = 0 (status), BASE+2 = 1 (data/FIFO)
        intChanged();
    } else {
        v = dac_.read(off & 3);         // C1C0 = A1A0 (off is 4..7 here)
    }
    return v;
}

void CadzillaBoard::write(const BusCycle& c) {
    uint8_t off = (uint8_t)(c.port() - port_);
    if (off == 0 || off == 2) {
        acrtc_.write(off == 2, c.data);
        intChanged();
        return;
    }
    if (off == 1) {
        if (c.data != modeReg_) dirty_ = true;      // AMODE moves the picture; the rest is status
        modeReg_ = c.data;
        return;
    }
    dac_.write(off & 3, c.data);
}

// ---------------------------------------------------------------------------
// Lifecycle. RESET* reaches the ACRTC's RES* pin; the Bt453 has no reset pin, so a warm
// reset keeps the palette a driver loaded (the picture blanks because the ACRTC stopped).
// ---------------------------------------------------------------------------
void CadzillaBoard::reset(Reset r) {
    if (r == Reset::Bus) {
        acrtc_.reset();
        // MODE is glue logic on the same RESET* line as the ACRTC, not a chip register with
        // its own reset behavior to cite -- clearing it deterministically (rather than
        // leaving it, which real flip-flops might or might not do) means a driver must
        // reprogram both the ACRTC's timing and the board's glue after a reset, which is
        // the simplest thing to be right about.
        if (modeReg_ != 0) dirty_ = true;
        modeReg_ = 0;
        dirty_   = true;
        intChanged();   // RESET* clears CCR's enables (acrtc_.reset()) -- IRQ* stands down
    }
}

void CadzillaBoard::power() {
    acrtc_.power();
    dac_.reset();
    modeReg_ = 0;
    dirty_   = true;   // the monitor shows its (black) frame from power-on, signal or not
    intChanged();       // a fresh chip asserts nothing
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
void CadzillaBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    acrtc_.serialize(w);
    dac_.serialize(w);
    w.u8(modeReg_);
}

void CadzillaBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    acrtc_.deserialize(r);
    dac_.deserialize(r);
    modeReg_ = r.u8();
    dirty_   = true;  // the restored picture owes the host a full redraw
    intChanged();      // the restored SR/CCR may be mid-interrupt
}

// ---------------------------------------------------------------------------
// The programmed picture in the monitor's frame.
//
// Horizontally the ACRTC counts memory cycles from HSYNC's rising edge: its display starts
// HDS cycles after it (manual 5.8.3). The monitor's picture starts a fixed number of cycles
// after the same edge -- the mode's back porch -- so frame x of the ACRTC's memory cycle m
// is (HDS + m - hbp) cycles' worth of pixels. In interleaved mode a memory cycle is half a
// display cycle, so every register is in doubled units and a cycle is worth 8 pixels.
// Vertically: VDS rasters after VSYNC's rising edge against the mode's vertical back porch.
// ---------------------------------------------------------------------------
int CadzillaBoard::programmedWidth() const {
    return acrtc_.hdw() * kPixelsPerFetch / glueAccessMode();
}
int CadzillaBoard::programmedHeight() const {
    const uint16_t dcr = acrtc_.dcr();
    int h = acrtc_.reg(0x8A) & 0x0FFF;
    if (dcr & 0x2000) h += acrtc_.reg(0x8C) & 0x0FFF;
    if (dcr & 0x0800) h += acrtc_.reg(0x8E) & 0x0FFF;
    return h;
}
int CadzillaBoard::programmedX() const {
    const int acm = glueAccessMode();
    return (acrtc_.hds() - currentMode().hbp * acm) * (kPixelsPerFetch / acm);
}
int CadzillaBoard::programmedY() const { return acrtc_.vds() - currentMode().vbp; }

std::string CadzillaBoard::wiring() const {
    std::string s;
    if (acrtc_.bitsPerPixel() != 8)
        s += "CCR GBM is " + std::to_string(acrtc_.bitsPerPixel()) + " bpp, the board is wired for 8; ";
    if (acrtc_.gaiWords() != kWordsPerFetch)
        s += "OMR GAI is +" + std::to_string(acrtc_.gaiWords()) + " words, the board fetches 8; ";
    if ((acrtc_.omr() & 0x0C) == 0x0C) {
        s += "OMR ACM is superimposed, the board wires single and interleaved only; ";
    } else {
        // The chip's own OMR ACM bit and the board's MODE.AMODE strap must be programmed in
        // agreement -- neither can see the other's setting, so a driver has to set both.
        const bool acrtcInterleaved = (acrtc_.omr() & 0x08) != 0;
        const bool modeInterleaved  = (modeReg_ & kModeAmode) != 0;
        if (acrtcInterleaved != modeInterleaved)
            s += "OMR ACM is " + std::string(acrtcInterleaved ? "interleaved" : "single") +
                 ", MODE AMODE says " + std::string(modeInterleaved ? "interleaved" : "single") + "; ";
    }
    if (s.empty()) return "ok";
    s.erase(s.size() - 2);
    return s;
}

// ---------------------------------------------------------------------------
// The host turn: same three gates as the Dazzler. The chips say whether anything moved
// (a drawing command, a register, a palette entry); the host says whether it wants a
// frame yet; only then is the frame built.
// ---------------------------------------------------------------------------
void CadzillaBoard::pump() {
    if (!g_display) return;
    // Both consumed every time, so neither chip's flag is lost behind the other's.
    bool a = acrtc_.takeDirty();
    bool d = dac_.takeDirty();
    dirty_ = dirty_ || a || d;
    if (!dirty_) return;
    if (!g_display->wantsFrame()) return;
    render();
    dirty_ = false;
}

// THE MONITOR IS ALWAYS THERE. A fixed-frequency display with no signal shows a black frame,
// so the window opens on the first pump after power -- like the Dazzler's and the VDM-1's --
// at the mode's size, and the ACRTC's picture appears in it once a program starts the chip.
void CadzillaBoard::render() {
    const bool  on = acrtc_.displayOn();
    const Mode& m  = currentMode();
    // `this` keys this board's own window (issue #234); id titles it; videoWidth_ sizes it.
    Surface* s = g_display->acquire(this, id, m.width, m.height, PixelFormat::Indexed8, videoWidth_);
    if (!s) return;

    // THE RAMDAC: its 256-entry table is the palette, verbatim.
    g_display->setPalette(this, dac_.palette());

    s->clear(0);                              // blanking is black
    if (on) paintFrame(s, m.width, m.height);
    g_display->present(this, s);
}

// The shift register, run over the ACRTC's addresses for every raster of the frame.
void CadzillaBoard::paintFrame(Surface* s, int w, int h) {
    const Mode& m    = currentMode();
    const int   acm  = glueAccessMode();          // the board's OWN glue, from MODE AMODE
    const int   ppmc = kPixelsPerFetch / acm;     // pixels one memory cycle is worth
    const int   gai  = acrtc_.gaiWords();         // the ACRTC's own step per display cycle
    const int   hbp  = m.hbp * acm;               // the porch, in this mode's memory cycles
    auto        px   = s->pixels();

    // One run of display cycles: `cycles` memory cycles starting at frame x `x0`, fetching
    // 8 words at `addr` (advancing by GAI) on every `acm`-th memory cycle.
    auto run = [&](uint8_t* dst, int x0, int cycles, uint32_t addr) {
        for (int mc = 0; mc < cycles; mc += acm) {
            int x = x0 + mc * ppmc;
            for (int i = 0; i < kWordsPerFetch; ++i) {
                uint16_t word = acrtc_.peekWord(addr + (uint32_t)i);
                int      xa   = x + 2 * i;
                if (xa >= 0 && xa < w) dst[xa] = (uint8_t)word;            // low byte first
                if (xa + 1 >= 0 && xa + 1 < w) dst[xa + 1] = (uint8_t)(word >> 8);
            }
            addr += (uint32_t)gai;
        }
    };

    const int vbp = m.vbp;
    for (int y = 0; y < h; ++y) {
        uint8_t*  dst   = px.data() + (size_t)y * (size_t)s->pitch();
        const int vsync = y + vbp;                // this raster, counted from VSYNC's rise
        uint32_t  addr  = 0;
        const int r     = vsync - acrtc_.vds();   // which background raster, if any
        if (acrtc_.backgroundRaster(r, addr))
            run(dst, (acrtc_.hds() - hbp) * ppmc, acrtc_.hdw(), addr);
        if (acrtc_.windowRaster(vsync, addr))
            run(dst, (acrtc_.hws() - hbp) * ppmc, acrtc_.hww(), addr);
    }
}

// ---------------------------------------------------------------------------
// Reflection.
// ---------------------------------------------------------------------------
std::vector<Property> CadzillaBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "I/O base -- one 8-port block: BASE the ACRTC address/status, BASE+1 the "
                  "MODE register, BASE+2 the ACRTC data/FIFO port, BASE+4..+7 the Bt453. A "
                  "multiple of 8; default 70";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xF8;
        x.get   = [this] { return Value::ofInt(port_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 7) {
                err = "the board occupies one 8-port block -- BASE must be a multiple of 8";
                return false;
            }
            port_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "mode";
        x.help    = "The monitor: a fixed-frequency VESA raster the ACRTC's picture is placed in "
                    "by its HDS/VDS. 640x400, 640x480 (default), 800x600 or 1024x768";
        x.kind    = Kind::Enum;
        for (int i = 0; i < modeCount(); ++i) x.choices.push_back(mode(i).name);
        x.get     = [this] { return Value::ofStr(currentMode().name); };
        x.set     = [this](const Value& v, std::string& err) {
            for (int i = 0; i < modeCount(); ++i) {
                if (v.s() == mode(i).name) {
                    mode_  = i;
                    dirty_ = true;
                    return true;
                }
            }
            err = "mode is 640x400, 640x480, 800x600 or 1024x768";
            return false;
        };
        p.push_back(std::move(x));
    }
    p.push_back(Display::widthProperty(videoWidth_));
    // The interrupt strap (SW1-8: On connects IRQ* to the named line, Off -- the default,
    // `none` -- disconnects it). The same ten-choice vocabulary every interrupting board
    // uses; docs/devguide/adding-a-board.md.
    p.push_back(irqJumperProperty(
        "interrupt",
        "SW1-8: where the ACRTC's IRQ* lands -- none (default, disconnected) or the S-100 "
        "line (int = pin 73, or vi0..vi7) to raise while an enabled status flag is pending",
        irq_));

    // ---- LIVE STATUS (read-only: no setter, so SHOW says so and CONFIG SAVE skips it) ----
    {
        Property x;
        x.name = "video";
        x.help = "LIVE: whether the ACRTC is displaying -- OMR STR and DCR SE1 both set. Read-only";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr(acrtc_.displayOn() ? "on" : "off"); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "picture";
        x.help = "LIVE: the picture the ACRTC is programmed to show -- its size in pixels "
                 "(HDW memory cycles by the enabled split-screen rasters) and where its top-left "
                 "corner lands in the monitor's frame, from HDS/VDS against the mode's back "
                 "porch. Read-only";
        x.kind = Kind::Str;
        x.get  = [this] {
            return Value::ofStr(std::to_string(programmedWidth()) + "x" +
                                std::to_string(programmedHeight()) + " at (" +
                                std::to_string(programmedX()) + "," +
                                std::to_string(programmedY()) + ")");
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "wiring";
        x.help = "LIVE: whether the ACRTC is programmed the way the board is wired -- CCR GBM = "
                 "8 bpp, OMR GAI = +8 words, and OMR ACM agreeing with MODE AMODE. 'ok', or "
                 "what is off (the picture is then scrambled, as on the hardware). Read-only";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr(wiring()); };
        p.push_back(std::move(x));
    }
    // ---- MODE register (BASE+1), write-only on the wire: these are the board's own shadow
    // of what the guest last wrote there, reported the same way the Dazzler decodes its
    // write-only control/format bytes into readable status. ----
    {
        Property x;
        x.name = "hspol";
        x.help = "LIVE: MODE register HSPOL -- horizontal sync polarity the board was told to "
                 "use. Recorded, not modeled: nothing here generates a sync pulse to invert. "
                 "Read-only";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr((modeReg_ & kModeHspol) ? "negative" : "positive"); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "vspol";
        x.help = "LIVE: MODE register VSPOL -- vertical sync polarity the board was told to "
                 "use. Recorded, not modeled, like hspol. Read-only";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr((modeReg_ & kModeVspol) ? "negative" : "positive"); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "amode";
        x.help = "LIVE: MODE register AMODE -- the access mode the board's OWN fetch logic runs "
                 "(not the ACRTC's OMR ACM bit, which must agree with it -- see wiring). Read-only";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr((modeReg_ & kModeAmode) ? "interleaved" : "single"); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "olen";
        x.help = "LIVE: MODE register OLEN -- overlay enable. TBD: not wired to anything yet, "
                 "so setting it changes nothing today. Read-only";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool((modeReg_ & kModeOlen) != 0); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "status";
        x.help  = "LIVE: the ACRTC status register -- CER ARD CED LPD RFF RFR WFR WFE. Read-only";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.get   = [this] { return Value::ofInt(acrtc_.status()); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "irq";
        x.help = "LIVE: whether IRQ* is asserted right now -- an enabled status flag pending "
                 "AND the interrupt strap not 'none'. Read-only";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(irq_ != IrqJumper::None && acrtc_.irq()); };
        p.push_back(std::move(x));
    }
    return p;
}

std::vector<MapEntry> CadzillaBoard::ioMap() const {
    return {
        {(uint32_t)port_, (uint32_t)port_, "read/write",
         "ACRTC RS=0 -- status (CER ARD CED LPD RFF RFR WFR WFE) / address register"},
        {(uint32_t)(port_ + 1), (uint32_t)(port_ + 1), "write",
         "MODE -- board glue: HSPOL(0) VSPOL(1) AMODE(2) OLEN(3)"},
        {(uint32_t)(port_ + 2), (uint32_t)(port_ + 2), "read/write",
         "ACRTC RS=1 -- the register the address names, or the command FIFOs at AR=0"},
        {(uint32_t)(port_ + 4), (uint32_t)(port_ + 7), "read/write",
         "Bt453 RAMDAC -- address, palette RAM (R,G,B), address, overlay"},
    };
}

} // namespace altair
