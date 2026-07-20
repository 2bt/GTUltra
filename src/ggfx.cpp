#include "gimgui.hpp"
#include "gplatform.hpp"

SDL_Renderer* gfx_renderer = nullptr;

bool gfx_init() {
    if (gfx_renderer) return true;

    gfx_renderer = SDL_CreateRenderer(win_window, -1, SDL_RENDERER_ACCELERATED);
    if (!gfx_renderer) return false;

    win_reapply_mousemode();
    return true;
}

void gfx_present() {
    if (!gfx_renderer) return;
    SDL_SetRenderDrawColor(gfx_renderer, 0, 0, 0, 255);
    SDL_RenderClear(gfx_renderer);
    gimgui_render();
    SDL_RenderPresent(gfx_renderer);
}
