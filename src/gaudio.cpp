#include "gplatform.hpp"

#include <cstdlib>
#include <cstring>

void (*snd_player)()  = nullptr;
int      snd_bpmtempo = 125;
unsigned snd_mixrate  = 0;

namespace {

void (*snd_custom_mixer)(Sint32* dest, unsigned samples) = nullptr;
unsigned      snd_buffer_size                            = 0;
unsigned      snd_sample_size                            = 0;
unsigned      snd_mix_mode                               = 0;
int           snd_bpm_count                              = 0;
bool          snd_initted                                = false;
bool          snd_atexit_registered                      = false;
Sint32*       snd_clip_buffer                            = nullptr;
SDL_AudioSpec desired;
SDL_AudioSpec obtained;

void snd_clear_clip_buffer(Sint32* clip_buffer, unsigned clip_samples) {
    memset(clip_buffer, 0, clip_samples * sizeof(Sint32));
}

void snd_16bit_postprocess(Sint32* src, Sint16* dest, unsigned samples) {
    while (samples--) {
        int sample = *src++;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        *dest++ = static_cast<Sint16>(sample);
    }
}

void snd_8bit_postprocess(Sint32* src, Uint8* dest, unsigned samples) {
    while (samples--) {
        int sample = *src++;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        *dest++ = static_cast<Uint8>((sample >> 8) + 128);
    }
}

void snd_uninit_mixer() {
    free(snd_clip_buffer);
    snd_clip_buffer = nullptr;
}

bool snd_init_mixer() {
    snd_uninit_mixer();

    size_t n = snd_buffer_size / snd_sample_size;
    if (snd_mix_mode & STEREO) n *= 2;
    snd_clip_buffer = static_cast<Sint32*>(malloc(n * sizeof(Sint32)));
    return snd_clip_buffer != nullptr;
}

void snd_mix_data(Uint8* dest, unsigned bytes) {
    unsigned mix_samples  = bytes;
    unsigned clip_samples = bytes;
    Sint32*  clip_ptr     = snd_clip_buffer;
    if (snd_mix_mode & STEREO) mix_samples >>= 1;
    if (snd_mix_mode & SIXTEENBIT) {
        clip_samples >>= 1;
        mix_samples >>= 1;
    }
    snd_clear_clip_buffer(snd_clip_buffer, clip_samples);
    if (snd_player) {
        while (mix_samples) {
            if (!snd_bpm_count && snd_player) {
                snd_player();
                snd_bpm_count = static_cast<int>(((snd_mixrate * 5) >> 1) / static_cast<unsigned>(snd_bpmtempo));
            }

            unsigned music_samples = mix_samples;
            if (static_cast<int>(music_samples) > snd_bpm_count)
                music_samples = static_cast<unsigned>(snd_bpm_count);
            snd_bpm_count -= static_cast<int>(music_samples);
            if (snd_custom_mixer) snd_custom_mixer(clip_ptr, music_samples);
            if (snd_mix_mode & STEREO) clip_ptr += music_samples * 2;
            else clip_ptr += music_samples;
            mix_samples -= music_samples;
        }
    }
    else if (snd_custom_mixer) {
        snd_custom_mixer(clip_ptr, mix_samples);
    }

    clip_ptr = snd_clip_buffer;
    if (snd_mix_mode & SIXTEENBIT) snd_16bit_postprocess(clip_ptr, reinterpret_cast<Sint16*>(dest), clip_samples);
    else snd_8bit_postprocess(clip_ptr, dest, clip_samples);
}

void snd_mixer(void* /*userdata*/, Uint8* stream, int len) { snd_mix_data(stream, static_cast<unsigned>(len)); }

void snd_uninit() {
    if (snd_initted) {
        SDL_CloseAudio();
        snd_initted = false;
    }
    snd_uninit_mixer();
}

} // namespace

bool snd_init(unsigned mixrate, unsigned mixmode, unsigned bufferlength_ms) {
    if (!snd_atexit_registered) {
        atexit(snd_uninit);
        snd_atexit_registered = true;
    }

    snd_uninit();

    if (!mixrate || !bufferlength_ms) {
        snd_uninit();
        return false;
    }

    desired.freq   = static_cast<int>(mixrate);
    desired.format = AUDIO_U8;
    if (mixmode & SIXTEENBIT) desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    if (mixmode & STEREO) desired.channels = 2;
    desired.samples = static_cast<Uint16>(bufferlength_ms * mixrate / 1000);
    {
        int bits = 0;
        for (;;) {
            desired.samples = static_cast<Uint16>(desired.samples >> 1);
            if (!desired.samples) break;
            bits++;
        }
        desired.samples = static_cast<Uint16>(1 << bits);
    }

    desired.callback = snd_mixer;
    desired.userdata = nullptr;
    snd_bpm_count    = 0;

    SDL_PauseAudio(1);

    if (SDL_OpenAudio(&desired, &obtained)) {
        snd_uninit();
        return false;
    }
    snd_initted = true;

    snd_mix_mode    = 0;
    snd_sample_size = 1;
    if (obtained.channels == 2) {
        snd_mix_mode |= STEREO;
        snd_sample_size <<= 1;
    }
    if ((obtained.format == AUDIO_S16SYS) || (obtained.format == AUDIO_S16LSB) ||
        (obtained.format == AUDIO_S16MSB)) {
        snd_mix_mode |= SIXTEENBIT;
        snd_sample_size <<= 1;
    }
    snd_buffer_size = obtained.size;
    snd_mixrate     = static_cast<unsigned>(obtained.freq);

    if (!snd_init_mixer()) {
        snd_uninit();
        return false;
    }

    SDL_PauseAudio(0);
    return true;
}

void snd_set_custom_mixer(void (*custom_mixer)(Sint32* dest, unsigned samples)) {
    snd_custom_mixer = custom_mixer;
}
