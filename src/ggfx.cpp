#include "gplatform.hpp"

#include <cstdlib>

int gfx_initted = 0;
int gfx_redraw = 0;
int gfx_fullscreen = 0;
int gfx_scanlinemode = 0;
int gfx_preventswitch = 0;
unsigned gfx_virtualxsize = 0;
unsigned gfx_virtualysize = 0;
unsigned gfx_windowxsize = 0;
unsigned gfx_windowysize = 0;
SDL_Renderer *gfx_renderer = nullptr;

void (*gp_overlay_render_hook)(void) = nullptr;

static int gfx_initexec = 0;
static unsigned gfx_last_xsize = 0;
static unsigned gfx_last_ysize = 0;
static unsigned gfx_last_framerate = 0;
static unsigned gfx_last_flags = 0;
static SDL_Color gfx_sdlpalette[MAX_COLORS];

void gfx_resize(unsigned int /*xsize*/, unsigned int /*ysize*/)
{
}

int gfx_init(unsigned xsize, unsigned ysize, unsigned framerate, unsigned flags)
{
	int sdlflags = SDL_RENDERER_ACCELERATED;

	if (gfx_initexec)
		return BME_OK;
	gfx_initexec = 1;

	gfx_last_xsize = xsize;
	gfx_last_ysize = ysize;
	gfx_last_framerate = framerate;
	gfx_last_flags = flags & ~(GFX_FULLSCREEN | GFX_WINDOW);

	gfx_scanlinemode = (int)(flags & (GFX_SCANLINES | GFX_DOUBLESIZE));

	if (flags & GFX_NOSWITCHING)
		gfx_preventswitch = 1;
	else
		gfx_preventswitch = 0;
	gfx_fullscreen = win_fullscreen ? 1 : 0;

	gfx_virtualxsize = xsize;
	gfx_virtualxsize /= 16;
	gfx_virtualxsize *= 16;
	gfx_virtualysize = ysize;

	if ((!gfx_virtualxsize) || (!gfx_virtualysize))
	{
		gfx_initexec = 0;
		gfx_uninit();
		bme_error = BME_ILLEGAL_CONFIG;
		return BME_ERROR;
	}

	gfx_windowxsize = gfx_virtualxsize;
	gfx_windowysize = gfx_virtualysize;
	if (gfx_scanlinemode)
	{
		gfx_windowxsize <<= 1;
		gfx_windowysize <<= 1;
	}

	gfx_sdlpalette[0].r = 0;
	gfx_sdlpalette[0].g = 0;
	gfx_sdlpalette[0].b = 0;
	gfx_sdlpalette[0].a = 255;
	gfx_sdlpalette[255].r = 255;
	gfx_sdlpalette[255].g = 255;
	gfx_sdlpalette[255].b = 255;
	gfx_sdlpalette[255].a = 255;

	gfx_renderer = SDL_CreateRenderer(win_window, -1, sdlflags);
	gfx_initexec = 0;
	if (gfx_renderer)
	{
		gfx_initted = 1;
		gfx_redraw = 1;
		win_setmousemode(win_mousemode);
		return BME_OK;
	}
	return BME_ERROR;
}

int gfx_reinit(void)
{
	return gfx_init(gfx_last_xsize, gfx_last_ysize, gfx_last_framerate, gfx_last_flags);
}

void gfx_uninit(void)
{
	if (gfx_renderer)
	{
		SDL_DestroyRenderer(gfx_renderer);
		gfx_renderer = nullptr;
	}
	gfx_initted = 0;
}

void gfx_get_view(float *scale, float *offx, float *offy)
{
	int outW = 0, outH = 0;
	if (gfx_renderer)
		SDL_GetRendererOutputSize(gfx_renderer, &outW, &outH);
	int sw = (int)gfx_virtualxsize;
	int sh = (int)gfx_virtualysize;
	if (sw <= 0) sw = 1;
	if (sh <= 0) sh = 1;

	float sx = (float)outW / (float)sw;
	float sy = (float)outH / (float)sh;
	float s = sx < sy ? sx : sy;
	if (s <= 0.0f) s = 1.0f;

	*scale = s;
	*offx = ((float)outW - (float)sw * s) * 0.5f;
	*offy = ((float)outH - (float)sh * s) * 0.5f;
}

void gfx_present(void)
{
	if (!gfx_renderer)
		return;
	SDL_SetRenderDrawColor(gfx_renderer, 0, 0, 0, 255);
	SDL_RenderClear(gfx_renderer);
	if (gp_overlay_render_hook)
		gp_overlay_render_hook();
	SDL_RenderPresent(gfx_renderer);
	gfx_redraw = 0;
}

void gfx_setPaletteRGB(int index, int r, int g, int b)
{
	if (index < 0 || index >= MAX_COLORS)
		return;
	gfx_sdlpalette[index].r = (Uint8)r;
	gfx_sdlpalette[index].g = (Uint8)g;
	gfx_sdlpalette[index].b = (Uint8)b;
	gfx_sdlpalette[index].a = 255;
	gfx_redraw = 1;
}
