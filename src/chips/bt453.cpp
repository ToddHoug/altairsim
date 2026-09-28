#include "chips/bt453.h"

#include "core/statefile.h"

namespace altair {

Bt453::Bt453() { reset(); }

void Bt453::reset() {
    for (auto& e : ram_) e[0] = e[1] = e[2] = 0;
    for (auto& e : ovl_) e[0] = e[1] = e[2] = 0;
    addr_  = 0;
    phase_ = 0;
    pending_[0] = pending_[1] = pending_[2] = 0;
    dirty_ = true;
}

// ---------------------------------------------------------------------------
// The MPU's four locations (Table 1).
// ---------------------------------------------------------------------------
uint8_t Bt453::read(uint8_t c) {
    switch (c & 3) {
    case 0:
    case 2:
        // The address register. A READ does not touch the phase (Table 2 note) -- a
        // driver may look mid-sequence without losing its place.
        return addr_;
    case 1:
        return cycleRead(false);
    default:
        return cycleRead(true);
    }
}

void Bt453::write(uint8_t c, uint8_t v) {
    switch (c & 3) {
    case 0:
    case 2:
        // The address register. A WRITE resets the phase to red.
        addr_  = v;
        phase_ = 0;
        return;
    case 1:
        cycleWrite(false, v);
        return;
    default:
        cycleWrite(true, v);
        return;
    }
}

// ---------------------------------------------------------------------------
// One byte cycle of a three-cycle color. The overlay registers are addressed by
// ADDR1..0 only ("the 6 most significant bits of the address register are ignored").
// ---------------------------------------------------------------------------
uint8_t Bt453::cycleRead(bool overlay) {
    const uint8_t* e = overlay ? ovl_[addr_ & 3] : ram_[addr_];
    uint8_t        v = e[phase_];
    advance(overlay);
    return v;
}

void Bt453::cycleWrite(bool overlay, uint8_t v) {
    pending_[phase_] = v;
    if (phase_ == 2) {
        // The blue cycle: the three bytes are concatenated into one 24-bit word and
        // written to the addressed location NOW. Red and green alone changed nothing.
        uint8_t* e = overlay ? ovl_[addr_ & 3] : ram_[addr_];
        e[0] = pending_[0];
        e[1] = pending_[1];
        e[2] = pending_[2];
        // The reserved overlay slot is never displayed, so it does not dirty the picture.
        if (!overlay || (addr_ & 3) != 0) dirty_ = true;
    }
    advance(overlay);
}

void Bt453::advance(bool /*overlay*/) {
    if (++phase_ < 3) return;
    phase_ = 0;
    // "Following the blue cycle, the address register increments to the next location"
    // -- and "$FF" wraps to "$00". The overlay case increments the same 8-bit register;
    // only the two low bits matter to it.
    addr_ = (uint8_t)(addr_ + 1);
}

// ---------------------------------------------------------------------------
// The video side (Table 3).
// ---------------------------------------------------------------------------
Color Bt453::lookup(uint8_t p, uint8_t ol) const {
    const uint8_t* e = (ol & 3) ? ovl_[ol & 3] : ram_[p];
    return Color{e[0], e[1], e[2], 0xFF};
}

std::array<Color, 256> Bt453::palette() const {
    std::array<Color, 256> out;
    for (int i = 0; i < 256; ++i) out[(size_t)i] = Color{ram_[i][0], ram_[i][1], ram_[i][2], 0xFF};
    return out;
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
void Bt453::serialize(StateWriter& w) const {
    w.raw(&ram_[0][0], sizeof ram_);
    w.raw(&ovl_[0][0], sizeof ovl_);
    w.u8(addr_);
    w.u8(phase_);
    w.raw(pending_, sizeof pending_);
}

void Bt453::deserialize(StateReader& r) {
    r.raw(&ram_[0][0], sizeof ram_);
    r.raw(&ovl_[0][0], sizeof ovl_);
    addr_  = r.u8();
    phase_ = r.u8();
    if (phase_ > 2) phase_ = 0;
    r.raw(pending_, sizeof pending_);
    dirty_ = true;  // the restored table owes the host a palette
}

} // namespace altair
