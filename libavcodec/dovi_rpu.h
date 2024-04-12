/*
 * Dolby Vision RPU decoder
 *
 * Copyright (C) 2021 Jan Ekström
 * Copyright (C) 2021 Niklas Haas
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

#ifndef AVCODEC_DOVI_RPU_H
#define AVCODEC_DOVI_RPU_H

#include "libavutil/dovi_meta.h"
#include "libavutil/frame.h"

#define DOVI_MAX_DM_ID 15
typedef struct DOVIContext {
    void *logctx;

    /**
     * Currently active dolby vision configuration, or {0} for none.
     * Set by the user when decoding.
     *
     * Note: sizeof(cfg) is not part of the libavutil ABI, so users should
     * never pass &cfg to any other library calls. This is included merely as
     * a way to look up the values of fields known at compile time.
     */
    AVDOVIDecoderConfigurationRecord cfg;

    /**
     * Currently active RPU data header, updates on every dovi_rpu_parse().
     */
    AVDOVIRpuDataHeader header;

    /**
     * Currently active data mappings, or NULL. Points into memory owned by the
     * corresponding rpu/vdr_ref, which becomes invalid on the next call to
     * dovi_rpu_parse.
     */
    const AVDOVIDataMapping *mapping;
    const AVDOVIColorMetadata *color;

    /**
     * Currently active extension blocks, updates on every ff_dovi_rpu_parse()
     */
    AVDOVIDmData *ext_blocks;
    int num_ext_blocks;

    /**
     * Private fields internal to dovi_rpu.c
     */
    struct DOVIVdr *vdr[DOVI_MAX_DM_ID+1]; ///< RefStruct references
    uint8_t *rpu_buf; ///< temporary buffer
    unsigned rpu_buf_sz;

} DOVIContext;

void ff_dovi_ctx_replace(DOVIContext *s, const DOVIContext *s0);

/**
 * Completely reset a DOVIContext, preserving only logctx.
 */
void ff_dovi_ctx_unref(DOVIContext *s);

/**
 * Partially reset the internal state. Resets per-frame state, but preserves
 * the stream-wide configuration record.
 */
void ff_dovi_ctx_flush(DOVIContext *s);

/**
 * Parse the contents of a Dovi RPU NAL and update the parsed values in the
 * DOVIContext struct.
 *
 * Returns 0 or an error code.
 *
 * Note: `DOVIContext.cfg` should be initialized before calling into this
 * function. If not done, the profile will be guessed according to HEVC
 * semantics.
 */
int ff_dovi_rpu_parse(DOVIContext *s, const uint8_t *rpu, size_t rpu_size,
                      int err_recognition);

/**
 * Attach the decoded AVDOVIMetadata as side data to an AVFrame.
 */
int ff_dovi_attach_side_data(DOVIContext *s, AVFrame *frame);

/***************************************************
 * The following section is for internal use only. *
 ***************************************************/

typedef struct DOVIVdr {
    AVDOVIDataMapping mapping;
    AVDOVIColorMetadata color;
} DOVIVdr;

enum {
    RPU_COEFF_FIXED = 0,
    RPU_COEFF_FLOAT = 1,
};

/**
 * Internal helper function to guess the correct DV profile for HEVC.
 *
 * Returns the profile number or 0 if unknown.
 */
int ff_dovi_guess_profile_hevc(const AVDOVIRpuDataHeader *hdr);

#endif /* AVCODEC_DOVI_RPU_H */
