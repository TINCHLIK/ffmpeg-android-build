/*
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

#ifndef FFTOOLS_FFMPEG_UTILS_H
#define FFTOOLS_FFMPEG_UTILS_H

#include <stdint.h>

#include "libavutil/common.h"
#include "libavutil/frame.h"

#include "libavcodec/packet.h"

/**
 * Merge two return codes - return one of the error codes if at least one of
 * them was negative, 0 otherwise.
 * Currently just picks the first one, eventually we might want to do something
 * more sophisticated, like sorting them by priority.
 */
static inline int err_merge(int err0, int err1)
{
    return (err0 < 0) ? err0 : FFMIN(err1, 0);
}

static inline void pkt_move(void *dst, void *src)
{
    av_packet_move_ref(dst, src);
}

static inline void frame_move(void *dst, void *src)
{
    av_frame_move_ref(dst, src);
}

#endif // FFTOOLS_FFMPEG_UTILS_H
