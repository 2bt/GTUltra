#pragma once

#include <cstdint>

constexpr int NUMSIDREGS    = 0x19;
constexpr int SIDWRITEDELAY = 9;
constexpr int SIDWAVEDELAY  = 4;

struct FILTERPARAMS {
    float distortionrate;
    float distortionpoint;
    float distortioncfthreshold;
    float type3baseresistance;
    float type3offset;
    float type3steepness;
    float type3minimumfetresistance;
    float type4k;
    float type4b;
    float voicenonlinearity;
};

void    sid_init(int      sample_rate,
                 bool     use_8580,
                 bool     ntsc,
                 bool     interpolate,
                 unsigned custom_clock_rate,
                 bool     use_fp);
int     sid_fillbuffer(short*       lptr,
                       short*       rptr,
                       short*       lptr2,
                       short*       rptr2,
                       int          samples,
                       int          bufferHalfSize,
                       unsigned int adparam);
uint8_t sid_getorder(uint8_t index, unsigned int adparam);

extern uint8_t      sidreg[NUMSIDREGS];
extern uint8_t      sidreg2[NUMSIDREGS];
extern uint8_t      sidreg3[NUMSIDREGS];
extern uint8_t      sidreg4[NUMSIDREGS];
extern FILTERPARAMS filterparams;
