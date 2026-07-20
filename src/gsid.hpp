#pragma once

#define NUMSIDREGS 0x19
#define SIDWRITEDELAY 9
#define SIDWAVEDELAY 4

typedef struct {
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
} FILTERPARAMS;

void          sid_init(int      speed,
                       unsigned m,
                       unsigned ntsc,
                       unsigned interpolate,
                       unsigned customclockrate,
                       unsigned usefp);
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
