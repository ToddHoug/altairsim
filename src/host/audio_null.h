#pragma once
//
// NullAudio -- an Audio with no device behind it (the sound analogue of NullJoystick /
// NullDisplay).
//
// This is what a board is given when there is no sound backend: in a headless build (no
// SDL, ALTAIRSIM_ENABLE_SDL off), and in EVERY test that does not install its own stub.
// A board wired to one of these renders and pushes exactly as it would to a real device,
// and the samples go nowhere -- a speaker that is not plugged in.

#include "host/audio.h"

namespace altair {

class NullAudio : public Audio {
public:
    int    rate() const override { return 44100; }
    bool   available() const override { return false; }
    void   push(Owner, std::span<const int16_t>) override {}
    size_t queued(Owner) const override { return 0; }
};

} // namespace altair
