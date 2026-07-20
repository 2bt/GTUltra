#include "gplatform.hpp"

#include <cstdlib>
#include <cstring>

void (*snd_player)()  = nullptr;
int      snd_bpmtempo = 125;
unsigned snd_mixrate  = 0;

namespace {

void (*snd_custommixer)(Sint32* dest, unsigned samples) = nullptr;
unsigned      snd_buffersize                            = 0;
unsigned      snd_samplesize                            = 0;
unsigned      snd_mixmode                               = 0;
int           snd_bpmcount                              = 0;
bool          snd_sndinitted                            = false;
bool          snd_atexit_registered                     = false;
Sint32*       snd_clipbuffer                            = nullptr;
SDL_AudioSpec desired;
SDL_AudioSpec obtained;

void snd_uninit();
bool snd_initmixer();
void snd_uninitmixer();
void snd_mixdata(Uint8* dest, unsigned bytes);
void snd_mixer(void* userdata, Uint8* stream, int len);
void snd_clearclipbuffer(Sint32* clipbuffer, unsigned clipsamples);
void snd_16bit_postprocess(Sint32* src, Sint16* dest, unsigned samples);
void snd_8bit_postprocess(Sint32* src, Uint8* dest, unsigned samples);

void snd_uninit() {
    if (snd_sndinitted) {
        SDL_CloseAudio();
        snd_sndinitted = false;
    }
    snd_uninitmixer();
}

bool snd_initmixer() {
    snd_uninitmixer();

    size_t n = snd_buffersize / snd_samplesize;
    if (snd_mixmode & STEREO) n *= 2;
    snd_clipbuffer = (Sint32*)malloc(n * sizeof(Sint32));
    return snd_clipbuffer != nullptr;
}

void snd_uninitmixer() {
    free(snd_clipbuffer);
    snd_clipbuffer = nullptr;
}

void snd_mixer(void* /*userdata*/, Uint8* stream, int len) { snd_mixdata(stream, (unsigned)len); }

void snd_mixdata(Uint8* dest, unsigned bytes) {
    unsigned mixsamples  = bytes;
    unsigned clipsamples = bytes;
    Sint32*  clipptr     = snd_clipbuffer;
    if (snd_mixmode & STEREO) mixsamples >>= 1;
    if (snd_mixmode & SIXTEENBIT) {
        clipsamples >>= 1;
        mixsamples >>= 1;
    }
    snd_clearclipbuffer(snd_clipbuffer, clipsamples);
    if (snd_player) {
        while (mixsamples) {
            if (!snd_bpmcount && snd_player) {
                snd_player();
                snd_bpmcount = (int)(((snd_mixrate * 5) >> 1) / (unsigned)snd_bpmtempo);
            }

            unsigned musicsamples = mixsamples;
            if ((int)musicsamples > snd_bpmcount) musicsamples = (unsigned)snd_bpmcount;
            snd_bpmcount -= (int)musicsamples;
            if (snd_custommixer) snd_custommixer(clipptr, musicsamples);
            if (snd_mixmode & STEREO) clipptr += musicsamples * 2;
            else
                clipptr += musicsamples;
            mixsamples -= musicsamples;
        }
    }
    else if (snd_custommixer) {
        snd_custommixer(clipptr, mixsamples);
    }

    clipptr = snd_clipbuffer;
    if (snd_mixmode & SIXTEENBIT) snd_16bit_postprocess(clipptr, (Sint16*)dest, clipsamples);
    else
        snd_8bit_postprocess(clipptr, dest, clipsamples);
}

void snd_clearclipbuffer(Sint32* clipbuffer, unsigned clipsamples) {
    memset(clipbuffer, 0, clipsamples * sizeof(Sint32));
}

void snd_16bit_postprocess(Sint32* src, Sint16* dest, unsigned samples) {
    while (samples--) {
        int sample = *src++;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        *dest++ = (Sint16)sample;
    }
}

void snd_8bit_postprocess(Sint32* src, Uint8* dest, unsigned samples) {
    while (samples--) {
        int sample = *src++;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        *dest++ = (Uint8)((sample >> 8) + 128);
    }
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

    desired.freq   = (int)mixrate;
    desired.format = AUDIO_U8;
    if (mixmode & SIXTEENBIT) desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    if (mixmode & STEREO) desired.channels = 2;
    desired.samples = (Uint16)(bufferlength_ms * mixrate / 1000);
    {
        int bits = 0;
        for (;;) {
            desired.samples = (Uint16)(desired.samples >> 1);
            if (!desired.samples) break;
            bits++;
        }
        desired.samples = (Uint16)(1 << bits);
    }

    desired.callback = snd_mixer;
    desired.userdata = nullptr;
    snd_bpmcount     = 0;

    SDL_PauseAudio(1);

    if (SDL_OpenAudio(&desired, &obtained)) {
        snd_uninit();
        return false;
    }
    snd_sndinitted = true;

    snd_mixmode    = 0;
    snd_samplesize = 1;
    if (obtained.channels == 2) {
        snd_mixmode |= STEREO;
        snd_samplesize <<= 1;
    }
    if ((obtained.format == AUDIO_S16SYS) || (obtained.format == AUDIO_S16LSB) ||
        (obtained.format == AUDIO_S16MSB)) {
        snd_mixmode |= SIXTEENBIT;
        snd_samplesize <<= 1;
    }
    snd_buffersize = obtained.size;
    snd_mixrate    = (unsigned)obtained.freq;

    if (!snd_initmixer()) {
        snd_uninit();
        return false;
    }

    SDL_PauseAudio(0);
    return true;
}

void snd_setcustommixer(void (*custommixer)(Sint32* dest, unsigned samples)) { snd_custommixer = custommixer; }
