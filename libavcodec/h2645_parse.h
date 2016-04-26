/*
 * H.264/HEVC common parsing code
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

#ifndef AVCODEC_H2645_PARSE_H
#define AVCODEC_H2645_PARSE_H

#include <stdint.h>

#include "avcodec.h"
#include "get_bits.h"

typedef struct H2645NAL {
    uint8_t *rbsp_buffer;
    int rbsp_buffer_size;

    int size;
    const uint8_t *data;

    int raw_size;
    const uint8_t *raw_data;

    GetBitContext gb;

    int type;
    int temporal_id;

    int skipped_bytes;
    int skipped_bytes_pos_size;
    int *skipped_bytes_pos;
} H2645NAL;

/* an input packet split into unescaped NAL units */
typedef struct H2645Packet {
    H2645NAL *nals;
    int nb_nals;
    int nals_allocated;
} H2645Packet;

/**
 * Extract the raw (unescaped) bitstream.
 */
int ff_h2645_extract_rbsp(const uint8_t *src, int length,
                          H2645NAL *nal);

/**
 * Split an input packet into NAL units.
 */
int ff_h2645_packet_split(H2645Packet *pkt, const uint8_t *buf, int length,
                          AVCodecContext *avctx, int is_nalff, int nal_length_size);

#endif /* AVCODEC_H2645_PARSE_H */
