#pragma once

#include <SDL.h>

bool win_openwindow(unsigned xsize, unsigned ysize, const char* appname, int enable_anti_alias);
// Editor bootstrap: SDL init, window, icon, renderer. scale is 1..3 (legacy "bigwindow").
bool win_init_editor(unsigned scale, int enable_anti_alias);
int  win_getspeed(int framerate);
void win_setmousemode(int mode);
void win_reapply_mousemode();
void win_enableKeyRepeat();
void win_disableKeyRepeat();
void win_native_modal_begin();
void win_native_modal_end();

unsigned mou_getbuttons();

extern int           win_mousewheel;
extern char*         dropFileDir;
extern int           win_quitted;
extern int           win_fullscreen;
extern unsigned char win_keytable[SDL_NUM_SCANCODES];
extern unsigned char win_keystate[SDL_NUM_SCANCODES];
extern unsigned char win_asciikey;
extern SDL_Window*   win_window;
