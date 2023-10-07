/*
 * Mpeg video formats-related defines and utility functions
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

#ifndef AVCODEC_MPEGPICTURE_H
#define AVCODEC_MPEGPICTURE_H

#include <stddef.h>
#include <stdint.h>

#include "avcodec.h"
#include "motion_est.h"
#include "threadframe.h"

#define MPV_MAX_PLANES 3
#define MAX_PICTURE_COUNT 36
#define EDGE_WIDTH 16

typedef struct ScratchpadContext {
    uint8_t *edge_emu_buffer;     ///< temporary buffer for if MVs point to out-of-frame data
    uint8_t *rd_scratchpad;       ///< scratchpad for rate distortion mb decision
    uint8_t *obmc_scratchpad;
    uint8_t *b_scratchpad;        ///< scratchpad used for writing into write only buffers
    int      linesize;            ///< linesize that the buffers in this context have been allocated for
} ScratchpadContext;

typedef struct BufferPoolContext {
    struct FFRefStructPool *mbskip_table_pool;
    struct FFRefStructPool *qscale_table_pool;
    struct FFRefStructPool *mb_type_pool;
    struct FFRefStructPool *motion_val_pool;
    struct FFRefStructPool *ref_index_pool;
    int alloc_mb_width;                         ///< mb_width  used to allocate tables
    int alloc_mb_height;                        ///< mb_height used to allocate tables
    int alloc_mb_stride;                        ///< mb_stride used to allocate tables
} BufferPoolContext;

/**
 * MPVPicture.
 */
typedef struct MPVPicture {
    struct AVFrame *f;
    ThreadFrame tf;

    uint8_t  *data[MPV_MAX_PLANES];
    ptrdiff_t linesize[MPV_MAX_PLANES];

    int8_t *qscale_table_base;
    int8_t *qscale_table;

    int16_t (*motion_val_base[2])[2];
    int16_t (*motion_val[2])[2];

    uint32_t *mb_type_base;
    uint32_t *mb_type;          ///< types and macros are defined in mpegutils.h

    uint8_t *mbskip_table;

    int8_t *ref_index[2];

    /// RefStruct reference for hardware accelerator private data
    void *hwaccel_picture_private;

    int mb_width;               ///< mb_width  of the tables
    int mb_height;              ///< mb_height of the tables
    int mb_stride;              ///< mb_stride of the tables

    int dummy;                  ///< Picture is a dummy and should not be output
    int field_picture;          ///< whether or not the picture was encoded in separate fields

    int b_frame_score;

    int reference;
    int shared;

    int display_picture_number;
    int coded_picture_number;
} MPVPicture;

/**
 * Allocate an MPVPicture's accessories, but not the AVFrame's buffer itself.
 */
int ff_mpv_alloc_pic_accessories(AVCodecContext *avctx, MPVPicture *pic,
                                 MotionEstContext *me, ScratchpadContext *sc,
                                 BufferPoolContext *pools, int mb_height);

/**
 * Check that the linesizes of an AVFrame are consistent with the requirements
 * of mpegvideo.
 * FIXME: There should be no need for this function. mpegvideo should be made
 *        to work with changing linesizes.
 */
int ff_mpv_pic_check_linesize(void *logctx, const struct AVFrame *f,
                              ptrdiff_t *linesizep, ptrdiff_t *uvlinesizep);

int ff_mpeg_framesize_alloc(AVCodecContext *avctx, MotionEstContext *me,
                            ScratchpadContext *sc, int linesize);

int ff_mpeg_ref_picture(MPVPicture *dst, MPVPicture *src);
void ff_mpeg_unref_picture(MPVPicture *picture);

void ff_mpv_picture_free(MPVPicture *pic);

int ff_find_unused_picture(AVCodecContext *avctx, MPVPicture *picture, int shared);

#endif /* AVCODEC_MPEGPICTURE_H */
