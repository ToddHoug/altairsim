#pragma once
//
// Audio -- the host sound service a board plays samples through (DESIGN.md 7.4; the
// output analogue of host/joystick.h, and the sound half of host/display.h).
//
// THE BOARD NEVER CALLS SDL. A board that makes sound -- a Cromemco D+7A driving a JS-1
// speaker from a D/A latch -- turns what the guest did into 16-bit PCM and hands it to
// this interface. Where the samples go -- a sound device, or nowhere -- is the host's
// business. So the board compiles and RUNS with no sound library at all: against a
// NullAudio (audio_null.h) every push is discarded, which is a speaker that is not
// plugged in, and a headless test reads the samples back from a stub.
//
// PUSHED FROM Board::pump(), NEVER FROM INSIDE A BUS CYCLE. read()/write() are pure
// computation over state; a board records what the guest wrote, with the emulated time
// it wrote it, and renders it to samples in its host turn. The service is called on the
// main thread only. A backend may drain its queue on a thread of its own, and that
// thread never sees the Machine.
//
// EMULATED TIME IS NOT REAL TIME, AND THIS SEAM DOES NOT PRETEND OTHERWISE. A device
// plays rate() samples each real second. A board renders rate() samples for each
// EMULATED second. The two agree only when the machine runs at a crystal (Clock::free()
// is false); flat out, a board must push nothing. queued() is how a board sees that it
// has run ahead of the device anyway, and holds back.

#include <cstddef>
#include <cstdint>
#include <span>

namespace altair {

class Audio {
public:
    // Which voice the samples belong to -- a stable address the caller owns (one speaker,
    // one voice). The service mixes the voices; a caller never mixes for another.
    using Owner = const void*;

    virtual ~Audio() = default;

    // Samples per second the service takes: mono, signed 16-bit. Constant for the life of
    // the service, and valid before any device is open, so a board can render with it.
    virtual int rate() const = 0;

    // Is there a sound device behind this service? False headless, and false once the
    // host's device has failed to open. A backend that opens its device on the first
    // push() says true until then. For SHOW; push() is safe either way.
    virtual bool available() const = 0;

    // Append samples to `owner`'s voice. They play after whatever is already queued.
    virtual void push(Owner owner, std::span<const int16_t> samples) = 0;

    // Samples of `owner`'s voice pushed and not yet played. Zero means the voice has run
    // dry (or never started), and the next push starts with a gap unless it brings a
    // cushion with it.
    virtual size_t queued(Owner owner) const = 0;
};

} // namespace altair
