/*
 * HEVC Supplementary Enhancement Information messages
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

#ifndef AVCODEC_HEVC_SEI_H
#define AVCODEC_HEVC_SEI_H

#include <stdint.h>

#include "libavutil/md5.h"

#include "get_bits.h"

/**
 * SEI message types
 */
typedef enum {
    HEVC_SEI_TYPE_BUFFERING_PERIOD                     = 0,
    HEVC_SEI_TYPE_PICTURE_TIMING                       = 1,
    HEVC_SEI_TYPE_PAN_SCAN_RECT                        = 2,
    HEVC_SEI_TYPE_FILLER_PAYLOAD                       = 3,
    HEVC_SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35       = 4,
    HEVC_SEI_TYPE_USER_DATA_UNREGISTERED               = 5,
    HEVC_SEI_TYPE_RECOVERY_POINT                       = 6,
    HEVC_SEI_TYPE_SCENE_INFO                           = 9,
    HEVC_SEI_TYPE_FULL_FRAME_SNAPSHOT                  = 15,
    HEVC_SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
    HEVC_SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_END   = 17,
    HEVC_SEI_TYPE_FILM_GRAIN_CHARACTERISTICS           = 19,
    HEVC_SEI_TYPE_POST_FILTER_HINT                     = 22,
    HEVC_SEI_TYPE_TONE_MAPPING_INFO                    = 23,
    HEVC_SEI_TYPE_FRAME_PACKING                        = 45,
    HEVC_SEI_TYPE_DISPLAY_ORIENTATION                  = 47,
    HEVC_SEI_TYPE_SOP_DESCRIPTION                      = 128,
    HEVC_SEI_TYPE_ACTIVE_PARAMETER_SETS                = 129,
    HEVC_SEI_TYPE_DECODING_UNIT_INFO                   = 130,
    HEVC_SEI_TYPE_TEMPORAL_LEVEL0_INDEX                = 131,
    HEVC_SEI_TYPE_DECODED_PICTURE_HASH                 = 132,
    HEVC_SEI_TYPE_SCALABLE_NESTING                     = 133,
    HEVC_SEI_TYPE_REGION_REFRESH_INFO                  = 134,
    HEVC_SEI_TYPE_MASTERING_DISPLAY_INFO               = 137,
    HEVC_SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO             = 144,
} HEVC_SEI_Type;

typedef struct HEVCSEIPictureHash {
    struct AVMD5 *md5_ctx;
    uint8_t       md5[3][16];
    uint8_t is_md5;
} HEVCSEIPictureHash;

typedef struct HEVCSEIFramePacking {
    int present;
    int arrangement_type;
    int content_interpretation_type;
    int quincunx_subsampling;
} HEVCSEIFramePacking;

typedef struct HEVCSEIDisplayOrientation {
    int present;
    int anticlockwise_rotation;
    int hflip, vflip;
} HEVCSEIDisplayOrientation;

typedef struct HEVCSEI {
    HEVCSEIPictureHash picture_hash;
    HEVCSEIFramePacking frame_packing;
    HEVCSEIDisplayOrientation display_orientation;
} HEVCSEI;

int ff_hevc_decode_nal_sei(GetBitContext *gb, void *logctx, HEVCSEI *s,
                           int type);

#endif /* AVCODEC_HEVC_SEI_H */
