#include "gimgui.hpp"
#include "embed.hpp"
#include "gplatform.hpp"

#include <cstring>

SDL_Window* win_window = nullptr;

char* dropFileDir = nullptr;

int     win_mousewheel                  = 0;
int     win_fullscreen                  = 0;
int     win_quitted                     = 0;
uint8_t win_keytable[SDL_NUM_SCANCODES] = { 0 };
uint8_t win_asciikey                    = 0;
uint8_t win_keystate[SDL_NUM_SCANCODES] = { 0 };

namespace {

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

    // goat32.png is a BMP payload (historical name).
    SDL_RWops*   rw      = SDL_RWFromConstMem(icon.data, static_cast<int>(icon.size));
    SDL_Surface* surface = SDL_LoadBMP_RW(rw, 1);
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

} // namespace

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
