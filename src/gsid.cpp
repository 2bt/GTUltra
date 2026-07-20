/*
 * GTUltra reSID interface
 */

#include <cstdlib>

#include "resid/sid.hpp"
#include "resid-fp/sidfp.hpp"

#include "gsid.hpp"
#include "gsound.hpp"

namespace {

int clockrate;
int samplerate;

uint8_t sidorder[] = {
    0x15, 0x16, 0x18, 0x17, 0x05, 0x06, 0x02, 0x03, 0x00, 0x01, 0x04, 0x0c, 0x0d,
    0x09, 0x0a, 0x07, 0x08, 0x0b, 0x13, 0x14, 0x10, 0x11, 0x0e, 0x0f, 0x12,
};

uint8_t altsidorder[] = {
    0x15, 0x16, 0x18, 0x17, 0x04, 0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x0b, 0x07,
    0x08, 0x09, 0x0a, 0x0c, 0x0d, 0x12, 0x0e, 0x0f, 0x10, 0x11, 0x13, 0x14,
};

SID*   sid    = nullptr;
SID*   sid2   = nullptr;
SID*   sid3   = nullptr;
SID*   sid4   = nullptr;
SIDFP* sidfp  = nullptr;
SIDFP* sidfp2 = nullptr;
SIDFP* sidfp3 = nullptr;
SIDFP* sidfp4 = nullptr;

} // namespace

uint8_t sidreg[NUMSIDREGS];
uint8_t sidreg2[NUMSIDREGS];
uint8_t sidreg3[NUMSIDREGS];
uint8_t sidreg4[NUMSIDREGS];

FILTERPARAMS filterparams = {
    0.50f,
    3.3e6f,
    1.0e-4f,
    1147036.4394268463f,
    274228796.97550374f,
    1.0066634233403395f,
    16125.154840564108f,
    5.5f,
    20.f,
    0.9613160610660189f,
};

extern unsigned residdelay;
// extern unsigned editorInfo.adparam;

// Caller splits editorInfo.interpolate: bit0 → interpolate, bit1+ → use_fp.
void sid_init(int      sample_rate,
              bool     use_8580,
              bool     ntsc,
              bool     interpolate,
              unsigned custom_clock_rate,
              bool     use_fp) {
    int c;

    if (ntsc) clockrate = NTSCCLOCKRATE;
    else clockrate = PALCLOCKRATE;

    if (custom_clock_rate) clockrate = static_cast<int>(custom_clock_rate);

    samplerate = sample_rate;

    if (!use_fp) {
        if (!sid) sid = new SID;
        if (!sid2) sid2 = new SID;
        if (!sid3) sid3 = new SID;
        if (!sid4) sid4 = new SID;


        if (sidfp) {
            delete sidfp;
            sidfp = nullptr;
        }
        if (sidfp2) {
            delete sidfp2;
            sidfp2 = nullptr;
        }
        if (sidfp3) {
            delete sidfp3;
            sidfp3 = nullptr;
        }
        if (sidfp4) {
            delete sidfp4;
            sidfp4 = nullptr;
        }
    }
    else {
        if (!sidfp) sidfp = new SIDFP;
        if (!sidfp2) sidfp2 = new SIDFP;
        if (!sidfp3) sidfp3 = new SIDFP;
        if (!sidfp4) sidfp4 = new SIDFP;

        if (sid) {
            delete sid;
            sid = nullptr;
        }
        if (sid2) {
            delete sid2;
            sid2 = nullptr;
        }
        if (sid3) {
            delete sid3;
            sid3 = nullptr;
        }
        if (sid4) {
            delete sid4;
            sid4 = nullptr;
        }
    }

    if (!interpolate) {
        if (sid) sid->set_sampling_parameters(clockrate, SAMPLE_FAST, sample_rate);
        if (sid2) sid2->set_sampling_parameters(clockrate, SAMPLE_FAST, sample_rate);
        if (sid3) sid3->set_sampling_parameters(clockrate, SAMPLE_FAST, sample_rate);
        if (sid4) sid4->set_sampling_parameters(clockrate, SAMPLE_FAST, sample_rate);

        if (sidfp) sidfp->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sidfp2) sidfp2->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sidfp3) sidfp3->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sidfp4) sidfp4->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
    }
    else {
        if (sid) sid->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sid2) sid2->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sid3) sid3->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sid4) sid4->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, sample_rate);
        if (sidfp) sidfp->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, sample_rate);
        if (sidfp2) sidfp2->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, sample_rate);
        if (sidfp3) sidfp3->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, sample_rate);
        if (sidfp4) sidfp4->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, sample_rate);
    }

    if (sid) sid->reset();
    if (sid2) sid2->reset();
    if (sid3) sid3->reset();
    if (sid4) sid4->reset();
    if (sidfp) sidfp->reset();
    if (sidfp2) sidfp2->reset();
    if (sidfp3) sidfp3->reset();
    if (sidfp4) sidfp4->reset();

    for (c = 0; c < NUMSIDREGS; c++) {
        sidreg[c]  = 0x00;
        sidreg2[c] = 0x00;
        sidreg3[c] = 0x00;
        sidreg4[c] = 0x00;
    }
    const chip_model model = use_8580 ? MOS8580 : MOS6581;
    if (sid) sid->set_chip_model(model);
    if (sid2) sid2->set_chip_model(model);
    if (sid3) sid3->set_chip_model(model);
    if (sid4) sid4->set_chip_model(model);
    if (sidfp) sidfp->set_chip_model(model);
    if (sidfp2) sidfp2->set_chip_model(model);
    if (sidfp3) sidfp3->set_chip_model(model);
    if (sidfp4) sidfp4->set_chip_model(model);

    if (sidfp) {
        sidfp->get_filter().set_distortion_properties(filterparams.distortionrate,
                                                      filterparams.distortionpoint,
                                                      filterparams.distortioncfthreshold);
        sidfp->get_filter().set_type3_properties(filterparams.type3baseresistance,
                                                 filterparams.type3offset,
                                                 filterparams.type3steepness,
                                                 filterparams.type3minimumfetresistance);
        sidfp->get_filter().set_type4_properties(filterparams.type4k, filterparams.type4b);
        sidfp->set_voice_nonlinearity(filterparams.voicenonlinearity);
    }

    if (sidfp2) {
        sidfp2->get_filter().set_distortion_properties(filterparams.distortionrate,
                                                       filterparams.distortionpoint,
                                                       filterparams.distortioncfthreshold);
        sidfp2->get_filter().set_type3_properties(filterparams.type3baseresistance,
                                                  filterparams.type3offset,
                                                  filterparams.type3steepness,
                                                  filterparams.type3minimumfetresistance);
        sidfp2->get_filter().set_type4_properties(filterparams.type4k, filterparams.type4b);
        sidfp2->set_voice_nonlinearity(filterparams.voicenonlinearity);
    }

    if (sidfp3) {
        sidfp3->get_filter().set_distortion_properties(filterparams.distortionrate,
                                                       filterparams.distortionpoint,
                                                       filterparams.distortioncfthreshold);
        sidfp3->get_filter().set_type3_properties(filterparams.type3baseresistance,
                                                  filterparams.type3offset,
                                                  filterparams.type3steepness,
                                                  filterparams.type3minimumfetresistance);
        sidfp3->get_filter().set_type4_properties(filterparams.type4k, filterparams.type4b);
        sidfp3->set_voice_nonlinearity(filterparams.voicenonlinearity);
    }

    if (sidfp4) {
        sidfp4->get_filter().set_distortion_properties(filterparams.distortionrate,
                                                       filterparams.distortionpoint,
                                                       filterparams.distortioncfthreshold);
        sidfp4->get_filter().set_type3_properties(filterparams.type3baseresistance,
                                                  filterparams.type3offset,
                                                  filterparams.type3steepness,
                                                  filterparams.type3minimumfetresistance);
        sidfp4->get_filter().set_type4_properties(filterparams.type4k, filterparams.type4b);
        sidfp4->set_voice_nonlinearity(filterparams.voicenonlinearity);
    }
}

uint8_t sid_getorder(uint8_t index, unsigned int adparam) {
    if (adparam >= 0xf000) return altsidorder[index];
    else return sidorder[index];
}

// Left = 0-samples-1
// Right = samples>s(amples*2)-1
int sid_fillbuffer(short*       lptr,
                   short*       rptr,
                   short*       lptr2,
                   short*       rptr2,
                   int          samples,
                   int          bufferHalfSize,
                   unsigned int adparam) {
    int tdelta;
    int tdelta2;
    int result = 0;
    int total  = 0;
    int c;

    int badline = rand() % NUMSIDREGS;

    tdelta = clockrate * samples / samplerate;
    if (tdelta <= 0) return total;


    for (c = 0; c < NUMSIDREGS; c++) {
        uint8_t o = sid_getorder(static_cast<uint8_t>(c), adparam);

        // Extra delay for loading the waveform (and mt_chngate,x)
        if ((o == 4) || (o == 11) || (o == 18)) {
            tdelta2 = SIDWAVEDELAY;
            if (sid) result = sid->clock(tdelta2, lptr, samples, bufferHalfSize);
            if (sidfp) result = sidfp->clock(tdelta2, lptr, samples, bufferHalfSize);
            tdelta2 = SIDWAVEDELAY;
            if (sid2) sid2->clock(tdelta2, rptr, samples, bufferHalfSize);
            if (sidfp2) sidfp2->clock(tdelta2, rptr, samples, bufferHalfSize);

            tdelta2 = SIDWAVEDELAY;
            if (sid3) sid3->clock(tdelta2, lptr2, samples, bufferHalfSize);
            if (sidfp3) sidfp3->clock(tdelta2, lptr2, samples, bufferHalfSize);

            tdelta2 = SIDWAVEDELAY;
            if (sid4) sid4->clock(tdelta2, rptr2, samples, bufferHalfSize);
            if (sidfp4) sidfp4->clock(tdelta2, rptr2, samples, bufferHalfSize);

            total += result;
            lptr += result;
            rptr += result;
            lptr2 += result;
            rptr2 += result;
            samples -= result;
            tdelta -= SIDWAVEDELAY;
        }

        // Possible random badline delay once per writing
        if ((badline == c) && (residdelay)) {
            tdelta2 = residdelay;
            if (sid) result = sid->clock(tdelta2, lptr, samples, bufferHalfSize);
            if (sidfp) result = sidfp->clock(tdelta2, lptr, samples, bufferHalfSize);
            tdelta2 = residdelay;
            if (sid2) sid2->clock(tdelta2, rptr, samples, bufferHalfSize);
            if (sidfp2) sidfp2->clock(tdelta2, rptr, samples, bufferHalfSize);

            tdelta2 = residdelay;
            if (sid3) sid3->clock(tdelta2, lptr2, samples, bufferHalfSize);
            if (sidfp3) sidfp3->clock(tdelta2, lptr2, samples, bufferHalfSize);

            tdelta2 = residdelay;
            if (sid4) sid4->clock(tdelta2, rptr2, samples, bufferHalfSize);
            if (sidfp4) sidfp4->clock(tdelta2, rptr2, samples, bufferHalfSize);

            total += result;
            lptr += result;
            rptr += result;
            lptr2 += result;
            rptr2 += result;
            samples -= result;
            tdelta -= residdelay;
        }

        if (sid) sid->write(o, sidreg[o]);
        if (sidfp) sidfp->write(o, sidreg[o]);
        if (sid2) sid2->write(o, sidreg2[o]);
        if (sidfp2) sidfp2->write(o, sidreg2[o]);
        if (sid3) sid3->write(o, sidreg3[o]);
        if (sidfp3) sidfp3->write(o, sidreg3[o]);
        if (sid4) sid4->write(o, sidreg4[o]);
        if (sidfp4) sidfp4->write(o, sidreg4[o]);

        tdelta2 = SIDWRITEDELAY;
        if (sid) result = sid->clock(tdelta2, lptr, samples, bufferHalfSize);
        if (sidfp) result = sidfp->clock(tdelta2, lptr, samples, bufferHalfSize);
        tdelta2 = SIDWRITEDELAY;
        if (sid2) sid2->clock(tdelta2, rptr, samples, bufferHalfSize);
        if (sidfp2) sidfp2->clock(tdelta2, rptr, samples, bufferHalfSize);
        tdelta2 = SIDWRITEDELAY;
        if (sid3) sid3->clock(tdelta2, lptr2, samples, bufferHalfSize);
        if (sidfp3) sidfp3->clock(tdelta2, lptr2, samples, bufferHalfSize);
        tdelta2 = SIDWRITEDELAY;
        if (sid4) sid4->clock(tdelta2, rptr2, samples, bufferHalfSize);
        if (sidfp4) sidfp4->clock(tdelta2, rptr2, samples, bufferHalfSize);

        total += result;
        lptr += result;
        rptr += result;
        lptr2 += result;
        rptr2 += result;
        samples -= result;
        tdelta -= SIDWRITEDELAY;

        if (tdelta <= 0) return total;
    }

    tdelta2 = tdelta;
    if (sid) result = sid->clock(tdelta2, lptr, samples, bufferHalfSize);
    if (sidfp) result = sidfp->clock(tdelta2, lptr, samples, bufferHalfSize);
    tdelta2 = tdelta;
    if (sid2) result = sid2->clock(tdelta2, rptr, samples, bufferHalfSize);
    if (sidfp2) result = sidfp2->clock(tdelta2, rptr, samples, bufferHalfSize);
    tdelta2 = tdelta;
    if (sid3) result = sid3->clock(tdelta2, lptr2, samples, bufferHalfSize);
    if (sidfp3) result = sidfp3->clock(tdelta2, lptr2, samples, bufferHalfSize);
    tdelta2 = tdelta;
    if (sid4) result = sid4->clock(tdelta2, rptr2, samples, bufferHalfSize);
    if (sidfp4) result = sidfp4->clock(tdelta2, rptr2, samples, bufferHalfSize);

    total += result;
    lptr += result;
    rptr += result;
    lptr2 += result;
    rptr2 += result;
    samples -= result;

    // Loop extra cycles until all samples produced
    while (samples) {
        tdelta = clockrate * samples / samplerate;
        if (tdelta <= 0) return total;

        if (sid) result = sid->clock(tdelta, lptr, samples, bufferHalfSize);
        if (sidfp) result = sidfp->clock(tdelta, lptr, samples, bufferHalfSize);
        tdelta = clockrate * samples / samplerate;
        if (sid2) result = sid2->clock(tdelta, rptr, samples, bufferHalfSize);
        if (sidfp2) result = sidfp2->clock(tdelta, rptr, samples, bufferHalfSize);
        tdelta = clockrate * samples / samplerate;
        if (sid3) result = sid3->clock(tdelta, lptr2, samples, bufferHalfSize);
        if (sidfp3) result = sidfp3->clock(tdelta, lptr2, samples, bufferHalfSize);
        tdelta = clockrate * samples / samplerate;
        if (sid4) result = sid4->clock(tdelta, rptr2, samples, bufferHalfSize);
        if (sidfp4) result = sidfp4->clock(tdelta, rptr2, samples, bufferHalfSize);
        total += result;
        lptr += result;
        rptr += result;
        lptr2 += result;
        rptr2 += result;
        samples -= result;
    }


    return total;
}
