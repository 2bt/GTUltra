#include "gplatform.hpp"

#include <cstdlib>
#include <cstring>

void (*snd_player)(void) = nullptr;
CHANNEL *snd_channel = nullptr;
int snd_channels = 0;
int snd_sndinitted = 0;
int snd_bpmcount = 0;
int snd_bpmtempo = 125;
unsigned snd_mixmode = 0;
unsigned snd_mixrate = 0;

static void (*snd_custommixer)(Sint32 *dest, unsigned samples) = nullptr;
static unsigned snd_buffersize = 0;
static unsigned snd_samplesize = 0;
static unsigned snd_previouschannels = 0xffffffffu;
static int snd_atexit_registered = 0;
static Sint32 *snd_clipbuffer = nullptr;
static SDL_AudioSpec desired;
static SDL_AudioSpec obtained;

static int snd_initmixer(void);
static void snd_uninitmixer(void);
static void snd_mixdata(Uint8 *dest, unsigned bytes);
static void snd_mixer(void *userdata, Uint8 *stream, int len);
static void snd_clearclipbuffer(Sint32 *clipbuffer, unsigned clipsamples);
static void snd_16bit_postprocess(Sint32 *src, Sint16 *dest, unsigned samples);
static void snd_8bit_postprocess(Sint32 *src, Uint8 *dest, unsigned samples);

int snd_init(unsigned mixrate, unsigned mixmode, unsigned bufferlength, unsigned channels, int /*usedirectsound*/)
{
	if (!snd_atexit_registered)
	{
		atexit(snd_uninit);
		snd_atexit_registered = 1;
	}

	snd_uninit();

	if ((channels < 1) || (!mixrate) || (!bufferlength))
	{
		bme_error = BME_ILLEGAL_CONFIG;
		snd_uninit();
		return BME_ERROR;
	}

	desired.freq = (int)mixrate;
	desired.format = AUDIO_U8;
	if (mixmode & SIXTEENBIT)
		desired.format = AUDIO_S16SYS;
	desired.channels = 1;
	if (mixmode & STEREO)
		desired.channels = 2;
	desired.samples = (Uint16)(bufferlength * mixrate / 1000);
	{
		int bits = 0;
		for (;;)
		{
			desired.samples = (Uint16)(desired.samples >> 1);
			if (!desired.samples)
				break;
			bits++;
		}
		desired.samples = (Uint16)(1 << bits);
	}

	desired.callback = snd_mixer;
	desired.userdata = nullptr;

	snd_bpmcount = 0;

	if (snd_previouschannels != channels)
	{
		if (snd_channel)
		{
			free(snd_channel);
			snd_channel = nullptr;
			snd_channels = 0;
		}

		snd_channel = (CHANNEL *)malloc(channels * sizeof(CHANNEL));
		if (!snd_channel)
		{
			bme_error = BME_OUT_OF_MEMORY;
			snd_uninit();
			return BME_ERROR;
		}
		CHANNEL *chptr = &snd_channel[0];
		snd_channels = (int)channels;
		snd_previouschannels = channels;

		for (int c = snd_channels; c > 0; c--)
		{
			chptr->voicemode = VM_OFF;
			chptr->smp = nullptr;
			chptr->mastervol = 64;
			chptr++;
		}
	}

	SDL_PauseAudio(1);

	if (SDL_OpenAudio(&desired, &obtained))
	{
		bme_error = BME_OPEN_ERROR;
		snd_uninit();
		return BME_ERROR;
	}
	snd_sndinitted = 1;

	snd_mixmode = 0;
	snd_samplesize = 1;
	if (obtained.channels == 2)
	{
		snd_mixmode |= STEREO;
		snd_samplesize <<= 1;
	}
	if ((obtained.format == AUDIO_S16SYS) ||
	    (obtained.format == AUDIO_S16LSB) ||
	    (obtained.format == AUDIO_S16MSB))
	{
		snd_mixmode |= SIXTEENBIT;
		snd_samplesize <<= 1;
	}
	snd_buffersize = obtained.size;
	snd_mixrate = (unsigned)obtained.freq;

	if (!snd_initmixer())
	{
		bme_error = BME_OUT_OF_MEMORY;
		snd_uninit();
		return BME_ERROR;
	}

	SDL_PauseAudio(0);

	bme_error = BME_OK;
	return BME_OK;
}

void snd_uninit(void)
{
	if (snd_sndinitted)
	{
		SDL_CloseAudio();
		snd_sndinitted = 0;
	}
	snd_uninitmixer();
}

void snd_setcustommixer(void (*custommixer)(Sint32 *dest, unsigned samples))
{
	snd_custommixer = custommixer;
}

static int snd_initmixer(void)
{
	snd_uninitmixer();

	if (snd_mixmode & STEREO)
		snd_clipbuffer = (Sint32 *)malloc((snd_buffersize / snd_samplesize) * sizeof(int) * 2);
	else
		snd_clipbuffer = (Sint32 *)malloc((snd_buffersize / snd_samplesize) * sizeof(int));
	if (!snd_clipbuffer)
		return 0;
	return 1;
}

static void snd_uninitmixer(void)
{
	if (snd_clipbuffer)
	{
		free(snd_clipbuffer);
		snd_clipbuffer = nullptr;
	}
}

static void snd_mixer(void * /*userdata*/, Uint8 *stream, int len)
{
	snd_mixdata(stream, (unsigned)len);
}

static void snd_mixdata(Uint8 *dest, unsigned bytes)
{
	unsigned mixsamples = bytes;
	unsigned clipsamples = bytes;
	Sint32 *clipptr = snd_clipbuffer;
	if (snd_mixmode & STEREO)
		mixsamples >>= 1;
	if (snd_mixmode & SIXTEENBIT)
	{
		clipsamples >>= 1;
		mixsamples >>= 1;
	}
	snd_clearclipbuffer(snd_clipbuffer, clipsamples);
	if (snd_player)
	{
		while (mixsamples)
		{
			if ((!snd_bpmcount) && (snd_player))
			{
				snd_player();
				snd_bpmcount = (int)(((snd_mixrate * 5) >> 1) / (unsigned)snd_bpmtempo);
			}

			unsigned musicsamples = mixsamples;
			if ((int)musicsamples > snd_bpmcount)
				musicsamples = (unsigned)snd_bpmcount;
			snd_bpmcount -= (int)musicsamples;
			if (snd_custommixer)
				snd_custommixer(clipptr, musicsamples);
			if (snd_mixmode & STEREO)
				clipptr += musicsamples * 2;
			else
				clipptr += musicsamples;
			mixsamples -= musicsamples;
		}
	}
	else if (snd_custommixer)
	{
		snd_custommixer(clipptr, mixsamples);
	}

	clipptr = snd_clipbuffer;
	if (snd_mixmode & SIXTEENBIT)
		snd_16bit_postprocess(clipptr, (Sint16 *)dest, clipsamples);
	else
		snd_8bit_postprocess(clipptr, dest, clipsamples);
}

static void snd_clearclipbuffer(Sint32 *clipbuffer, unsigned clipsamples)
{
	memset(clipbuffer, 0, clipsamples * sizeof(int));
}

static void snd_16bit_postprocess(Sint32 *src, Sint16 *dest, unsigned samples)
{
	while (samples--)
	{
		int sample = *src++;
		if (sample > 32767) sample = 32767;
		if (sample < -32768) sample = -32768;
		*dest++ = (Sint16)sample;
	}
}

static void snd_8bit_postprocess(Sint32 *src, Uint8 *dest, unsigned samples)
{
	while (samples--)
	{
		int sample = *src++;
		if (sample > 32767) sample = 32767;
		if (sample < -32768) sample = -32768;
		*dest++ = (Uint8)((sample >> 8) + 128);
	}
}
