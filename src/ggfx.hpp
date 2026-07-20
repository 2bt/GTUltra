#ifndef GGFX_HPP
#define GGFX_HPP

#include <SDL.h>

void gfx_resize(unsigned int xsize, unsigned int ysize);
int gfx_init(unsigned xsize, unsigned ysize, unsigned framerate, unsigned flags);
int gfx_reinit(void);
void gfx_uninit(void);
void gfx_present(void);
void gfx_get_view(float *scale, float *offx, float *offy);
void gfx_setPaletteRGB(int index, int r, int g, int b);

extern int gfx_initted;
extern int gfx_scanlinemode;
extern int gfx_preventswitch;
extern int gfx_fullscreen;
extern int gfx_redraw;
extern unsigned gfx_windowxsize;
extern unsigned gfx_windowysize;
extern unsigned gfx_virtualxsize;
extern unsigned gfx_virtualysize;
extern SDL_Renderer *gfx_renderer;

extern void (*gp_overlay_render_hook)(void);

#endif
