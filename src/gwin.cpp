#include "gplatform.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

SDL_Joystick *joy[MAX_JOYSTICKS] = { nullptr };
Sint16 joyx[MAX_JOYSTICKS];
Sint16 joyy[MAX_JOYSTICKS];
Uint32 joybuttons[MAX_JOYSTICKS];
SDL_Window *win_window = nullptr;

char *dropFileDir = nullptr;

int win_mousewheel = 0;
int win_fullscreen = 0;
int win_windowinitted = 0;
int win_quitted = 0;
unsigned char win_keytable[SDL_NUM_SCANCODES] = { 0 };
unsigned char win_asciikey = 0;
unsigned win_virtualkey = 0;
unsigned win_mousexpos = 0;
unsigned win_mouseypos = 0;
unsigned win_mousexrel = 0;
unsigned win_mouseyrel = 0;
unsigned win_mousebuttons = 0;

int win_mousemode = MOUSE_FULLSCREEN_HIDDEN;
unsigned char win_keystate[SDL_NUM_SCANCODES] = { 0 };

static int win_lasttime = 0;
static int win_currenttime = 0;
static int win_framecounter = 0;

float xmouseScale = 1.0f;
float ymouseScale = 1.0f;

static int keyRepeat = 0;

void (*gp_event_hook)(void *sdl_event) = nullptr;
int (*gp_input_capture_hook)(void) = nullptr;

int win_openwindow(unsigned xsize, unsigned ysize, char *appname, char * /*icon*/, int enableAntiAlias)
{
	Uint32 flags = win_fullscreen ? (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN)
	                              : (SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

	xmouseScale = 1;
	ymouseScale = 1;

	if (!win_windowinitted)
	{
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
			return BME_ERROR;
		atexit(SDL_Quit);
		win_windowinitted = 1;
	}

	if (enableAntiAlias)
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	win_window = SDL_CreateWindow(appname,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		(int)xsize, (int)ysize,
		flags);
	return BME_OK;
}

void win_enableKeyRepeat(void)
{
	keyRepeat = 1;
}

void win_disableKeyRepeat(void)
{
	keyRepeat = 0;
}

void win_closewindow(void)
{
	SDL_DestroyWindow(win_window);
	win_window = nullptr;
}

void win_messagebox(char * /*string*/)
{
}

int win_getspeed(int framerate)
{
	int frametime = 10000 / framerate;
	int frames = 0;

	while (!frames)
	{
		win_checkmessages();

		win_lasttime = win_currenttime;
		win_currenttime = (int)SDL_GetTicks();

		win_framecounter += (win_currenttime - win_lasttime) * 10;
		frames = win_framecounter / frametime;
		win_framecounter -= frames * frametime;

		if (!frames)
			SDL_Delay((Uint32)((frametime - win_framecounter) / 10));
	}

	return frames;
}

void win_checkmessages(void)
{
	SDL_Event event;
	unsigned keynum;

	SDL_PumpEvents();

	while (SDL_PollEvent(&event))
	{
		if (gp_event_hook)
			gp_event_hook(&event);

		switch (event.type)
		{
		case SDL_DROPFILE:
			dropFileDir = event.drop.file;
			break;

		case SDL_MOUSEWHEEL:
			win_mousewheel = event.wheel.y;
			break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED)
				gfx_redraw = 1;
			break;

		case SDL_JOYBUTTONDOWN:
			joybuttons[event.jbutton.which] |= 1u << event.jbutton.button;
			break;

		case SDL_JOYBUTTONUP:
			joybuttons[event.jbutton.which] &= ~(1u << event.jbutton.button);
			break;

		case SDL_JOYAXISMOTION:
			switch (event.jaxis.axis)
			{
			case 0:
				joyx[event.jaxis.which] = event.jaxis.value;
				break;
			case 1:
				joyy[event.jaxis.which] = event.jaxis.value;
				break;
			}
			break;

		case SDL_MOUSEMOTION:
			win_mousexpos = (unsigned)event.motion.x;
			win_mouseypos = (unsigned)event.motion.y;
			win_mousexrel += (unsigned)event.motion.xrel;
			win_mouseyrel += (unsigned)event.motion.yrel;
			break;

		case SDL_MOUSEBUTTONDOWN:
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				win_mousebuttons |= MOUSEB_LEFT;
				break;
			case SDL_BUTTON_MIDDLE:
				win_mousebuttons |= MOUSEB_MIDDLE;
				break;
			case SDL_BUTTON_RIGHT:
				win_mousebuttons |= MOUSEB_RIGHT;
				break;
			}
			break;

		case SDL_MOUSEBUTTONUP:
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				win_mousebuttons &= ~MOUSEB_LEFT;
				break;
			case SDL_BUTTON_MIDDLE:
				win_mousebuttons &= ~MOUSEB_MIDDLE;
				break;
			case SDL_BUTTON_RIGHT:
				win_mousebuttons &= ~MOUSEB_RIGHT;
				break;
			}
			break;

		case SDL_QUIT:
			win_quitted = 1;
			break;

		case SDL_TEXTINPUT:
			win_asciikey = (unsigned char)event.text.text[0];
			break;

		case SDL_KEYDOWN:
			if (!(event.key.repeat && !keyRepeat))
			{
				win_virtualkey = (unsigned)event.key.keysym.sym;
				keynum = event.key.keysym.scancode;
				if (keynum < SDL_NUM_SCANCODES)
				{
					win_keytable[keynum] = 1;
					win_keystate[keynum] = 1;
					if ((keynum == SDL_SCANCODE_RETURN) &&
					    (win_keystate[SDL_SCANCODE_LALT] || win_keystate[SDL_SCANCODE_RALT]))
					{
						win_fullscreen ^= 1;
						SDL_SetWindowFullscreen(win_window,
							win_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
					}
				}
			}
			break;

		case SDL_KEYUP:
			keynum = event.key.keysym.scancode;
			if (keynum < SDL_NUM_SCANCODES)
			{
				win_keytable[keynum] = 0;
				win_keystate[keynum] = 0;
			}
			break;
		}
	}
}

void win_setmousemode(int mode)
{
	win_mousemode = mode;

	switch (mode)
	{
	case MOUSE_ALWAYS_VISIBLE:
		SDL_ShowCursor(SDL_ENABLE);
		break;

	case MOUSE_FULLSCREEN_HIDDEN:
		if (gfx_fullscreen)
			SDL_ShowCursor(SDL_DISABLE);
		else
			SDL_ShowCursor(SDL_ENABLE);
		break;

	case MOUSE_ALWAYS_HIDDEN:
		SDL_ShowCursor(SDL_DISABLE);
		break;
	}
}

static float win_modal_saved_opacity = 1.0f;

static int native_modal_event_filter(void * /*userdata*/, SDL_Event *event)
{
	if (event->type == SDL_QUIT)
		return 1;
	return 0;
}

static void win_clear_transient_input(void)
{
	win_mousebuttons = 0;
	win_mousexrel = 0;
	win_mouseyrel = 0;
	win_mousewheel = 0;
	memset(win_keytable, 0, sizeof win_keytable);
	win_asciikey = 0;
	win_virtualkey = 0;
}

void win_native_modal_begin(void)
{
	SDL_SetEventFilter(native_modal_event_filter, nullptr);
	if (win_window)
	{
		float opacity = 1.0f;
		if (SDL_GetWindowOpacity(win_window, &opacity) == 0)
			win_modal_saved_opacity = opacity;
		SDL_SetWindowOpacity(win_window, 0.35f);
	}
}

void win_native_modal_end(void)
{
	SDL_SetEventFilter(nullptr, nullptr);
	if (win_window)
		SDL_SetWindowOpacity(win_window, win_modal_saved_opacity);

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			win_quitted = 1;
	}
	win_clear_transient_input();
}

void mou_init(void)
{
	win_mousebuttons = 0;
}

void mou_uninit(void)
{
}

void mou_getpos(unsigned *x, unsigned *y)
{
	if (!gfx_initted || !gfx_renderer || !win_window)
	{
		*x = win_mousexpos;
		*y = win_mouseypos;
		return;
	}

	int lw = (int)gfx_virtualxsize;
	int lh = (int)gfx_virtualysize;
	if (lw <= 0) lw = 1;
	if (lh <= 0) lh = 1;

	float scale = 1.0f, offx = 0.0f, offy = 0.0f;
	gfx_get_view(&scale, &offx, &offy);

	int outW = 0, outH = 0, winW = 0, winH = 0;
	SDL_GetRendererOutputSize(gfx_renderer, &outW, &outH);
	SDL_GetWindowSize(win_window, &winW, &winH);
	float px = (winW > 0) ? (float)win_mousexpos * (float)outW / (float)winW : (float)win_mousexpos;
	float py = (winH > 0) ? (float)win_mouseypos * (float)outH / (float)winH : (float)win_mouseypos;

	float lx = (px - offx) / scale;
	float ly = (py - offy) / scale;

	if (lx < 0.0f) lx = 0.0f;
	if (ly < 0.0f) ly = 0.0f;
	if (lx > (float)(lw - 1)) lx = (float)(lw - 1);
	if (ly > (float)(lh - 1)) ly = (float)(lh - 1);

	*x = (unsigned)lx;
	*y = (unsigned)ly;
}

void mou_getmove(int *dx, int *dy)
{
	*dx = (int)win_mousexrel;
	*dy = (int)win_mouseyrel;
	win_mousexrel = 0;
	win_mouseyrel = 0;
}

unsigned mou_getbuttons(void)
{
	return win_mousebuttons;
}
