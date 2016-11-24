/*
 * audio resampling
 * Copyright (c) 2004-2012 Michael Niedermayer <michaelni@gmx.at>
 * bessel function: Copyright (c) 2006 Xiaogang Zhang
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * audio resampling
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#include "libavutil/avassert.h"
#include "resample.h"

static inline double eval_poly(const double *coeff, int size, double x) {
    double sum = coeff[size-1];
    int i;
    for (i = size-2; i >= 0; --i) {
        sum *= x;
        sum += coeff[i];
    }
    return sum;
}

/**
 * 0th order modified bessel function of the first kind.
 * Algorithm taken from the Boost project, source:
 * https://searchcode.com/codesearch/view/14918379/
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0 (see notice below).
 * Boost Software License - Version 1.0 - August 17th, 2003
Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
 */

static double bessel(double x) {
// Modified Bessel function of the first kind of order zero
// minimax rational approximations on intervals, see
// Blair and Edwards, Chalk River Report AECL-4928, 1974
    static const double p1[] = {
        -2.2335582639474375249e+15,
        -5.5050369673018427753e+14,
        -3.2940087627407749166e+13,
        -8.4925101247114157499e+11,
        -1.1912746104985237192e+10,
        -1.0313066708737980747e+08,
        -5.9545626019847898221e+05,
        -2.4125195876041896775e+03,
        -7.0935347449210549190e+00,
        -1.5453977791786851041e-02,
        -2.5172644670688975051e-05,
        -3.0517226450451067446e-08,
        -2.6843448573468483278e-11,
        -1.5982226675653184646e-14,
        -5.2487866627945699800e-18,
    };
    static const double q1[] = {
        -2.2335582639474375245e+15,
         7.8858692566751002988e+12,
        -1.2207067397808979846e+10,
         1.0377081058062166144e+07,
        -4.8527560179962773045e+03,
         1.0,
    };
    static const double p2[] = {
        -2.2210262233306573296e-04,
         1.3067392038106924055e-02,
        -4.4700805721174453923e-01,
         5.5674518371240761397e+00,
        -2.3517945679239481621e+01,
         3.1611322818701131207e+01,
        -9.6090021968656180000e+00,
    };
    static const double q2[] = {
        -5.5194330231005480228e-04,
         3.2547697594819615062e-02,
        -1.1151759188741312645e+00,
         1.3982595353892851542e+01,
        -6.0228002066743340583e+01,
         8.5539563258012929600e+01,
        -3.1446690275135491500e+01,
        1.0,
    };
    double y, r, factor;
    if (x == 0)
        return 1.0;
    x = fabs(x);
    if (x <= 15) {
        y = x * x;
        return eval_poly(p1, FF_ARRAY_ELEMS(p1), y) / eval_poly(q1, FF_ARRAY_ELEMS(q1), y);
    }
    else {
        y = 1 / x - 1.0 / 15;
        r = eval_poly(p2, FF_ARRAY_ELEMS(p2), y) / eval_poly(q2, FF_ARRAY_ELEMS(q2), y);
        factor = exp(x) / sqrt(x);
        return factor * r;
    }
}

/**
 * builds a polyphase filterbank.
 * @param factor resampling factor
 * @param scale wanted sum of coefficients for each filter
 * @param filter_type  filter type
 * @param kaiser_beta  kaiser window beta
 * @return 0 on success, negative on error
 */
static int build_filter(ResampleContext *c, void *filter, double factor, int tap_count, int alloc, int phase_count, int scale,
                        int filter_type, double kaiser_beta){
    int ph, i;
    int ph_nb = phase_count % 2 ? phase_count : phase_count / 2 + 1;
    double x, y, w, t, s;
    double *tab = av_malloc_array(tap_count+1,  sizeof(*tab));
    double *sin_lut = av_malloc_array(ph_nb, sizeof(*sin_lut));
    const int center= (tap_count-1)/2;
    int ret = AVERROR(ENOMEM);

    if (!tab || !sin_lut)
        goto fail;

    /* if upsampling, only need to interpolate, no filter */
    if (factor > 1.0)
        factor = 1.0;

    if (factor == 1.0) {
        for (ph = 0; ph < ph_nb; ph++)
            sin_lut[ph] = sin(M_PI * ph / phase_count);
    }
    for(ph = 0; ph < ph_nb; ph++) {
        double norm = 0;
        s = sin_lut[ph];
        for(i=0;i<=tap_count;i++) {
            x = M_PI * ((double)(i - center) - (double)ph / phase_count) * factor;
            if (x == 0) y = 1.0;
            else if (factor == 1.0)
                y = s / x;
            else
                y = sin(x) / x;
            switch(filter_type){
            case SWR_FILTER_TYPE_CUBIC:{
                const float d= -0.5; //first order derivative = -0.5
                x = fabs(((double)(i - center) - (double)ph / phase_count) * factor);
                if(x<1.0) y= 1 - 3*x*x + 2*x*x*x + d*(            -x*x + x*x*x);
                else      y=                       d*(-4 + 8*x - 5*x*x + x*x*x);
                break;}
            case SWR_FILTER_TYPE_BLACKMAN_NUTTALL:
                w = 2.0*x / (factor*tap_count);
                t = -cos(w);
                y *= 0.3635819 - 0.4891775 * t + 0.1365995 * (2*t*t-1) - 0.0106411 * (4*t*t*t - 3*t);
                break;
            case SWR_FILTER_TYPE_KAISER:
                w = 2.0*x / (factor*tap_count*M_PI);
                y *= bessel(kaiser_beta*sqrt(FFMAX(1-w*w, 0)));
                break;
            default:
                av_assert0(0);
            }

            tab[i] = y;
            s = -s;
            if (i < tap_count)
                norm += y;
        }

        /* normalize so that an uniform color remains the same */
        switch(c->format){
        case AV_SAMPLE_FMT_S16P:
            for(i=0;i<tap_count;i++)
                ((int16_t*)filter)[ph * alloc + i] = av_clip_int16(lrintf(tab[i] * scale / norm));
            if (phase_count % 2) break;
            if (tap_count % 2 == 0 || tap_count == 1) {
                for (i = 0; i < tap_count; i++)
                    ((int16_t*)filter)[(phase_count-ph) * alloc + tap_count-1-i] = ((int16_t*)filter)[ph * alloc + i];
            }
            else {
                for (i = 1; i <= tap_count; i++)
                    ((int16_t*)filter)[(phase_count-ph) * alloc + tap_count-i] =
                        av_clip_int16(lrintf(tab[i] * scale / (norm - tab[0] + tab[tap_count])));
            }
            break;
        case AV_SAMPLE_FMT_S32P:
            for(i=0;i<tap_count;i++)
                ((int32_t*)filter)[ph * alloc + i] = av_clipl_int32(llrint(tab[i] * scale / norm));
            if (phase_count % 2) break;
            if (tap_count % 2 == 0 || tap_count == 1) {
                for (i = 0; i < tap_count; i++)
                    ((int32_t*)filter)[(phase_count-ph) * alloc + tap_count-1-i] = ((int32_t*)filter)[ph * alloc + i];
            }
            else {
                for (i = 1; i <= tap_count; i++)
                    ((int32_t*)filter)[(phase_count-ph) * alloc + tap_count-i] =
                        av_clipl_int32(llrint(tab[i] * scale / (norm - tab[0] + tab[tap_count])));
            }
            break;
        case AV_SAMPLE_FMT_FLTP:
            for(i=0;i<tap_count;i++)
                ((float*)filter)[ph * alloc + i] = tab[i] * scale / norm;
            if (phase_count % 2) break;
            if (tap_count % 2 == 0 || tap_count == 1) {
                for (i = 0; i < tap_count; i++)
                    ((float*)filter)[(phase_count-ph) * alloc + tap_count-1-i] = ((float*)filter)[ph * alloc + i];
            }
            else {
                for (i = 1; i <= tap_count; i++)
                    ((float*)filter)[(phase_count-ph) * alloc + tap_count-i] = tab[i] * scale / (norm - tab[0] + tab[tap_count]);
            }
            break;
        case AV_SAMPLE_FMT_DBLP:
            for(i=0;i<tap_count;i++)
                ((double*)filter)[ph * alloc + i] = tab[i] * scale / norm;
            if (phase_count % 2) break;
            if (tap_count % 2 == 0 || tap_count == 1) {
                for (i = 0; i < tap_count; i++)
                    ((double*)filter)[(phase_count-ph) * alloc + tap_count-1-i] = ((double*)filter)[ph * alloc + i];
            }
            else {
                for (i = 1; i <= tap_count; i++)
                    ((double*)filter)[(phase_count-ph) * alloc + tap_count-i] = tab[i] * scale / (norm - tab[0] + tab[tap_count]);
            }
            break;
        }
    }
#if 0
    {
#define LEN 1024
        int j,k;
        double sine[LEN + tap_count];
        double filtered[LEN];
        double maxff=-2, minff=2, maxsf=-2, minsf=2;
        for(i=0; i<LEN; i++){
            double ss=0, sf=0, ff=0;
            for(j=0; j<LEN+tap_count; j++)
                sine[j]= cos(i*j*M_PI/LEN);
            for(j=0; j<LEN; j++){
                double sum=0;
                ph=0;
                for(k=0; k<tap_count; k++)
                    sum += filter[ph * tap_count + k] * sine[k+j];
                filtered[j]= sum / (1<<FILTER_SHIFT);
                ss+= sine[j + center] * sine[j + center];
                ff+= filtered[j] * filtered[j];
                sf+= sine[j + center] * filtered[j];
            }
            ss= sqrt(2*ss/LEN);
            ff= sqrt(2*ff/LEN);
            sf= 2*sf/LEN;
            maxff= FFMAX(maxff, ff);
            minff= FFMIN(minff, ff);
            maxsf= FFMAX(maxsf, sf);
            minsf= FFMIN(minsf, sf);
            if(i%11==0){
                av_log(NULL, AV_LOG_ERROR, "i:%4d ss:%f ff:%13.6e-%13.6e sf:%13.6e-%13.6e\n", i, ss, maxff, minff, maxsf, minsf);
                minff=minsf= 2;
                maxff=maxsf= -2;
            }
        }
    }
#endif

    ret = 0;
fail:
    av_free(tab);
    av_free(sin_lut);
    return ret;
}

static ResampleContext *resample_init(ResampleContext *c, int out_rate, int in_rate, int filter_size, int phase_shift, int linear,
                                    double cutoff0, enum AVSampleFormat format, enum SwrFilterType filter_type, double kaiser_beta,
                                    double precision, int cheby, int exact_rational)
{
    double cutoff = cutoff0? cutoff0 : 0.97;
    double factor= FFMIN(out_rate * cutoff / in_rate, 1.0);
    int phase_count= 1<<phase_shift;
    int phase_count_compensation = phase_count;

    if (exact_rational) {
        int phase_count_exact, phase_count_exact_den;

        av_reduce(&phase_count_exact, &phase_count_exact_den, out_rate, in_rate, INT_MAX);
        if (phase_count_exact <= phase_count) {
            phase_count_compensation = phase_count_exact * (phase_count / phase_count_exact);
            phase_count = phase_count_exact;
        }
    }

    if (!c || c->phase_count != phase_count || c->linear!=linear || c->factor != factor
           || c->filter_length != FFMAX((int)ceil(filter_size/factor), 1) || c->format != format
           || c->filter_type != filter_type || c->kaiser_beta != kaiser_beta) {
        c = av_mallocz(sizeof(*c));
        if (!c)
            return NULL;

        c->format= format;

        c->felem_size= av_get_bytes_per_sample(c->format);

        switch(c->format){
        case AV_SAMPLE_FMT_S16P:
            c->filter_shift = 15;
            break;
        case AV_SAMPLE_FMT_S32P:
            c->filter_shift = 30;
            break;
        case AV_SAMPLE_FMT_FLTP:
        case AV_SAMPLE_FMT_DBLP:
            c->filter_shift = 0;
            break;
        default:
            av_log(NULL, AV_LOG_ERROR, "Unsupported sample format\n");
            av_assert0(0);
        }

        if (filter_size/factor > INT32_MAX/256) {
            av_log(NULL, AV_LOG_ERROR, "Filter length too large\n");
            goto error;
        }

        c->phase_count   = phase_count;
        c->linear        = linear;
        c->factor        = factor;
        c->filter_length = FFMAX((int)ceil(filter_size/factor), 1);
        c->filter_alloc  = FFALIGN(c->filter_length, 8);
        c->filter_bank   = av_calloc(c->filter_alloc, (phase_count+1)*c->felem_size);
        c->filter_type   = filter_type;
        c->kaiser_beta   = kaiser_beta;
        c->phase_count_compensation = phase_count_compensation;
        if (!c->filter_bank)
            goto error;
        if (build_filter(c, (void*)c->filter_bank, factor, c->filter_length, c->filter_alloc, phase_count, 1<<c->filter_shift, filter_type, kaiser_beta))
            goto error;
        memcpy(c->filter_bank + (c->filter_alloc*phase_count+1)*c->felem_size, c->filter_bank, (c->filter_alloc-1)*c->felem_size);
        memcpy(c->filter_bank + (c->filter_alloc*phase_count  )*c->felem_size, c->filter_bank + (c->filter_alloc - 1)*c->felem_size, c->felem_size);
    }

    c->compensation_distance= 0;
    if(!av_reduce(&c->src_incr, &c->dst_incr, out_rate, in_rate * (int64_t)phase_count, INT32_MAX/2))
        goto error;
    while (c->dst_incr < (1<<20) && c->src_incr < (1<<20)) {
        c->dst_incr *= 2;
        c->src_incr *= 2;
    }
    c->ideal_dst_incr = c->dst_incr;
    c->dst_incr_div   = c->dst_incr / c->src_incr;
    c->dst_incr_mod   = c->dst_incr % c->src_incr;

    c->index= -phase_count*((c->filter_length-1)/2);
    c->frac= 0;

    swri_resample_dsp_init(c);

    return c;
error:
    av_freep(&c->filter_bank);
    av_free(c);
    return NULL;
}

static void resample_free(ResampleContext **c){
    if(!*c)
        return;
    av_freep(&(*c)->filter_bank);
    av_freep(c);
}

static int rebuild_filter_bank_with_compensation(ResampleContext *c)
{
    uint8_t *new_filter_bank;
    int new_src_incr, new_dst_incr;
    int phase_count = c->phase_count_compensation;
    int ret;

    if (phase_count == c->phase_count)
        return 0;

    av_assert0(!c->frac && !c->dst_incr_mod && !c->compensation_distance);

    new_filter_bank = av_calloc(c->filter_alloc, (phase_count + 1) * c->felem_size);
    if (!new_filter_bank)
        return AVERROR(ENOMEM);

    ret = build_filter(c, new_filter_bank, c->factor, c->filter_length, c->filter_alloc,
                       phase_count, 1 << c->filter_shift, c->filter_type, c->kaiser_beta);
    if (ret < 0) {
        av_freep(&new_filter_bank);
        return ret;
    }
    memcpy(new_filter_bank + (c->filter_alloc*phase_count+1)*c->felem_size, new_filter_bank, (c->filter_alloc-1)*c->felem_size);
    memcpy(new_filter_bank + (c->filter_alloc*phase_count  )*c->felem_size, new_filter_bank + (c->filter_alloc - 1)*c->felem_size, c->felem_size);

    if (!av_reduce(&new_src_incr, &new_dst_incr, c->src_incr,
                   c->dst_incr * (int64_t)(phase_count/c->phase_count), INT32_MAX/2))
    {
        av_freep(&new_filter_bank);
        return AVERROR(EINVAL);
    }

    c->src_incr = new_src_incr;
    c->dst_incr = new_dst_incr;
    while (c->dst_incr < (1<<20) && c->src_incr < (1<<20)) {
        c->dst_incr *= 2;
        c->src_incr *= 2;
    }
    c->ideal_dst_incr = c->dst_incr;
    c->dst_incr_div   = c->dst_incr / c->src_incr;
    c->dst_incr_mod   = c->dst_incr % c->src_incr;
    c->index         *= phase_count / c->phase_count;
    c->phase_count    = phase_count;
    av_freep(&c->filter_bank);
    c->filter_bank = new_filter_bank;
    return 0;
}

static int set_compensation(ResampleContext *c, int sample_delta, int compensation_distance){
    int ret;

    if (compensation_distance) {
        ret = rebuild_filter_bank_with_compensation(c);
        if (ret < 0)
            return ret;
    }

    c->compensation_distance= compensation_distance;
    if (compensation_distance)
        c->dst_incr = c->ideal_dst_incr - c->ideal_dst_incr * (int64_t)sample_delta / compensation_distance;
    else
        c->dst_incr = c->ideal_dst_incr;

    c->dst_incr_div   = c->dst_incr / c->src_incr;
    c->dst_incr_mod   = c->dst_incr % c->src_incr;

    return 0;
}

static int swri_resample(ResampleContext *c,
                         uint8_t *dst, const uint8_t *src, int *consumed,
                         int src_size, int dst_size, int update_ctx)
{
    if (c->filter_length == 1 && c->phase_count == 1) {
        int index= c->index;
        int frac= c->frac;
        int64_t index2= (1LL<<32)*c->frac/c->src_incr + (1LL<<32)*index;
        int64_t incr= (1LL<<32) * c->dst_incr / c->src_incr;
        int new_size = (src_size * (int64_t)c->src_incr - frac + c->dst_incr - 1) / c->dst_incr;

        dst_size= FFMIN(dst_size, new_size);
        c->dsp.resample_one(dst, src, dst_size, index2, incr);

        index += dst_size * c->dst_incr_div;
        index += (frac + dst_size * (int64_t)c->dst_incr_mod) / c->src_incr;
        av_assert2(index >= 0);
        *consumed= index;
        if (update_ctx) {
            c->frac   = (frac + dst_size * (int64_t)c->dst_incr_mod) % c->src_incr;
            c->index = 0;
        }
    } else {
        int64_t end_index = (1LL + src_size - c->filter_length) * c->phase_count;
        int64_t delta_frac = (end_index - c->index) * c->src_incr - c->frac;
        int delta_n = (delta_frac + c->dst_incr - 1) / c->dst_incr;

        dst_size = FFMIN(dst_size, delta_n);
        if (dst_size > 0) {
            /* resample_linear and resample_common should have same behavior
             * when frac and dst_incr_mod are zero */
            if (c->linear && (c->frac || c->dst_incr_mod))
                *consumed = c->dsp.resample_linear(c, dst, src, dst_size, update_ctx);
            else
                *consumed = c->dsp.resample_common(c, dst, src, dst_size, update_ctx);
        } else {
            *consumed = 0;
        }
    }

    return dst_size;
}

static int multiple_resample(ResampleContext *c, AudioData *dst, int dst_size, AudioData *src, int src_size, int *consumed){
    int i, ret= -1;
    int av_unused mm_flags = av_get_cpu_flags();
    int need_emms = c->format == AV_SAMPLE_FMT_S16P && ARCH_X86_32 &&
                    (mm_flags & (AV_CPU_FLAG_MMX2 | AV_CPU_FLAG_SSE2)) == AV_CPU_FLAG_MMX2;
    int64_t max_src_size = (INT64_MAX/2 / c->phase_count) / c->src_incr;

    if (c->compensation_distance)
        dst_size = FFMIN(dst_size, c->compensation_distance);
    src_size = FFMIN(src_size, max_src_size);

    for(i=0; i<dst->ch_count; i++){
        ret= swri_resample(c, dst->ch[i], src->ch[i],
                           consumed, src_size, dst_size, i+1==dst->ch_count);
    }
    if(need_emms)
        emms_c();

    if (c->compensation_distance) {
        c->compensation_distance -= ret;
        if (!c->compensation_distance) {
            c->dst_incr     = c->ideal_dst_incr;
            c->dst_incr_div = c->dst_incr / c->src_incr;
            c->dst_incr_mod = c->dst_incr % c->src_incr;
        }
    }

    return ret;
}

static int64_t get_delay(struct SwrContext *s, int64_t base){
    ResampleContext *c = s->resample;
    int64_t num = s->in_buffer_count - (c->filter_length-1)/2;
    num *= c->phase_count;
    num -= c->index;
    num *= c->src_incr;
    num -= c->frac;
    return av_rescale(num, base, s->in_sample_rate*(int64_t)c->src_incr * c->phase_count);
}

static int64_t get_out_samples(struct SwrContext *s, int in_samples) {
    ResampleContext *c = s->resample;
    // The + 2 are added to allow implementations to be slightly inaccurate, they should not be needed currently.
    // They also make it easier to proof that changes and optimizations do not
    // break the upper bound.
    int64_t num = s->in_buffer_count + 2LL + in_samples;
    num *= c->phase_count;
    num -= c->index;
    num = av_rescale_rnd(num, s->out_sample_rate, ((int64_t)s->in_sample_rate) * c->phase_count, AV_ROUND_UP) + 2;

    if (c->compensation_distance) {
        if (num > INT_MAX)
            return AVERROR(EINVAL);

        num = FFMAX(num, (num * c->ideal_dst_incr - 1) / c->dst_incr + 1);
    }
    return num;
}

static int resample_flush(struct SwrContext *s) {
    AudioData *a= &s->in_buffer;
    int i, j, ret;
    if((ret = swri_realloc_audio(a, s->in_buffer_index + 2*s->in_buffer_count)) < 0)
        return ret;
    av_assert0(a->planar);
    for(i=0; i<a->ch_count; i++){
        for(j=0; j<s->in_buffer_count; j++){
            memcpy(a->ch[i] + (s->in_buffer_index+s->in_buffer_count+j  )*a->bps,
                a->ch[i] + (s->in_buffer_index+s->in_buffer_count-j-1)*a->bps, a->bps);
        }
    }
    s->in_buffer_count += (s->in_buffer_count+1)/2;
    return 0;
}

// in fact the whole handle multiple ridiculously small buffers might need more thinking...
static int invert_initial_buffer(ResampleContext *c, AudioData *dst, const AudioData *src,
                                 int in_count, int *out_idx, int *out_sz)
{
    int n, ch, num = FFMIN(in_count + *out_sz, c->filter_length + 1), res;

    if (c->index >= 0)
        return 0;

    if ((res = swri_realloc_audio(dst, c->filter_length * 2 + 1)) < 0)
        return res;

    // copy
    for (n = *out_sz; n < num; n++) {
        for (ch = 0; ch < src->ch_count; ch++) {
            memcpy(dst->ch[ch] + ((c->filter_length + n) * c->felem_size),
                   src->ch[ch] + ((n - *out_sz) * c->felem_size), c->felem_size);
        }
    }

    // if not enough data is in, return and wait for more
    if (num < c->filter_length + 1) {
        *out_sz = num;
        *out_idx = c->filter_length;
        return INT_MAX;
    }

    // else invert
    for (n = 1; n <= c->filter_length; n++) {
        for (ch = 0; ch < src->ch_count; ch++) {
            memcpy(dst->ch[ch] + ((c->filter_length - n) * c->felem_size),
                   dst->ch[ch] + ((c->filter_length + n) * c->felem_size),
                   c->felem_size);
        }
    }

    res = num - *out_sz;
    *out_idx = c->filter_length;
    while (c->index < 0) {
        --*out_idx;
        c->index += c->phase_count;
    }
    *out_sz = FFMAX(*out_sz + c->filter_length,
                    1 + c->filter_length * 2) - *out_idx;

    return FFMAX(res, 0);
}

struct Resampler const swri_resampler={
  resample_init,
  resample_free,
  multiple_resample,
  resample_flush,
  set_compensation,
  get_delay,
  invert_initial_buffer,
  get_out_samples,
};
