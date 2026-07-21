// GTUltra platform layer: SDL2 window, renderer, audio mixer and input.
// Consolidates the former gwin / ggfx / gaudio modules into one TU.
#include "gplatform.hpp"

#include "embed.hpp"
#include "gimgui.hpp"

#include <cstdlib>
#include <cstring>

// --- Public state --------------------------------------------------------

// Audio
void (*snd_player)()  = nullptr;
int      snd_bpmtempo = 125;
unsigned snd_mixrate  = 0;

// Graphics
SDL_Renderer* gfx_renderer = nullptr;

// Window / input
SDL_Window* win_window = nullptr;

char* dropFileDir = nullptr;

int     win_mousewheel                  = 0;
int     win_fullscreen                  = 0;
int     win_quitted                     = 0;
uint8_t win_keytable[SDL_NUM_SCANCODES] = { 0 };
uint8_t win_asciikey                    = 0;
uint8_t win_keystate[SDL_NUM_SCANCODES] = { 0 };

namespace {

// --- Audio internals -----------------------------------------------------

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

// --- Window / input internals --------------------------------------------

constexpr unsigned k_window_w[] = { 1280, 1600, 1920 };
constexpr unsigned k_window_h[] = { 800, 1000, 1200 };

int      window_initted = 0;
unsigned mouse_buttons  = 0;
int      mouse_mode     = MOUSE_ALWAYS_VISIBLE;
int      last_time      = 0;
int      current_time   = 0;
int      frame_counter  = 0;
int      key_repeat     = 0;
float    modal_opacity  = 1.0f;

void load_window_icon() {
    const auto icon = embed::get(embed::Id::window_icon);

    // 128x128 RGBA8888, decoded from assets/icon.ico into assets/icon128.rgba.
    constexpr int size = 128;
    if (icon.size < static_cast<size_t>(size) * size * 4) return;

    // SDL_SetWindowIcon copies the pixels, so the wrapper can be freed
    // afterwards and the const embed data is never written.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(icon.data), size, size, 32, size * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface) {
        SDL_SetWindowIcon(win_window, surface);
        SDL_FreeSurface(surface);
    }
}

void pump_events() {
    SDL_Event event;
    SDL_PumpEvents();

    while (SDL_PollEvent(&event)) {
        gimgui_process_event(&event);

        switch (event.type) {
        case SDL_DROPFILE: dropFileDir = event.drop.file; break;
        case SDL_MOUSEWHEEL: win_mousewheel = event.wheel.y; break;
        case SDL_MOUSEBUTTONDOWN:
            switch (event.button.button) {
            case SDL_BUTTON_LEFT: mouse_buttons |= MOUSEB_LEFT; break;
            case SDL_BUTTON_MIDDLE: mouse_buttons |= MOUSEB_MIDDLE; break;
            case SDL_BUTTON_RIGHT: mouse_buttons |= MOUSEB_RIGHT; break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            switch (event.button.button) {
            case SDL_BUTTON_LEFT: mouse_buttons &= ~MOUSEB_LEFT; break;
            case SDL_BUTTON_MIDDLE: mouse_buttons &= ~MOUSEB_MIDDLE; break;
            case SDL_BUTTON_RIGHT: mouse_buttons &= ~MOUSEB_RIGHT; break;
            }
            break;
        case SDL_QUIT: win_quitted = 1; break;
        case SDL_TEXTINPUT: win_asciikey = static_cast<uint8_t>(event.text.text[0]); break;
        case SDL_KEYDOWN:
            if (!event.key.repeat || key_repeat) {
                unsigned keynum = event.key.keysym.scancode;
                if (keynum < SDL_NUM_SCANCODES) {
                    win_keytable[keynum] = 1;
                    win_keystate[keynum] = 1;
                    if ((keynum == SDL_SCANCODE_RETURN) &&
                        (win_keystate[SDL_SCANCODE_LALT] || win_keystate[SDL_SCANCODE_RALT])) {
                        win_fullscreen ^= 1;
                        SDL_SetWindowFullscreen(win_window, win_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    }
                }
            }
            break;
        case SDL_KEYUP: {
            unsigned keynum = event.key.keysym.scancode;
            if (keynum < SDL_NUM_SCANCODES) {
                win_keytable[keynum] = 0;
                win_keystate[keynum] = 0;
            }
            break;
        }
        }
    }
}

int native_modal_event_filter(void*, SDL_Event* event) { return event->type == SDL_QUIT ? 1 : 0; }

void clear_transient_input() {
    mouse_buttons  = 0;
    win_mousewheel = 0;
    memset(win_keytable, 0, sizeof win_keytable);
    win_asciikey = 0;
}

void win_reapply_mousemode() {
    switch (mouse_mode) {
    case MOUSE_ALWAYS_VISIBLE: SDL_ShowCursor(SDL_ENABLE); break;
    case MOUSE_FULLSCREEN_HIDDEN: SDL_ShowCursor(win_fullscreen ? SDL_DISABLE : SDL_ENABLE); break;
    case MOUSE_ALWAYS_HIDDEN: SDL_ShowCursor(SDL_DISABLE); break;
    }
}

void win_setmousemode(int mode) {
    mouse_mode = mode;
    win_reapply_mousemode();
}

bool win_openwindow(unsigned xsize, unsigned ysize, const char* appname, int enable_anti_alias) {
    Uint32 flags = win_fullscreen ? (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN)
                                  : (SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    if (!window_initted) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return false;
        atexit(SDL_Quit);
        window_initted = 1;
    }

    if (enable_anti_alias) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    win_window =
        SDL_CreateWindow(appname, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)xsize, (int)ysize, flags);
    return win_window != nullptr;
}

bool gfx_init() {
    if (gfx_renderer) return true;

    gfx_renderer = SDL_CreateRenderer(win_window, -1, SDL_RENDERER_ACCELERATED);
    if (!gfx_renderer) return false;

    win_reapply_mousemode();
    return true;
}

} // namespace

// --- Audio ---------------------------------------------------------------

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

// --- Graphics ------------------------------------------------------------

void gfx_present() {
    if (!gfx_renderer) return;
    SDL_SetRenderDrawColor(gfx_renderer, 0, 0, 0, 255);
    SDL_RenderClear(gfx_renderer);
    gimgui_render();
    SDL_RenderPresent(gfx_renderer);
}

// --- Window / input ------------------------------------------------------

bool win_init_editor(unsigned scale, int enable_anti_alias) {
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;

    if (!win_openwindow(k_window_w[scale - 1], k_window_h[scale - 1], "GTUltra", enable_anti_alias)) return false;

    win_setmousemode(MOUSE_ALWAYS_VISIBLE);
    load_window_icon();

    if (!gfx_init()) {
        win_fullscreen = 0;
        return gfx_init();
    }
    return true;
}

void win_enable_key_repeat() { key_repeat = 1; }
void win_disable_key_repeat() { key_repeat = 0; }

int win_getspeed(int framerate) {
    int frametime = 10000 / framerate;
    int frames    = 0;
    while (!frames) {
        pump_events();
        last_time    = current_time;
        current_time = (int)SDL_GetTicks();
        frame_counter += (current_time - last_time) * 10;
        frames = frame_counter / frametime;
        frame_counter -= frames * frametime;
        if (!frames) SDL_Delay((Uint32)((frametime - frame_counter) / 10));
    }
    return frames;
}

void win_native_modal_begin() {
    SDL_SetEventFilter(native_modal_event_filter, nullptr);
    if (win_window) {
        float opacity = 1.0f;
        if (SDL_GetWindowOpacity(win_window, &opacity) == 0) modal_opacity = opacity;
        SDL_SetWindowOpacity(win_window, 0.35f);
    }
}

void win_native_modal_end() {
    SDL_SetEventFilter(nullptr, nullptr);
    if (win_window) SDL_SetWindowOpacity(win_window, modal_opacity);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) win_quitted = 1;
    }
    clear_transient_input();
}

unsigned mou_getbuttons() { return mouse_buttons; }
