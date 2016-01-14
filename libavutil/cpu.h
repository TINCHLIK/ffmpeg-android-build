/*
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVUTIL_CPU_H
#define AVUTIL_CPU_H

#include "version.h"

#define AV_CPU_FLAG_FORCE    0x80000000 /* force usage of selected flags (OR) */

    /* lower 16 bits - CPU features */
#define AV_CPU_FLAG_MMX          0x0001 ///< standard MMX
#define AV_CPU_FLAG_MMXEXT       0x0002 ///< SSE integer functions or AMD MMX ext
#define AV_CPU_FLAG_3DNOW        0x0004 ///< AMD 3DNOW
#define AV_CPU_FLAG_SSE          0x0008 ///< SSE functions
#define AV_CPU_FLAG_SSE2         0x0010 ///< PIV SSE2 functions
#define AV_CPU_FLAG_SSE2SLOW 0x40000000 ///< SSE2 supported, but usually not faster
                                        ///< than regular MMX/SSE (e.g. Core1)
#define AV_CPU_FLAG_3DNOWEXT     0x0020 ///< AMD 3DNowExt
#define AV_CPU_FLAG_SSE3         0x0040 ///< Prescott SSE3 functions
#define AV_CPU_FLAG_SSE3SLOW 0x20000000 ///< SSE3 supported, but usually not faster
                                        ///< than regular MMX/SSE (e.g. Core1)
#define AV_CPU_FLAG_SSSE3        0x0080 ///< Conroe SSSE3 functions
#define AV_CPU_FLAG_SSSE3SLOW 0x4000000 ///< SSSE3 supported, but usually not faster
#define AV_CPU_FLAG_ATOM     0x10000000 ///< Atom processor, some SSSE3 instructions are slower
#define AV_CPU_FLAG_SSE4         0x0100 ///< Penryn SSE4.1 functions
#define AV_CPU_FLAG_SSE42        0x0200 ///< Nehalem SSE4.2 functions
#define AV_CPU_FLAG_AVX          0x4000 ///< AVX functions: requires OS support even if YMM registers aren't used
#define AV_CPU_FLAG_AVXSLOW   0x8000000 ///< AVX supported, but slow when using YMM registers (e.g. Bulldozer)
#define AV_CPU_FLAG_XOP          0x0400 ///< Bulldozer XOP functions
#define AV_CPU_FLAG_FMA4         0x0800 ///< Bulldozer FMA4 functions
#define AV_CPU_FLAG_CMOV         0x1000 ///< i686 cmov
#define AV_CPU_FLAG_AVX2         0x8000 ///< AVX2 functions: requires OS support even if YMM registers aren't used
#define AV_CPU_FLAG_FMA3        0x10000 ///< Haswell FMA3 functions
#define AV_CPU_FLAG_BMI1        0x20000 ///< Bit Manipulation Instruction Set 1
#define AV_CPU_FLAG_BMI2        0x40000 ///< Bit Manipulation Instruction Set 2

#define AV_CPU_FLAG_ALTIVEC      0x0001 ///< standard
#define AV_CPU_FLAG_VSX          0x0002 ///< ISA 2.06
#define AV_CPU_FLAG_POWER8       0x0004 ///< ISA 2.07

#define AV_CPU_FLAG_ARMV5TE      (1 << 0)
#define AV_CPU_FLAG_ARMV6        (1 << 1)
#define AV_CPU_FLAG_ARMV6T2      (1 << 2)
#define AV_CPU_FLAG_VFP          (1 << 3)
#define AV_CPU_FLAG_VFPV3        (1 << 4)
#define AV_CPU_FLAG_NEON         (1 << 5)
#define AV_CPU_FLAG_ARMV8        (1 << 6)
#define AV_CPU_FLAG_VFP_VM       (1 << 7) ///< VFPv2 vector mode, deprecated in ARMv7-A and unavailable in various CPUs implementations

/**
 * Return the flags which specify extensions supported by the CPU.
 */
int av_get_cpu_flags(void);

/**
 * Set a mask on flags returned by av_get_cpu_flags().
 * This function is mainly useful for testing.
 *
 * @warning this function is not thread safe.
 */
void av_set_cpu_flags_mask(int mask);

/**
 * Parse CPU flags from a string.
 *
 * @return a combination of AV_CPU_* flags, negative on error.
 */
int av_parse_cpu_flags(const char *s);

/**
 * @return the number of logical CPU cores present.
 */
int av_cpu_count(void);

#endif /* AVUTIL_CPU_H */
