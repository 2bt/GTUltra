//
// BME (Blasphemous Multimedia Engine) mouse module
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "bme_main.h"
#include "bme_cfg.h"
#include "bme_win.h"
#include "bme_gfx.h"
#include "bme_io.h"
#include "bme_err.h"

void mou_init(void);
void mou_uninit(void);
void mou_getpos(unsigned *x, unsigned *y);
void mou_getmove(int *dx, int *dy);
unsigned mou_getbuttons(void);

void mou_init(void)
{
    win_mousebuttons = 0;
}

void mou_uninit(void)
{
}

void mou_getpos(unsigned *x, unsigned *y)
{
    if (!gfx_initted || !gfx_renderer || !gfx_screen || !win_window)
    {
        *x = win_mousexpos;
        *y = win_mouseypos;
        return;
    }

    // Map the OS pointer into the surface's (logical) coordinate space, matching
    // the aspect-preserving letterboxed scale used to present the frame.
    //
    // We do this from the window size *in points* (SDL_GetWindowSize), which is
    // the same coordinate space as the mouse events (SDL_MouseMotionEvent). This
    // is deliberately NOT SDL_RenderWindowToLogical: that works in renderer
    // output *pixels*, which differ from event points on HiDPI / scaled
    // displays and would squash the pointer into a fraction of the window.
    int ww = 0, wh = 0;
    SDL_GetWindowSize(win_window, &ww, &wh);
    int lw = gfx_screen->w;
    int lh = gfx_screen->h;
    if (ww <= 0 || wh <= 0 || lw <= 0 || lh <= 0)
    {
        *x = win_mousexpos;
        *y = win_mouseypos;
        return;
    }

    // Uniform scale that fits the logical frame in the window, plus the
    // letterbox offset that centres it.
    float scale = (float)ww / (float)lw;
    float sy = (float)wh / (float)lh;
    if (sy < scale) scale = sy;

    float offx = ((float)ww - (float)lw * scale) * 0.5f;
    float offy = ((float)wh - (float)lh * scale) * 0.5f;

    float lx = ((float)win_mousexpos - offx) / scale;
    float ly = ((float)win_mouseypos - offy) / scale;

    // Clamp into the virtual area (the pointer may sit over letterbox bars).
    if (lx < 0.0f) lx = 0.0f;
    if (ly < 0.0f) ly = 0.0f;
    if (lx > (float)(lw - 1)) lx = (float)(lw - 1);
    if (ly > (float)(lh - 1)) ly = (float)(lh - 1);

    *x = (unsigned)lx;
    *y = (unsigned)ly;
}

void mou_getmove(int *dx, int *dy)
{
    *dx = win_mousexrel;
    *dy = win_mouseyrel;
    win_mousexrel = 0;
    win_mouseyrel = 0;

}

unsigned mou_getbuttons(void)
{
    return win_mousebuttons;
}

