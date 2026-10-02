#pragma once
//
// SdlAudio -- the SDL3-backed Audio (host/audio.h). Compiled ONLY when SDL3 is found
// (ALTAIRSIM_ENABLE_SDL); the headless build uses NullAudio instead.
//
// THE ONLY AUDIO FILE THAT INCLUDES SDL. A board renders PCM and pushes it through the
// Audio seam and knows nothing of this; the composition root (src/main.cpp) creates one
// of these and injects it.
//
// ONE DEVICE, ONE STREAM FOR EACH VOICE. The host's default playback device is opened
// once. Each Owner gets an SDL_AudioStream of its own (mono, signed 16-bit, rate()),
// bound to that device. SDL mixes the bound streams and converts them to whatever the
// device takes, so two speakers sound together and neither board mixes for the other.
//
// SELF-CONTAINED AND LAZY. Nothing is touched until the first push(): a machine that
// makes no sound never brings up SDL_INIT_AUDIO and never opens a device. The destructor
// quits only that subsystem -- never the global SDL_Quit(), which is the SdlDisplay's
// (host/joystick_sdl.h has the same rule).
//
// A HOST WITH NO SOUND DEVICE IS NOT AN ERROR. If the subsystem or the device will not
// open, push() discards from then on and available() turns false. It is tried once, not
// once a slice. Until that first try available() is TRUE: nothing has failed, and asking
// a host for its devices only to answer a SHOW would open the sound system for a guest
// that may never make a sound.
//
// MAIN-THREAD CALLS, AND SDL'S THREAD ONLY DRAINS. push() and queued() run from a
// board's pump() on the main thread. SDL plays the streams from a thread of its own;
// that thread reads the stream's queue and nothing else. It never sees the Machine.

#include "host/audio.h"

#include <cstdint>
#include <unordered_map>

struct SDL_AudioStream;

namespace altair {

class SdlAudio : public Audio {
public:
    SdlAudio() = default;
    ~SdlAudio() override;

    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    int    rate() const override { return kRate; }
    bool   available() const override { return !failed_; }
    void   push(Owner owner, std::span<const int16_t> samples) override;
    size_t queued(Owner owner) const override;

private:
    static constexpr int kRate = 44100;

    // Bring up the subsystem and the device, once. False when there is no device.
    bool open();

    // The stream for `owner`, made and bound on its first push. Null if SDL refuses.
    SDL_AudioStream* stream(Owner owner);

    bool     inited_ = false;  // SDL_INIT_AUDIO is ours to quit
    bool     failed_ = false;  // the one try failed; stay quiet
    uint32_t dev_    = 0;      // the SDL_AudioDeviceID, 0 = not open

    std::unordered_map<Owner, SDL_AudioStream*> streams_;
};

} // namespace altair
