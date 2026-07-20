/*
 * GTUltra console / input glue (M6 Phase 4).
 * Chargen cell buffers and rasterize removed; window + getkey remain.
 */

#define GCONSOLE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goattrk2.hpp"

int gfxinitted = 0;
unsigned char* chardata = NULL; // retained symbol; unused after chargen removal

int key = 0;
int rawkey = 0;
int shiftpressed = 0;
int shiftOrCtrlPressed = 0;
int ctrlpressed = 0;
int bothShiftAndCtrlPressed = 0;
int cursorflashdelay = 0;
int mouseb = 0;
int prevmouseb = 0;
unsigned mousex = 0;
unsigned mousey = 0;
unsigned mousepixelx = 0;
unsigned mousepixely = 0;
unsigned oldmousepixelx = 0xffffffff;
unsigned oldmousepixely = 0xffffffff;
int mouseheld = 0;
int fontwidth = 8;
int fontheight = 14;
unsigned bigwindow = 1;

int mouseTicks = 0;
int mouseTicksDelta = 0;
int mousebDoubleClick = 0;
int keyDownCount = 0;

static void initicon(void);

int initscreen(void)
{
	if (bigwindow - 1) {
		fontwidth *= (int)bigwindow;
		fontheight *= (int)bigwindow;
	}

	const unsigned xsize = MAX_COLUMNS * (unsigned)fontwidth;
	const unsigned ysize = MAX_ROWS * (unsigned)fontheight;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
		return 0;
	win_openwindow(xsize, ysize, "GoatTracker Ultra - (Enhanced GoatTracker Stereo V2.76 - Jason Page / MSL)", NULL, enableAntiAlias);
	// OS cursor for ImGui (chargen software cursor removed).
	win_setmousemode(MOUSE_ALWAYS_VISIBLE);
	initicon();

	if (!gfx_init(xsize, ysize, 60, 0)) {
		win_fullscreen = 0;
		if (!gfx_init(xsize, ysize, 60, 0))
			return 0;
	}

	// Keep gfx palette tables alive for setSkin() boot colours (ImGui uses guicolors).
	// No INDEX8 surface after M6 Phase 6 — gfx_setpalette is a no-op.

	gfxinitted = 1;
	atexit(closescreen);
	return 1;
}

static void initicon(void)
{
	int handle = io_open("goat32.png");
	if (handle == -1)
		return;

	SDL_RWops* rw;
	SDL_Surface* icon;
	char* iconbuffer;
	int size;

	size = io_lseek(handle, 0, SEEK_END);
	io_lseek(handle, 0, SEEK_SET);
	iconbuffer = (char*)malloc((size_t)size);
	if (!iconbuffer) {
		io_close(handle);
		return;
	}
	io_read(handle, iconbuffer, size);
	io_close(handle);
	rw = SDL_RWFromMem(iconbuffer, size);
	icon = SDL_LoadBMP_RW(rw, 0);
	if (icon)
		SDL_SetWindowIcon(win_window, icon);
	free(iconbuffer);
}

void closescreen(void)
{
	gfxinitted = 0;
}

void clearscreen(int backColor)
{
	(void)backColor;
}

void fliptoscreen(void)
{
	// Leftover callers (reloc interactive / waitkey): present ImGui frame only.
	gfx_present();
}

void printtext(int x, int y, int color, const char* text)
{
	(void)x;
	(void)y;
	(void)color;
	(void)text;
}

void printtextc(int y, int color, const char* text)
{
	(void)y;
	(void)color;
	(void)text;
}

void printtextcp(int cp, int y, int color, const char* text)
{
	(void)cp;
	(void)y;
	(void)color;
	(void)text;
}

void printblank(int x, int y, int length)
{
	(void)x;
	(void)y;
	(void)length;
}

void printblankc(int x, int y, int color, int length)
{
	(void)x;
	(void)y;
	(void)color;
	(void)length;
}

void drawbox(int x, int y, int color, int sx, int sy)
{
	(void)x;
	(void)y;
	(void)color;
	(void)sx;
	(void)sy;
}

void printbg(int x, int y, int color, int length)
{
	(void)x;
	(void)y;
	(void)color;
	(void)length;
}

void printbyte(int x, int y, int color, unsigned int b)
{
	(void)x;
	(void)y;
	(void)color;
	(void)b;
}

void printbyterow(int x, int y, int color, unsigned int b, int length)
{
	(void)x;
	(void)y;
	(void)color;
	(void)b;
	(void)length;
}

void printbytecol(int x, int y, int color, unsigned int b, int length)
{
	(void)x;
	(void)y;
	(void)color;
	(void)b;
	(void)length;
}

void fillArea(int x, int y, int width, int height, int color, int fillchar)
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	(void)color;
	(void)fillchar;
}

int getColor(int fcolor, int bcolor)
{
	return fcolor | (bcolor << 8);
}

void forceRedraw(void) {}

void modifyChars(void) {}

void getkey(void)
{
	int c;
	win_asciikey = 0;
	cursorflashdelay += win_getspeed(50);

	prevmouseb = mouseb;

	mou_getpos(&mousepixelx, &mousepixely);

	mouseb = mou_getbuttons();
	mousex = mousepixelx / (unsigned)fontwidth;
	mousey = mousepixely / (unsigned)fontheight;

	if (mouseb) {
		mouseheld++;
		if (!prevmouseb) {
			mouseTicksDelta = (int)SDL_GetTicks() - mouseTicks;
			mouseTicks = (int)SDL_GetTicks();
			mousebDoubleClick = (mouseTicksDelta < 250) ? 1 : 0;
		}
	} else {
		mouseheld = 0;
	}

	keyDownCount = 0;
	key = win_asciikey;
	rawkey = 0;
	for (c = 0; c < SDL_NUM_SCANCODES; c++) {
		if (win_keystate[c])
			keyDownCount++;
		if (win_keytable[c]) {
			if ((c != SDL_SCANCODE_LSHIFT) && (c != SDL_SCANCODE_RSHIFT) &&
			    (c != SDL_SCANCODE_LCTRL) && (c != SDL_SCANCODE_RCTRL)) {
				rawkey = c;
				win_keytable[c] = 0;
				break;
			}
		}
	}

	shiftOrCtrlPressed = 0;
	ctrlpressed = 0;
	shiftpressed = 0;
	bothShiftAndCtrlPressed = 0;

	if (win_keystate[SDL_SCANCODE_LCTRL] || win_keystate[SDL_SCANCODE_RCTRL])
		ctrlpressed = 1;

	if ((win_keystate[SDL_SCANCODE_LSHIFT]) || (win_keystate[SDL_SCANCODE_RSHIFT]))
		shiftpressed = 1;

	shiftOrCtrlPressed = shiftpressed | ctrlpressed;
	bothShiftAndCtrlPressed = shiftpressed * ctrlpressed;

	if (rawkey == SDL_SCANCODE_KP_ENTER) {
		key = KEY_ENTER;
		rawkey = SDL_SCANCODE_RETURN;
	}

	if (rawkey == SDL_SCANCODE_KP_0) key = '0';
	if (rawkey == SDL_SCANCODE_KP_1) key = '1';
	if (rawkey == SDL_SCANCODE_KP_2) key = '2';
	if (rawkey == SDL_SCANCODE_KP_3) key = '3';
	if (rawkey == SDL_SCANCODE_KP_4) key = '4';
	if (rawkey == SDL_SCANCODE_KP_5) key = '5';
	if (rawkey == SDL_SCANCODE_KP_6) key = '6';
	if (rawkey == SDL_SCANCODE_KP_7) key = '7';
	if (rawkey == SDL_SCANCODE_KP_8) key = '8';
	if (rawkey == SDL_SCANCODE_KP_9) key = '9';

	if (gp_input_capture_hook) {
		int cap = gp_input_capture_hook();
		if (cap & 1) {
			mouseb = 0;
			prevmouseb = 0;
			mousebDoubleClick = 0;
			mouseheld = 0;
			win_mousewheel = 0;
		}
		if (cap & 2) {
			key = 0;
			rawkey = 0;
			win_asciikey = 0;
		}
	}
}
