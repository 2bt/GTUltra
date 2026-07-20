#ifndef GAUDIO_HPP
#define GAUDIO_HPP

#include <SDL.h>

// CHANNEL is defined in gplatform.hpp before this header is included.

int snd_init(unsigned mixrate, unsigned mixmode, unsigned bufferlength, unsigned channels, int usedirectsound);
void snd_uninit(void);
void snd_setcustommixer(void (*custommixer)(Sint32 *dest, unsigned samples));

extern void (*snd_player)(void);
extern CHANNEL *snd_channel;
extern int snd_sndinitted;
extern int snd_bpmtempo;
extern int snd_bpmcount;
extern int snd_channels;
extern unsigned snd_mixmode;
extern unsigned snd_mixrate;

#endif
