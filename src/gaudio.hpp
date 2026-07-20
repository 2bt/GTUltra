#pragma once

#include <SDL.h>

// Custom-mixer SID path used by gsound. Returns false on failure.
bool snd_init(unsigned mixrate, unsigned mixmode, unsigned bufferlength_ms);
void snd_set_custom_mixer(void (*custom_mixer)(Sint32* dest, unsigned samples));

// Consumers: gsound (player callback, tempo, mixrate).
extern void (*snd_player)();
extern int      snd_bpmtempo;
extern unsigned snd_mixrate;
