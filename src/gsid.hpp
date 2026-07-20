#pragma once

#define NUMSIDREGS 0x19
#define SIDWRITEDELAY 9
#define SIDWAVEDELAY 4

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

void          sid_init(int      sample_rate,
                       bool     use_8580,
                       bool     ntsc,
                       bool     interpolate,
                       unsigned custom_clock_rate,
                       bool     use_fp);
int           sid_fillbuffer(short*       lptr,
                             short*       rptr,
                             short*       lptr2,
                             short*       rptr2,
                             int          samples,
                             int          bufferHalfSize,
                             unsigned int adparam);
unsigned char sid_getorder(unsigned char index, unsigned int adparam);

extern unsigned char sidreg[NUMSIDREGS];
extern unsigned char sidreg2[NUMSIDREGS];
extern unsigned char sidreg3[NUMSIDREGS];
extern unsigned char sidreg4[NUMSIDREGS];
extern FILTERPARAMS  filterparams;
