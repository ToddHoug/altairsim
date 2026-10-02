#include "host/audio_sdl.h"

#include <SDL3/SDL.h>

namespace altair {

SdlAudio::~SdlAudio() {
    // Destroying a stream unbinds it, so the device has nothing to play by the time it
    // closes.
    for (auto& [owner, st] : streams_)
        if (st) SDL_DestroyAudioStream(st);
    if (dev_) SDL_CloseAudioDevice(dev_);
    // Quit ONLY our subsystem, never the whole library -- the SdlDisplay owns video and
    // may still be alive (host/audio_sdl.h).
    if (inited_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool SdlAudio::open() {
    if (dev_) return true;
    if (failed_) return false;

    // Just the audio subsystem. This works with no prior SDL_Init and no window, so a
    // speaker plays in a machine with no graphics board. If it fails, stay quiet -- no
    // sound, not a crashed simulator.
    if (!inited_) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            failed_ = true;
            return false;
        }
        inited_ = true;
    }

    // The default device in the format it prefers; each stream converts to it. A device
    // opened this way starts playing at once, and follows the host's default if the user
    // changes it.
    dev_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!dev_) {
        failed_ = true;
        return false;
    }
    return true;
}

SDL_AudioStream* SdlAudio::stream(Owner owner) {
    if (auto it = streams_.find(owner); it != streams_.end()) return it->second;

    SDL_AudioSpec spec{};
    spec.format   = SDL_AUDIO_S16;  // native byte order, as the int16_t samples are
    spec.channels = 1;
    spec.freq     = kRate;

    // The output side is set by the bind, to the device's own format.
    SDL_AudioStream* st = SDL_CreateAudioStream(&spec, nullptr);
    if (st && !SDL_BindAudioStream(dev_, st)) {
        SDL_DestroyAudioStream(st);
        st = nullptr;
    }
    // A null is remembered too: a voice SDL refused is not asked for again every slice.
    streams_[owner] = st;
    return st;
}

void SdlAudio::push(Owner owner, std::span<const int16_t> samples) {
    if (samples.empty() || !open()) return;
    SDL_AudioStream* st = stream(owner);
    if (!st) return;
    SDL_PutAudioStreamData(st, samples.data(), (int)(samples.size() * sizeof(int16_t)));
}

size_t SdlAudio::queued(Owner owner) const {
    auto it = streams_.find(owner);
    if (it == streams_.end() || !it->second) return 0;
    // Bytes of what was put in and is not yet taken by the device; -1 on an error.
    const int bytes = SDL_GetAudioStreamQueued(it->second);
    return bytes > 0 ? (size_t)bytes / sizeof(int16_t) : 0;
}

} // namespace altair
