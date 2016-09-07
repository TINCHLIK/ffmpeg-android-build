/*
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

#ifndef AVCODEC_X86_IDCTDSP_H
#define AVCODEC_X86_IDCTDSP_H

#include <stddef.h>
#include <stdint.h>

void ff_add_pixels_clamped_mmx(const int16_t *block, uint8_t *pixels,
                               ptrdiff_t line_size);
void ff_put_pixels_clamped_mmx(const int16_t *block, uint8_t *pixels,
                               ptrdiff_t line_size);
void ff_put_signed_pixels_clamped_mmx(const int16_t *block, uint8_t *pixels,
                                      ptrdiff_t line_size);

#endif /* AVCODEC_X86_IDCTDSP_H */
