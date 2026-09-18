#include "boards/cadzilla.h"

#include "core/statefile.h"
#include "host/display.h"

#include <vector>

namespace altair {
namespace {

// The injected host video service (setDisplay), borrowed. Null on the bench and in a
// headless build with nothing wired -- pump() then simply does not draw.
Display* g_display = nullptr;

} // namespace

void CadzillaBoard::setDisplay(Display* d) { g_display = d; }

// 128 K words is the default `vram` strap; the member initializer below must agree with it
// (a member is not yet constructed when the base-initializer list runs).
CadzillaBoard::CadzillaBoard() : acrtc_((size_t)128 * 1024) {}

// ---------------------------------------------------------------------------
// Bus: six I/O ports, no memory.
// ---------------------------------------------------------------------------
bool CadzillaBoard::decodes(const BusCycle& c) const {
    if (!enabled_) return false;
    if (c.type != Cycle::IoRead && c.type != Cycle::IoWrite) return false;
    uint8_t p = c.port();
    if (p == port_ || p == (uint8_t)(port_ | 1)) return true;             // the ACRTC
    return (uint8_t)(p & 0xFC) == dacPort_;                                // the Bt453
}

uint8_t CadzillaBoard::read(const BusCycle& c) {
    uint8_t p = c.port();
    if (p == port_ || p == (uint8_t)(port_ | 1)) return acrtc_.read(p & 1);   // RS = A0
    return dac_.read(p & 3);                                                 // C1C0 = A1A0
}

void CadzillaBoard::write(const BusCycle& c) {
    uint8_t p = c.port();
    if (p == port_ || p == (uint8_t)(port_ | 1)) {
        acrtc_.write(p & 1, c.data);
        return;
    }
    dac_.write(p & 3, c.data);
}

// ---------------------------------------------------------------------------
// Lifecycle. RESET* reaches the ACRTC's RES* pin; the Bt453 has no reset pin, so a warm
// reset keeps the palette a driver loaded (the picture blanks because the ACRTC stopped).
// ---------------------------------------------------------------------------
void CadzillaBoard::reset(Reset r) {
    if (r == Reset::Bus) {
        acrtc_.reset();
        dirty_ = true;
    }
}

void CadzillaBoard::power() {
    acrtc_.power();
    dac_.reset();
    dirty_ = true;
    lastW_ = lastH_ = 0;
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
void CadzillaBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    acrtc_.serialize(w);
    dac_.serialize(w);
}

void CadzillaBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    acrtc_.deserialize(r);
    dac_.deserialize(r);
    dirty_ = true;  // the restored picture owes the host a full redraw
}

// ---------------------------------------------------------------------------
// The host turn: same three gates as the Dazzler. The chips say whether anything moved
// (a drawing command, a register, a palette entry); the host says whether it wants a
// frame yet; only then is the raster scanned out.
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

void CadzillaBoard::render() {
    const bool on = acrtc_.displayOn();
    int        w  = on ? acrtc_.displayWidth() : lastW_;
    int        h  = on ? acrtc_.displayHeight() : lastH_;
    if (w <= 0 || h <= 0) return;             // nothing to show and never was: no window
    if (w > 4096) w = 4096;                   // the ACRTC's own limits, and the host's sanity
    if (h > 4096) h = 4096;
    lastW_ = w;
    lastH_ = h;

    // `this` keys this board's own window (issue #234); id titles it; videoWidth_ sizes it.
    Surface* s = g_display->acquire(this, id, w, h, PixelFormat::Indexed8, videoWidth_);
    if (!s) return;

    // THE RAMDAC: its 256-entry table is the palette, verbatim.
    g_display->setPalette(this, dac_.palette());

    if (!on) {
        s->clear(0);
    } else {
        std::vector<uint16_t> row((size_t)w);
        auto                  px = s->pixels();
        for (int y = 0; y < h; ++y) {
            acrtc_.scanline(y, row);
            uint8_t* dst = px.data() + (size_t)y * (size_t)s->pitch();
            // P0-P7: the low byte of the pixel value is what the DAC sees.
            for (int x = 0; x < w; ++x) dst[x] = (uint8_t)row[(size_t)x];
        }
    }
    g_display->present(this, s);
}

// ---------------------------------------------------------------------------
// Reflection.
// ---------------------------------------------------------------------------
std::vector<Property> CadzillaBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "ACRTC I/O base -- BASE is the address/status port (RS=0), BASE+1 the data "
                  "port (RS=1). Even; default 70";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xFE;
        x.get   = [this] { return Value::ofInt(port_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 1) {
                err = "the ACRTC occupies BASE and BASE+1 -- BASE must be even";
                return false;
            }
            port_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "dac";
        x.help  = "Bt453 RAMDAC I/O base -- four ports: DAC+0 address, DAC+1 palette RAM, "
                  "DAC+2 address, DAC+3 overlay. A multiple of 4; default 74";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xFC;
        x.get   = [this] { return Value::ofInt(dacPort_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 3) {
                err = "the Bt453 occupies DAC..DAC+3 -- DAC must be a multiple of 4";
                return false;
            }
            dacPort_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "vram";
        x.help  = "Frame memory fitted, in K words of 16 bits: a power of two from 4 to 1024 "
                  "(the ACRTC addresses 1 M words). Changing it clears the picture. Default 128";
        x.kind  = Kind::Int;
        x.min   = 4;
        x.max   = 1024;
        x.get   = [this] { return Value::ofInt(vramK_); };
        x.set   = [this](const Value& v, std::string& err) {
            long long k = v.i();
            if (k < 4 || k > 1024 || (k & (k - 1)) != 0) {
                err = "vram must be a power of two from 4 to 1024 K words";
                return false;
            }
            if ((int)k != vramK_) {
                vramK_ = (int)k;
                acrtc_  = Hd63484((size_t)vramK_ * 1024);
                dirty_  = true;
            }
            return true;
        };
        p.push_back(std::move(x));
    }
    p.push_back(Display::widthProperty(videoWidth_));

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
        x.name = "resolution";
        x.help = "LIVE: the visible raster in pixels, from HDW, GAI and the split-screen "
                 "widths. Read-only";
        x.kind = Kind::Str;
        x.get  = [this] {
            return Value::ofStr(std::to_string(acrtc_.displayWidth()) + "x" +
                                std::to_string(acrtc_.displayHeight()));
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "depth";
        x.help = "LIVE: bits per pixel from CCR GBM (1, 2, 4, 8 or 16). The DAC sees eight "
                 "of them. Read-only";
        x.kind = Kind::Int;
        x.get  = [this] { return Value::ofInt(acrtc_.bitsPerPixel()); };
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
    return p;
}

std::vector<MapEntry> CadzillaBoard::ioMap() const {
    return {
        {(uint32_t)port_, (uint32_t)port_, "read/write",
         "ACRTC -- status (CER ARD CED LPD RFF RFR WFR WFE) / address register"},
        {(uint32_t)(port_ | 1), (uint32_t)(port_ | 1), "read/write",
         "ACRTC -- the register the address names, or the command FIFOs at AR=0"},
        {(uint32_t)dacPort_, (uint32_t)(dacPort_ + 3), "read/write",
         "Bt453 RAMDAC -- address, palette RAM (R,G,B), address, overlay"},
    };
}

} // namespace altair
