// GTUltra platform layer (SDL2 window / renderer / audio / input).
#pragma once

#include <SDL.h>
#include <cstdint>

#define MOUSE_ALWAYS_VISIBLE 0
#define MOUSE_FULLSCREEN_HIDDEN 1
#define MOUSE_ALWAYS_HIDDEN 2

#define MOUSEB_LEFT 1
#define MOUSEB_RIGHT 2
#define MOUSEB_MIDDLE 4

#define STEREO 1
#define SIXTEENBIT 2

// --- Audio (SDL custom-mixer SID path, used by gsound) -------------------
bool snd_init(unsigned mixrate, unsigned mixmode, unsigned bufferlength_ms);
void snd_set_custom_mixer(void (*custom_mixer)(Sint32* dest, unsigned samples));

extern void (*snd_player)();
extern int      snd_bpmtempo;
extern unsigned snd_mixrate;

// --- Graphics (SDL renderer + ImGui present) -----------------------------
bool gfx_init();
void gfx_present();

extern SDL_Renderer* gfx_renderer;

// --- Window / input ------------------------------------------------------
bool win_openwindow(unsigned xsize, unsigned ysize, const char* appname, int enable_anti_alias);
// Editor bootstrap: SDL init, window, icon, renderer. scale is 1..3 (legacy "bigwindow").
bool win_init_editor(unsigned scale, int enable_anti_alias);
int  win_getspeed(int framerate);
void win_setmousemode(int mode);
void win_reapply_mousemode();
void win_enable_key_repeat();
void win_disable_key_repeat();
void win_native_modal_begin();
void win_native_modal_end();

unsigned mou_getbuttons();

extern int         win_mousewheel;
extern char*       dropFileDir;
extern int         win_quitted;
extern int         win_fullscreen;
extern uint8_t     win_keytable[SDL_NUM_SCANCODES];
extern uint8_t     win_keystate[SDL_NUM_SCANCODES];
extern uint8_t     win_asciikey;
extern SDL_Window* win_window;
