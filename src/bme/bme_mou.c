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

    // The frame is letterboxed onto the renderer OUTPUT (pixels) with this exact
    // scale/offset (gfx_present / gfx_get_view use the same mapping).
    float scale = 1.0f, offx = 0.0f, offy = 0.0f;
    gfx_get_view(&scale, &offx, &offy);

    // SDL mouse events are in window coordinates (points); the letterbox above
    // is in output pixels. Convert the pointer points -> output pixels using the
    // output/window ratio, then invert the letterbox. Doing both sides in output
    // pixels is what keeps the drawn cursor aligned on HiDPI / scaled displays.
    int outW = 0, outH = 0, winW = 0, winH = 0;
    SDL_GetRendererOutputSize(gfx_renderer, &outW, &outH);
    SDL_GetWindowSize(win_window, &winW, &winH);
    float px = (winW > 0) ? (float)win_mousexpos * (float)outW / (float)winW : (float)win_mousexpos;
    float py = (winH > 0) ? (float)win_mouseypos * (float)outH / (float)winH : (float)win_mouseypos;

    float lx = (px - offx) / scale;
    float ly = (py - offy) / scale;

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

