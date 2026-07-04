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
    if (!gfx_initted || !gfx_renderer)
    {
        *x = win_mousexpos;
        *y = win_mouseypos;
        return;
    }

    // Map the OS pointer (window pixels) into the renderer's logical space,
    // which SDL keeps in sync with the aspect-preserving scale used to present
    // the frame. Works correctly when the window is resized or fullscreen.
    float lx = 0.0f, ly = 0.0f;
    SDL_RenderWindowToLogical(gfx_renderer, win_mousexpos, win_mouseypos, &lx, &ly);

    // Clamp into the virtual area (the pointer may sit over letterbox bars).
    if (lx < 0.0f) lx = 0.0f;
    if (ly < 0.0f) ly = 0.0f;
    if (lx > (float)(gfx_virtualxsize - 1)) lx = (float)(gfx_virtualxsize - 1);
    if (ly > (float)(gfx_virtualysize - 1)) ly = (float)(gfx_virtualysize - 1);

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

