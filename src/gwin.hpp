#ifndef GWIN_HPP
#define GWIN_HPP

#include <SDL.h>

int win_openwindow(unsigned xsize, unsigned ysize, char *appname, char *icon, int enableAntiAlias);
void win_closewindow(void);
void win_messagebox(char *string);
void win_checkmessages(void);
int win_getspeed(int framerate);
void win_setmousemode(int mode);
void win_enableKeyRepeat(void);
void win_disableKeyRepeat(void);
void win_native_modal_begin(void);
void win_native_modal_end(void);

void mou_init(void);
void mou_uninit(void);
void mou_getpos(unsigned *x, unsigned *y);
void mou_getmove(int *dx, int *dy);
unsigned mou_getbuttons(void);

extern float xmouseScale;
extern float ymouseScale;

extern int win_mousewheel;
extern char *dropFileDir;
extern int win_windowinitted;
extern int win_quitted;
extern int win_fullscreen;
extern unsigned char win_keytable[SDL_NUM_SCANCODES];
extern unsigned char win_keystate[SDL_NUM_SCANCODES];
extern unsigned char win_asciikey;
extern unsigned win_virtualkey;
extern unsigned win_mousexpos;
extern unsigned win_mouseypos;
extern unsigned win_mousexrel;
extern unsigned win_mouseyrel;
extern unsigned win_mousebuttons;
extern int win_mousemode;
extern SDL_Joystick *joy[16];
extern Sint16 joyx[16];
extern Sint16 joyy[16];
extern Uint32 joybuttons[16];
extern SDL_Window *win_window;

// Overlay hooks (formerly bme_*).
extern void (*gp_event_hook)(void *sdl_event);
extern int (*gp_input_capture_hook)(void);

#endif
