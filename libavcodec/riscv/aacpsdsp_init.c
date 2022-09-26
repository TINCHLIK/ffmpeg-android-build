/*
 * Copyright © 2022 Rémi Denis-Courmont.
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

#include "libavutil/attributes.h"
#include "libavutil/cpu.h"
#include "libavcodec/aacpsdsp.h"

void ff_ps_add_squares_rvv(float *dst, const float (*src)[2], int n);
void ff_ps_mul_pair_single_rvv(float (*dst)[2], float (*src0)[2], float *src1,
                               int n);

av_cold void ff_psdsp_init_riscv(PSDSPContext *c)
{
#if HAVE_RVV
    int flags = av_get_cpu_flags();

    if (flags & AV_CPU_FLAG_RVV_F32) {
        c->add_squares = ff_ps_add_squares_rvv;
        c->mul_pair_single = ff_ps_mul_pair_single_rvv;
    }
#endif
}
