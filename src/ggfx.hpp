#pragma once

#include <SDL.h>

bool gfx_init();
void gfx_present();

extern SDL_Renderer* gfx_renderer;
