/*
 * VVC DSP init for x86
 *
 * Copyright (C) 2022-2024 Nuo Mi
 * Copyright (c) 2023-2024 Wu Jianhua
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

#include "config.h"

#include "libavutil/cpu.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "libavcodec/vvc/vvcdec.h"
#include "libavcodec/vvc/vvc_ctu.h"
#include "libavcodec/vvc/vvcdsp.h"
#include "libavcodec/x86/h26x/h2656dsp.h"

#define FW_PUT(name, depth, opt) \
static void ff_vvc_put_ ## name ## _ ## depth ## _##opt(int16_t *dst, const uint8_t *src, ptrdiff_t srcstride, \
                                                 int height, const int8_t *hf, const int8_t *vf, int width)    \
{                                                                                                              \
    ff_h2656_put_## name ## _ ## depth ## _##opt(dst, 2 * MAX_PB_SIZE, src, srcstride, height, hf, vf, width); \
}

#define FW_PUT_TAP(fname, bitd, opt ) \
    FW_PUT(fname##4,   bitd, opt );   \
    FW_PUT(fname##8,   bitd, opt );   \
    FW_PUT(fname##16,  bitd, opt );   \
    FW_PUT(fname##32,  bitd, opt );   \
    FW_PUT(fname##64,  bitd, opt );   \
    FW_PUT(fname##128, bitd, opt );   \

#define FW_PUT_4TAP(fname, bitd, opt) \
    FW_PUT(fname ## 2, bitd, opt)     \
    FW_PUT_TAP(fname,  bitd, opt)

#define FW_PUT_4TAP_SSE4(bitd)       \
    FW_PUT_4TAP(pixels,  bitd, sse4) \
    FW_PUT_4TAP(4tap_h,  bitd, sse4) \
    FW_PUT_4TAP(4tap_v,  bitd, sse4) \
    FW_PUT_4TAP(4tap_hv, bitd, sse4)

#define FW_PUT_8TAP_SSE4(bitd)      \
    FW_PUT_TAP(8tap_h,  bitd, sse4) \
    FW_PUT_TAP(8tap_v,  bitd, sse4) \
    FW_PUT_TAP(8tap_hv, bitd, sse4)

#define FW_PUT_SSE4(bitd)  \
    FW_PUT_4TAP_SSE4(bitd) \
    FW_PUT_8TAP_SSE4(bitd)

FW_PUT_SSE4( 8);
FW_PUT_SSE4(10);
FW_PUT_SSE4(12);

#define FW_PUT_TAP_AVX2(n, bitd)        \
    FW_PUT(n ## tap_h32,   bitd, avx2)  \
    FW_PUT(n ## tap_h64,   bitd, avx2)  \
    FW_PUT(n ## tap_h128,  bitd, avx2)  \
    FW_PUT(n ## tap_v32,   bitd, avx2)  \
    FW_PUT(n ## tap_v64,   bitd, avx2)  \
    FW_PUT(n ## tap_v128,  bitd, avx2)

#define FW_PUT_AVX2(bitd) \
    FW_PUT(pixels32,  bitd, avx2) \
    FW_PUT(pixels64,  bitd, avx2) \
    FW_PUT(pixels128, bitd, avx2) \
    FW_PUT_TAP_AVX2(4, bitd)      \
    FW_PUT_TAP_AVX2(8, bitd)      \

FW_PUT_AVX2( 8)
FW_PUT_AVX2(10)
FW_PUT_AVX2(12)

#define FW_PUT_TAP_16BPC_AVX2(n, bitd) \
    FW_PUT(n ## tap_h16,   bitd, avx2) \
    FW_PUT(n ## tap_v16,   bitd, avx2) \
    FW_PUT(n ## tap_hv16,  bitd, avx2) \
    FW_PUT(n ## tap_hv32,  bitd, avx2) \
    FW_PUT(n ## tap_hv64,  bitd, avx2) \
    FW_PUT(n ## tap_hv128, bitd, avx2)

#define FW_PUT_16BPC_AVX2(bitd)     \
    FW_PUT(pixels16, bitd, avx2)    \
    FW_PUT_TAP_16BPC_AVX2(4, bitd)  \
    FW_PUT_TAP_16BPC_AVX2(8, bitd);

FW_PUT_16BPC_AVX2(10);
FW_PUT_16BPC_AVX2(12);

#define PEL_LINK(dst, C, W, idx1, idx2, name, D, opt)                              \
    dst[C][W][idx1][idx2] = ff_vvc_put_## name ## _ ## D ## _##opt;                \
    dst ## _uni[C][W][idx1][idx2] = ff_h2656_put_uni_ ## name ## _ ## D ## _##opt; \

#define MC_TAP_LINKS(pointer, C, my, mx, fname, bitd, opt )          \
    PEL_LINK(pointer, C, 1, my , mx , fname##4 ,  bitd, opt );       \
    PEL_LINK(pointer, C, 2, my , mx , fname##8 ,  bitd, opt );       \
    PEL_LINK(pointer, C, 3, my , mx , fname##16,  bitd, opt );       \
    PEL_LINK(pointer, C, 4, my , mx , fname##32,  bitd, opt );       \
    PEL_LINK(pointer, C, 5, my , mx , fname##64,  bitd, opt );       \
    PEL_LINK(pointer, C, 6, my , mx , fname##128, bitd, opt );

#define MC_8TAP_LINKS(pointer, my, mx, fname, bitd, opt)             \
    MC_TAP_LINKS(pointer, LUMA, my, mx, fname, bitd, opt)

#define MC_8TAP_LINKS_SSE4(bd)                                       \
    MC_8TAP_LINKS(c->inter.put, 0, 0, pixels, bd, sse4);             \
    MC_8TAP_LINKS(c->inter.put, 0, 1, 8tap_h, bd, sse4);             \
    MC_8TAP_LINKS(c->inter.put, 1, 0, 8tap_v, bd, sse4);             \
    MC_8TAP_LINKS(c->inter.put, 1, 1, 8tap_hv, bd, sse4)

#define MC_4TAP_LINKS(pointer, my, mx, fname, bitd, opt)             \
    PEL_LINK(pointer, CHROMA, 0, my , mx , fname##2 ,  bitd, opt );  \
    MC_TAP_LINKS(pointer, CHROMA, my, mx, fname, bitd, opt)          \

#define MC_4TAP_LINKS_SSE4(bd)                                       \
    MC_4TAP_LINKS(c->inter.put, 0, 0, pixels, bd, sse4);             \
    MC_4TAP_LINKS(c->inter.put, 0, 1, 4tap_h, bd, sse4);             \
    MC_4TAP_LINKS(c->inter.put, 1, 0, 4tap_v, bd, sse4);             \
    MC_4TAP_LINKS(c->inter.put, 1, 1, 4tap_hv, bd, sse4)

#define MC_LINK_SSE4(bd)                                             \
    MC_4TAP_LINKS_SSE4(bd)                                           \
    MC_8TAP_LINKS_SSE4(bd)

#define MC_TAP_LINKS_AVX2(C,tap,bd) do {                             \
        PEL_LINK(c->inter.put, C, 4, 0, 0, pixels32,      bd, avx2)  \
        PEL_LINK(c->inter.put, C, 5, 0, 0, pixels64,      bd, avx2)  \
        PEL_LINK(c->inter.put, C, 6, 0, 0, pixels128,     bd, avx2)  \
        PEL_LINK(c->inter.put, C, 4, 0, 1, tap##tap_h32,  bd, avx2)  \
        PEL_LINK(c->inter.put, C, 5, 0, 1, tap##tap_h64,  bd, avx2)  \
        PEL_LINK(c->inter.put, C, 6, 0, 1, tap##tap_h128, bd, avx2)  \
        PEL_LINK(c->inter.put, C, 4, 1, 0, tap##tap_v32,  bd, avx2)  \
        PEL_LINK(c->inter.put, C, 5, 1, 0, tap##tap_v64,  bd, avx2)  \
        PEL_LINK(c->inter.put, C, 6, 1, 0, tap##tap_v128, bd, avx2)  \
    } while (0)

#define MC_LINKS_AVX2(bd)                                            \
    MC_TAP_LINKS_AVX2(LUMA,   8, bd);                                \
    MC_TAP_LINKS_AVX2(CHROMA, 4, bd);

#define MC_TAP_LINKS_16BPC_AVX2(C, tap, bd) do {                     \
        PEL_LINK(c->inter.put, C, 3, 0, 0, pixels16, bd, avx2)       \
        PEL_LINK(c->inter.put, C, 3, 0, 1, tap##tap_h16, bd, avx2)   \
        PEL_LINK(c->inter.put, C, 3, 1, 0, tap##tap_v16, bd, avx2)   \
        PEL_LINK(c->inter.put, C, 3, 1, 1, tap##tap_hv16, bd, avx2)  \
        PEL_LINK(c->inter.put, C, 4, 1, 1, tap##tap_hv32, bd, avx2)  \
        PEL_LINK(c->inter.put, C, 5, 1, 1, tap##tap_hv64, bd, avx2)  \
        PEL_LINK(c->inter.put, C, 6, 1, 1, tap##tap_hv128, bd, avx2) \
    } while (0)

#define MC_LINKS_16BPC_AVX2(bd)                                      \
    MC_TAP_LINKS_16BPC_AVX2(LUMA,   8, bd);                          \
    MC_TAP_LINKS_16BPC_AVX2(CHROMA, 4, bd);

void ff_vvc_dsp_init_x86(VVCDSPContext *const c, const int bd)
{
    const int cpu_flags = av_get_cpu_flags();

    if (ARCH_X86_64) {
        if (bd == 8) {
            if (EXTERNAL_SSE4(cpu_flags)) {
                MC_LINK_SSE4(8);
            }
            if (EXTERNAL_AVX2_FAST(cpu_flags)) {
                MC_LINKS_AVX2(8);
            }
        } else if (bd == 10) {
            if (EXTERNAL_SSE4(cpu_flags)) {
                MC_LINK_SSE4(10);
            }
            if (EXTERNAL_AVX2_FAST(cpu_flags)) {
                MC_LINKS_AVX2(10);
                MC_LINKS_16BPC_AVX2(10);
            }
        } else if (bd == 12) {
            if (EXTERNAL_SSE4(cpu_flags)) {
                MC_LINK_SSE4(12);
            }
            if (EXTERNAL_AVX2_FAST(cpu_flags)) {
                MC_LINKS_AVX2(12);
                MC_LINKS_16BPC_AVX2(12);
            }
        }
    }
}
