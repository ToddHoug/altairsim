#include "boards/cromemco-d7a.h"

#include "core/clock.h"
#include "core/statefile.h"
#include "host/audio.h"
#include "host/joystick.h"

namespace altair {
namespace {

// The injected host game-controller service (setJoystick), borrowed. Null on the bench;
// a NullJoystick headless -- pump() then reads every stick as centered with no buttons,
// which is a D+7A with no JS-1 plugged in.
Joystick* g_joystick = nullptr;

// The injected host sound service (setAudio), borrowed. Null on the bench; a NullAudio
// headless, which takes the samples and plays nothing.
Audio* g_audio = nullptr;

// Is `s` a well-formed non-negative decimal index? (The joystick straps accept a number
// or a keyword; this tells the two apart, at set-time and again when resolving.)
bool parseIndex(const std::string& s, int& out) {
    if (s.empty()) return false;
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
        if (v > 32767) return false;  // no host has this many controllers; keep it sane
    }
    out = v;
    return true;
}

// A `joystick1`/`joystick2` strap is a keyword or a device index.
bool validJoySpec(const std::string& lowered) {
    if (lowered == "none" || lowered == "auto" || lowered == "keyboard" ||
        lowered == "kbd")
        return true;
    int idx = 0;
    return parseIndex(lowered, idx);
}

// A `speaker1`/`speaker2` strap is 'none' or one analog channel, 1..7. Gives the channel
// (0 for none); false when it is neither.
bool parseSpeaker(const std::string& lowered, int& ch) {
    if (lowered == "none") {
        ch = 0;
        return true;
    }
    if (lowered.size() == 1 && lowered[0] >= '1' && lowered[0] <= '7') {
        ch = lowered[0] - '0';
        return true;
    }
    return false;
}

} // namespace

void D7aBoard::setJoystick(Joystick* j) { g_joystick = j; }
void D7aBoard::setAudio(Audio* a) { g_audio = a; }

// ---------------------------------------------------------------------------
// Bus: eight consecutive I/O ports, no memory.
// ---------------------------------------------------------------------------
bool D7aBoard::decodes(const BusCycle& c) const {
    if (!enabled_) return false;
    if (c.type != Cycle::IoRead && c.type != Cycle::IoWrite) return false;
    uint8_t p = c.port();
    return p >= base_ && p < (uint8_t)(base_ + 8);
}

uint8_t D7aBoard::read(const BusCycle& c) {
    int off = c.port() - base_;
    if (off == 0) return parIn_;      // parallel input byte (JS-1 buttons)
    return analogIn_[off - 1];        // analog channel A/D
}

void D7aBoard::write(const BusCycle& c) {
    int off = c.port() - base_;
    if (off == 0) {
        parOut_ = c.data;             // parallel output latch
        return;
    }
    // Analog channel D/A latch. A JS-1 speaker port is written here in a timed loop, so
    // a CHANGE on a speaker's channel is recorded with the time it happened. That is all
    // a bus cycle does; pump() makes the sound.
    if (c.data != analogOut_[off - 1] && clock_) {
        for (Speaker* sp : {&spk1_, &spk2_}) {
            if (sp->ch != off) continue;
            sp->pcm.edge(clock_->now(), (int8_t)c.data);
            sp->heard      = true;
            sp->lastChange = clock_->now();
        }
    }
    analogOut_[off - 1] = c.data;
}

// ---------------------------------------------------------------------------
// Lifecycle.
// ---------------------------------------------------------------------------
void D7aBoard::reset(Reset) {
    // Clear the D/A outputs to 0 V and the parallel output latch; the A/D shadows are
    // re-read from the host on the next pump(). Straps and stick assignments stay.
    // A speaker hears its output go to 0 V, at the moment of the reset.
    if (clock_)
        for (Speaker* sp : {&spk1_, &spk2_})
            if (sp->ch) sp->pcm.edge(clock_->now(), 0);
    for (auto& v : analogOut_) v = 0;
    parOut_ = 0;
}

void D7aBoard::power() {
    for (auto& v : analogIn_) v = 0;
    for (auto& v : analogOut_) v = 0;
    parIn_  = 0xFF;   // active-low buttons idle high (released); refreshed each pump()
    parOut_ = 0;
    // The clock starts again at zero, so the sound not yet rendered belongs to no time.
    for (Speaker* sp : {&spk1_, &spk2_}) {
        sp->pcm.clear();
        sp->heard      = false;
        sp->lastChange = 0;
    }
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
void D7aBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    for (uint8_t v : analogIn_) w.u8(v);
    for (uint8_t v : analogOut_) w.u8(v);
    w.u8(parIn_);
    w.u8(parOut_);
}

void D7aBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    for (uint8_t& v : analogIn_) v = r.u8();
    for (uint8_t& v : analogOut_) v = r.u8();
    parIn_  = r.u8();
    parOut_ = r.u8();
    // The clock and the latches are another moment's now.
    resync(spk1_);
    resync(spk2_);
}

// ---------------------------------------------------------------------------
// The host turn: play the speakers, then read the joysticks and fold them into the A/D
// and parallel-input latches. Once per slice, on the main thread -- never inside a bus
// cycle.
// ---------------------------------------------------------------------------
void D7aBoard::pump() {
    pumpSpeaker(spk1_);
    pumpSpeaker(spk2_);

    if (!g_joystick) return;
    g_joystick->poll();
    // Console 1: X/Y -> analog channels 1/2 (0x19/0x1A), buttons -> parallel bits D0-D3;
    // `auto` prefers gamepad 0.
    applyConsole(js1_, 0, 1, 0, 0);
    // Console 2: X/Y -> analog channels 3/4 (0x1B/0x1C), buttons -> parallel bits D4-D7;
    // `auto` prefers gamepad 1, so two `auto` consoles drive two different sticks.
    applyConsole(js2_, 2, 3, 4, 1);
}

// ---------------------------------------------------------------------------
// The speaker. One slice of emulated time becomes the same length of sound, or nothing.
//
// A device plays rate() samples each REAL second and this renders rate() samples each
// EMULATED second, so the two must run at the same speed for the sound to be right.
// Each rule below is one way they do not:
//
//   flat out        no crystal: emulated time has no fixed relation to real time, and
//                   the slice is dropped. Nothing is played, at any pitch.
//   not driven      a level that does not move is silence. Nothing is pushed, so a guest
//                   that never touches the speaker never opens a sound device.
//   queue empty     the device has caught up (or has not started). A short cushion goes
//                   in first, or the device would run dry between this slice and the
//                   next and every slice would start with a gap.
//   queue deep      the machine is ahead of the device -- a run that nothing paces, with
//                   a crystal set. The slice is dropped, so the delay cannot grow.
// ---------------------------------------------------------------------------
bool D7aBoard::driven(const Speaker& sp) const {
    if (!clock_ || !sp.heard) return false;
    const uint64_t now = clock_->now();
    return now >= sp.lastChange && now - sp.lastChange <= (uint64_t)clock_->hz() / 2;
}

void D7aBoard::resync(Speaker& sp) {
    sp.pcm.clear();
    if (sp.ch) sp.pcm.edge(0, (int8_t)analogOut_[sp.ch - 1]);
    sp.pcm.skipTo(clock_ ? clock_->now() : 0);
    sp.heard      = false;
    sp.lastChange = 0;
}

void D7aBoard::pumpSpeaker(Speaker& sp) {
    if (!clock_) return;
    const uint64_t now = clock_->now();
    const uint64_t hz  = (uint64_t)clock_->hz();

    // More than a second since the last render is not a slice: the clock was moved under
    // the board. Never render it -- it could be hours of samples.
    const uint64_t from  = sp.pcm.renderedTo();
    const bool     moved = now < from || now - from > hz;

    if (!g_audio || sp.ch == 0 || clock_->free() || moved || !driven(sp)) {
        sp.pcm.skipTo(now);
        return;
    }
    // No time has gone by: the machine is stopped at the prompt, and a stopped machine
    // makes no sound. (Without this, an empty queue would get a cushion at every call.)
    if (now == from) return;

    const size_t rate   = (size_t)g_audio->rate();
    const size_t queued = g_audio->queued(&sp);
    if (queued > rate / 4) {  // a quarter second waiting to play
        sp.pcm.skipTo(now);
        return;
    }

    pcmBuf_.clear();
    if (queued == 0)          // 50 ms at the level the slice starts from
        pcmBuf_.assign(rate / 20, (int16_t)(sp.pcm.held() * LevelPcm::kGain));
    sp.pcm.render(now, (long long)hz, (int)rate, pcmBuf_);
    if (!pcmBuf_.empty()) g_audio->push(&sp, pcmBuf_);
}

uint8_t D7aBoard::axis8(int16_t a) {
    // Arithmetic >>8 (well-defined in C++20): center 0 -> 0x00, +full -> +127 (0x7F),
    // -full -> -127 (0x81). The negative end stops one short of the A/D's 0x80 so the range
    // is the same each way and the byte can be negated (see applyConsole).
    int v = (int)a >> 8;
    if (v < -127) v = -127;
    return (uint8_t)(int8_t)v;
}

StickState D7aBoard::resolveStick(const std::string& spec, int autoIndex) const {
    if (!g_joystick) return {};
    std::string s = lowerAscii(spec);
    if (s == "none") return {};
    if (s == "keyboard" || s == "kbd") return g_joystick->keyboardStick();
    if (s == "auto")
        return g_joystick->count() > autoIndex ? g_joystick->stick(autoIndex)
                                               : g_joystick->keyboardStick();
    int idx = 0;
    if (parseIndex(s, idx)) return g_joystick->stick(idx);
    return {};
}

void D7aBoard::applyConsole(const std::string& spec, int xCh, int yCh,
                            int buttonShift, int autoIndex) {
    StickState s = resolveStick(spec, autoIndex);  // absent -> centered, no buttons
    // A full deflection is the A/D's full scale, 0x81..0x7F (reference/JS-1.md 4.1). GOTCHA
    // adds the complement of the rest reading (0x00 -> 0xFF) to each reading and takes a
    // direction only for a size of 0x40 or more, so a stick that stops at +63 cannot steer
    // it. 0x80 is kept out: 0x80 + 0xFF is 0x7F, a full move the OPPOSITE way. Dazzle-Doodle
    // draws only for readings in 0xC0..0x3F and calls the rest "voltage out of range", so it
    // stops drawing past half deflection.
    analogIn_[xCh] = axis8(s.x);
    // The period Dazzler games read SDL +Y (stick DOWN) as up, so invert Y: stick up -> a
    // positive byte, stick down -> negative. Shift to the byte FIRST (so a small rest drift
    // near center still floors to 0x00), THEN negate the signed byte -- negating the raw axis
    // first would floor a resting +drift to 0xFF and slide center off zero.
    analogIn_[yCh] = (uint8_t)(int8_t)(-(int)(int8_t)axis8(s.y));
    // ACTIVE-LOW buttons: a bit reads 1 when the button is RELEASED and 0 when PRESSED
    // (the JS-1's pulled-up switches). So an idle/absent console reads its nibble all-1s,
    // and a press pulls its bit to 0. Sourced from David Hansel's Arduino Altair 8800
    // simulator firmware, which inverts exactly this way to drive the period Dazzler
    // games (reference/JS-1.md 3); StickState.buttons has bit = 1 for pressed.
    uint8_t nib  = (uint8_t)(~s.buttons & 0x0F);
    uint8_t mask = (uint8_t)(0x0F << buttonShift);
    parIn_ = (uint8_t)((parIn_ & ~mask) | (nib << buttonShift));
}

// ---------------------------------------------------------------------------
// Reflection.
// ---------------------------------------------------------------------------
std::vector<Property> D7aBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "Base of the 8-port block (A7..A3 jumpers): parallel at BASE, analog at "
                  "BASE+1..7. A multiple of 8; default 18";
        x.kind  = Kind::Int;
        x.radix = 16;  // ON THE WIRE -> HEX (DESIGN.md 10.0.1)
        x.min   = 0;
        x.max   = 0xF8;  // BASE+7 must still be a port
        x.get   = [this] { return Value::ofInt(base_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 7) {
                err = "the D+7A block is 8 ports selected by A7..A3 -- the base must be a "
                      "multiple of 8";
                return false;
            }
            base_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    auto joyProp = [](const char* name, const char* console, std::string& slot) {
        Property x;
        x.name = name;
        x.help = std::string("Which host controller drives JS-1 console ") + console +
                 ": 'none', 'auto' (the matching gamepad -- console 1->pad 0, console "
                 "2->pad 1 -- or the keyboard), 'keyboard', or a device index like 0";
        x.kind = Kind::Str;
        x.get  = [&slot] { return Value::ofStr(slot); };
        x.set  = [&slot](const Value& v, std::string& err) {
            std::string s = lowerAscii(v.s());
            if (!validJoySpec(s)) {
                err = "expected 'none', 'auto', 'keyboard', or a device index (0, 1, ...)";
                return false;
            }
            slot = s;
            return true;
        };
        return x;
    };
    p.push_back(joyProp("joystick1", "1", js1_));
    p.push_back(joyProp("joystick2", "2", js2_));

    auto spkProp = [this](const char* name, const char* console, const char* dflt,
                          Speaker& sp) {
        Property x;
        x.name = name;
        x.help = std::string("The analog output that JS-1 console ") + console +
                 "'s speaker is on: 'none', or a channel 1 to 7 (channel n is port "
                 "BASE+n). Default " + dflt;
        x.kind = Kind::Str;
        x.get  = [&sp] { return Value::ofStr(sp.ch ? std::to_string(sp.ch) : "none"); };
        x.set  = [this, &sp](const Value& v, std::string& err) {
            int ch = 0;
            if (!parseSpeaker(lowerAscii(v.s()), ch)) {
                err = "expected 'none' or an analog channel, 1 to 7";
                return false;
            }
            sp.ch = ch;
            resync(sp);
            return true;
        };
        return x;
    };
    p.push_back(spkProp("speaker1", "1", "1", spk1_));
    p.push_back(spkProp("speaker2", "2", "3", spk2_));
    return p;
}

// The live picture for SHOW <id>: what each console's strap actually resolves to right
// now -- a named gamepad, the keyboard, or nothing -- which the property table (the strap
// STRING) cannot show; and for each speaker, whether it plays and, if not, the reason. This is why "joystick1 = auto" is not the same question as "is a
// controller connected"; here we answer the second. Reads the host directly, so it polls
// first (count()/name() are cached by poll()); safe because SHOW runs on the main thread
// at a stopped prompt, the same place the display's idle hook pumps SDL.
std::vector<std::string> D7aBoard::statusLines() const {
    if (g_joystick) g_joystick->poll();

    // "-> ..." for one console's strap, given the gamepad `auto` prefers for it.
    auto resolve = [&](const std::string& spec, int autoIndex) -> std::string {
        if (!g_joystick) return "-> (no joystick service in this build)";
        std::string s = lowerAscii(spec);
        // Keyed on count() alone, exactly as resolveStick() decides what to READ, so the
        // reported source can never disagree with the source actually feeding the A/D.
        auto gamepad = [&](int i) -> std::string {
            if (g_joystick->count() > i) {
                std::string nm = g_joystick->name(i);
                return "-> gamepad " + std::to_string(i) + (nm.empty() ? "" : "  \"" + nm + "\"");
            }
            return "-> gamepad " + std::to_string(i) + "  (not present)";
        };
        if (s == "none") return "-> unwired";
        if (s == "keyboard" || s == "kbd")
            return g_joystick->keyboardStick().present ? "-> keyboard"
                                                       : "-> keyboard  (unavailable)";
        if (s == "auto") {
            if (g_joystick->count() > autoIndex) return gamepad(autoIndex);
            return g_joystick->keyboardStick().present
                       ? "-> keyboard  (no gamepad " + std::to_string(autoIndex) + ")"
                       : "-> nothing  (no gamepad " + std::to_string(autoIndex) +
                             ", no keyboard focus)";
        }
        int idx = 0;
        if (parseIndex(s, idx)) return gamepad(idx);
        return "-> (unrecognized)";
    };

    auto line = [&](const char* console, const std::string& strap, int autoIndex) {
        std::string s = "console " + std::string(console) + "  (" + strap + ")";
        while (s.size() < 24) s += ' ';
        return s + resolve(strap, autoIndex);
    };

    // The speakers: is the guest driving one, and if nothing plays, the reason.
    auto speaker = [&](const char* console, const Speaker& sp) {
        std::string s = "speaker " + std::string(console) + "  (" +
                        (sp.ch ? "channel " + std::to_string(sp.ch) : std::string("none")) + ")";
        while (s.size() < 24) s += ' ';
        if (sp.ch == 0) return s + "-> unwired";
        if (!g_audio) return s + "-> (no audio service in this build)";
        if (!g_audio->available()) return s + "-> no sound device";
        if (clock_ && clock_->free())
            return s + "-> silent: the machine has no crystal (clock_hz = 0)";
        return s + (driven(sp) ? "-> playing" : "-> silent");
    };

    return {line("1", js1_, 0), line("2", js2_, 1), speaker("1", spk1_), speaker("2", spk2_)};
}

std::vector<MapEntry> D7aBoard::ioMap() const {
    return {
        {(uint32_t)base_, (uint32_t)base_, "read/write",
         "D+7A -- parallel: input byte (JS-1 buttons) / output latch"},
        {(uint32_t)(base_ + 1), (uint32_t)(base_ + 7), "read/write",
         "D+7A -- 7 analog channels: A/D input / D/A output, two's-complement"},
    };
}

} // namespace altair
