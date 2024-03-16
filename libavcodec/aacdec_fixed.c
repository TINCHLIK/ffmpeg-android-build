/*
 * Copyright (c) 2013
 *      MIPS Technologies, Inc., California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the MIPS Technologies, Inc., nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE MIPS TECHNOLOGIES, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE MIPS TECHNOLOGIES, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * AAC decoder fixed-point implementation
 *
 * Copyright (c) 2005-2006 Oded Shimon ( ods15 ods15 dyndns org )
 * Copyright (c) 2006-2007 Maxim Gavrilov ( maxim.gavrilov gmail com )
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
 * AAC decoder
 * @author Oded Shimon  ( ods15 ods15 dyndns org )
 * @author Maxim Gavrilov ( maxim.gavrilov gmail com )
 *
 * Fixed point implementation
 * @author Stanislav Ocovaj ( stanislav.ocovaj imgtec com )
 */

#define USE_FIXED 1

#include "libavutil/fixed_dsp.h"
#include "avcodec.h"
#include "codec_internal.h"
#include "get_bits.h"

#include "aac.h"
#include "aacdec.h"
#include "aactab.h"
#include "aac/aacdec_tab.h"
#include "adts_header.h"
#include "cbrt_data.h"
#include "aacsbr.h"
#include "mpeg4audio.h"
#include "profiles.h"
#include "libavutil/intfloat.h"

#include <math.h>
#include <string.h>

DECLARE_ALIGNED(32, int, AAC_RENAME2(aac_kbd_long_1024))[1024];
DECLARE_ALIGNED(32, int, AAC_RENAME2(aac_kbd_short_128))[128];
DECLARE_ALIGNED(32, int, AAC_RENAME2(aac_kbd_long_960))[960];
DECLARE_ALIGNED(32, int, AAC_RENAME2(aac_kbd_short_120))[120];

/* @name ltp_coef
 * Table of the LTP coefficients
 */
static const int ltp_coef_fixed[8] = {
    Q30(0.570829), Q30(0.696616), Q30(0.813004), Q30(0.911304),
    Q30(0.984900), Q30(1.067894), Q30(1.194601), Q30(1.369533),
};

/* @name tns_tmp2_map
 * Tables of the tmp2[] arrays of LPC coefficients used for TNS.
 * The suffix _M_N[] indicate the values of coef_compress and coef_res
 * respectively.
 * @{
 */
static const int tns_tmp2_map_1_3[4] = {
    Q31(0.00000000), Q31(-0.43388373),  Q31(0.64278758),  Q31(0.34202015),
};

static const int tns_tmp2_map_0_3[8] = {
    Q31(0.00000000), Q31(-0.43388373), Q31(-0.78183150), Q31(-0.97492790),
    Q31(0.98480773), Q31( 0.86602539), Q31( 0.64278758), Q31( 0.34202015),
};

static const int tns_tmp2_map_1_4[8] = {
    Q31(0.00000000), Q31(-0.20791170), Q31(-0.40673664), Q31(-0.58778524),
    Q31(0.67369562), Q31( 0.52643216), Q31( 0.36124167), Q31( 0.18374951),
};

static const int tns_tmp2_map_0_4[16] = {
    Q31( 0.00000000), Q31(-0.20791170), Q31(-0.40673664), Q31(-0.58778524),
    Q31(-0.74314481), Q31(-0.86602539), Q31(-0.95105654), Q31(-0.99452192),
    Q31( 0.99573416), Q31( 0.96182561), Q31( 0.89516330), Q31( 0.79801720),
    Q31( 0.67369562), Q31( 0.52643216), Q31( 0.36124167), Q31( 0.18374951),
};

static const int * const tns_tmp2_map_fixed[4] = {
    tns_tmp2_map_0_3,
    tns_tmp2_map_0_4,
    tns_tmp2_map_1_3,
    tns_tmp2_map_1_4
};
// @}

static av_always_inline void reset_predict_state(PredictorState *ps)
{
    ps->r0.mant   = 0;
    ps->r0.exp   = 0;
    ps->r1.mant   = 0;
    ps->r1.exp   = 0;
    ps->cor0.mant = 0;
    ps->cor0.exp = 0;
    ps->cor1.mant = 0;
    ps->cor1.exp = 0;
    ps->var0.mant = 0x20000000;
    ps->var0.exp = 1;
    ps->var1.mant = 0x20000000;
    ps->var1.exp = 1;
}

static const int exp2tab[4] = { Q31(1.0000000000/2), Q31(1.1892071150/2), Q31(1.4142135624/2), Q31(1.6817928305/2) };  // 2^0, 2^0.25, 2^0.5, 2^0.75

static av_always_inline SoftFloat flt16_round(SoftFloat pf)
{
    SoftFloat tmp;
    int s;

    tmp.exp = pf.exp;
    s = pf.mant >> 31;
    tmp.mant = (pf.mant ^ s) - s;
    tmp.mant = (tmp.mant + 0x00200000U) & 0xFFC00000U;
    tmp.mant = (tmp.mant ^ s) - s;

    return tmp;
}

static av_always_inline SoftFloat flt16_even(SoftFloat pf)
{
    SoftFloat tmp;
    int s;

    tmp.exp = pf.exp;
    s = pf.mant >> 31;
    tmp.mant = (pf.mant ^ s) - s;
    tmp.mant = (tmp.mant + 0x001FFFFFU + (tmp.mant & 0x00400000U >> 16)) & 0xFFC00000U;
    tmp.mant = (tmp.mant ^ s) - s;

    return tmp;
}

static av_always_inline SoftFloat flt16_trunc(SoftFloat pf)
{
    SoftFloat pun;
    int s;

    pun.exp = pf.exp;
    s = pf.mant >> 31;
    pun.mant = (pf.mant ^ s) - s;
    pun.mant = pun.mant & 0xFFC00000U;
    pun.mant = (pun.mant ^ s) - s;

    return pun;
}

static av_always_inline void predict(PredictorState *ps, int *coef,
                                     int output_enable)
{
    const SoftFloat a     = { 1023410176, 0 };  // 61.0 / 64
    const SoftFloat alpha = {  973078528, 0 };  // 29.0 / 32
    SoftFloat e0, e1;
    SoftFloat pv;
    SoftFloat k1, k2;
    SoftFloat   r0 = ps->r0,     r1 = ps->r1;
    SoftFloat cor0 = ps->cor0, cor1 = ps->cor1;
    SoftFloat var0 = ps->var0, var1 = ps->var1;
    SoftFloat tmp;

    if (var0.exp > 1 || (var0.exp == 1 && var0.mant > 0x20000000)) {
        k1 = av_mul_sf(cor0, flt16_even(av_div_sf(a, var0)));
    }
    else {
        k1.mant = 0;
        k1.exp = 0;
    }

    if (var1.exp > 1 || (var1.exp == 1 && var1.mant > 0x20000000)) {
        k2 = av_mul_sf(cor1, flt16_even(av_div_sf(a, var1)));
    }
    else {
        k2.mant = 0;
        k2.exp = 0;
    }

    tmp = av_mul_sf(k1, r0);
    pv = flt16_round(av_add_sf(tmp, av_mul_sf(k2, r1)));
    if (output_enable) {
        int shift = 28 - pv.exp;

        if (shift < 31) {
            if (shift > 0) {
                *coef += (unsigned)((pv.mant + (1 << (shift - 1))) >> shift);
            } else
                *coef += (unsigned)pv.mant << -shift;
        }
    }

    e0 = av_int2sf(*coef, 2);
    e1 = av_sub_sf(e0, tmp);

    ps->cor1 = flt16_trunc(av_add_sf(av_mul_sf(alpha, cor1), av_mul_sf(r1, e1)));
    tmp = av_add_sf(av_mul_sf(r1, r1), av_mul_sf(e1, e1));
    tmp.exp--;
    ps->var1 = flt16_trunc(av_add_sf(av_mul_sf(alpha, var1), tmp));
    ps->cor0 = flt16_trunc(av_add_sf(av_mul_sf(alpha, cor0), av_mul_sf(r0, e0)));
    tmp = av_add_sf(av_mul_sf(r0, r0), av_mul_sf(e0, e0));
    tmp.exp--;
    ps->var0 = flt16_trunc(av_add_sf(av_mul_sf(alpha, var0), tmp));

    ps->r1 = flt16_trunc(av_mul_sf(a, av_sub_sf(r0, av_mul_sf(k1, e0))));
    ps->r0 = flt16_trunc(av_mul_sf(a, e0));
}

#include "aacdec_template.c"

const FFCodec ff_aac_fixed_decoder = {
    .p.name          = "aac_fixed",
    CODEC_LONG_NAME("AAC (Advanced Audio Coding)"),
    .p.type          = AVMEDIA_TYPE_AUDIO,
    .p.id            = AV_CODEC_ID_AAC,
    .p.priv_class    = &ff_aac_decoder_class,
    .priv_data_size  = sizeof(AACDecContext),
    .init            = aac_decode_init,
    .close           = ff_aac_decode_close,
    FF_CODEC_DECODE_CB(aac_decode_frame),
    .p.sample_fmts   = (const enum AVSampleFormat[]) {
        AV_SAMPLE_FMT_S32P, AV_SAMPLE_FMT_NONE
    },
    .p.capabilities  = AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DR1,
    .caps_internal   = FF_CODEC_CAP_INIT_CLEANUP,
    .p.ch_layouts    = ff_aac_ch_layout,
    .p.profiles      = NULL_IF_CONFIG_SMALL(ff_aac_profiles),
    .flush = flush,
};
