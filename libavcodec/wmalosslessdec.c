/*
 * Windows Media Audio Lossless decoder
 * Copyright (c) 2007 Baptiste Coudurier, Benjamin Larsson, Ulion
 * Copyright (c) 2008 - 2011 Sascha Sommer, Benjamin Larsson
 * Copyright (c) 2011 Andreas Öman
 * Copyright (c) 2011 - 2012 Mashiat Sarker Shakkhar
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

#include <inttypes.h>

#include "libavutil/attributes.h"
#include "libavutil/avassert.h"

#include "avcodec.h"
#include "bitstream.h"
#include "internal.h"
#include "put_bits.h"
#include "wma.h"
#include "wma_common.h"

/** current decoder limitations */
#define WMALL_MAX_CHANNELS      8                       ///< max number of handled channels
#define MAX_SUBFRAMES          32                       ///< max number of subframes per channel
#define MAX_BANDS              29                       ///< max number of scale factor bands
#define MAX_FRAMESIZE       32768                       ///< maximum compressed frame size
#define MAX_ORDER             256

#define WMALL_BLOCK_MIN_BITS    6                       ///< log2 of min block size
#define WMALL_BLOCK_MAX_BITS   14                       ///< log2 of max block size
#define WMALL_BLOCK_MAX_SIZE (1 << WMALL_BLOCK_MAX_BITS)    ///< maximum block size
#define WMALL_BLOCK_SIZES    (WMALL_BLOCK_MAX_BITS - WMALL_BLOCK_MIN_BITS + 1) ///< possible block sizes


/**
 * @brief frame-specific decoder context for a single channel
 */
typedef struct WmallChannelCtx {
    int16_t     prev_block_len;                         ///< length of the previous block
    uint8_t     transmit_coefs;
    uint8_t     num_subframes;
    uint16_t    subframe_len[MAX_SUBFRAMES];            ///< subframe length in samples
    uint16_t    subframe_offsets[MAX_SUBFRAMES];        ///< subframe positions in the current frame
    uint8_t     cur_subframe;                           ///< current subframe number
    uint16_t    decoded_samples;                        ///< number of already processed samples
    int         quant_step;                             ///< quantization step for the current subframe
    int         transient_counter;                      ///< number of transient samples from the beginning of the transient zone
} WmallChannelCtx;

/**
 * @brief main decoder context
 */
typedef struct WmallDecodeCtx {
    /* generic decoder variables */
    AVCodecContext  *avctx;
    AVFrame         *frame;
    uint8_t         frame_data[MAX_FRAMESIZE + AV_INPUT_BUFFER_PADDING_SIZE];  ///< compressed frame data
    PutBitContext   pb;                             ///< context for filling the frame_data buffer

    /* frame size dependent frame information (set during initialization) */
    uint32_t        decode_flags;                   ///< used compression features
    int             len_prefix;                     ///< frame is prefixed with its length
    int             dynamic_range_compression;      ///< frame contains DRC data
    uint8_t         bits_per_sample;                ///< integer audio sample size for the unscaled IMDCT output (used to scale to [-1.0, 1.0])
    uint16_t        samples_per_frame;              ///< number of samples to output
    uint16_t        log2_frame_size;
    int8_t          num_channels;                   ///< number of channels in the stream (same as AVCodecContext.num_channels)
    int8_t          lfe_channel;                    ///< lfe channel index
    uint8_t         max_num_subframes;
    uint8_t         subframe_len_bits;              ///< number of bits used for the subframe length
    uint8_t         max_subframe_len_bit;           ///< flag indicating that the subframe is of maximum size when the first subframe length bit is 1
    uint16_t        min_samples_per_subframe;

    /* packet decode state */
    BitstreamContext pbc;                           ///< bitstream reader context for the packet
    int             next_packet_start;              ///< start offset of the next WMA packet in the demuxer packet
    uint8_t         packet_offset;                  ///< offset to the frame in the packet
    uint8_t         packet_sequence_number;         ///< current packet number
    int             num_saved_bits;                 ///< saved number of bits
    int             frame_offset;                   ///< frame offset in the bit reservoir
    int             subframe_offset;                ///< subframe offset in the bit reservoir
    uint8_t         packet_loss;                    ///< set in case of bitstream error
    uint8_t         packet_done;                    ///< set when a packet is fully decoded

    /* frame decode state */
    uint32_t        frame_num;                      ///< current frame number (not used for decoding)
    BitstreamContext bc;                            ///< bitstream reader context
    int             buf_bit_size;                   ///< buffer size in bits
    int16_t         *samples_16[WMALL_MAX_CHANNELS]; ///< current sample buffer pointer (16-bit)
    int32_t         *samples_32[WMALL_MAX_CHANNELS]; ///< current sample buffer pointer (24-bit)
    uint8_t         drc_gain;                       ///< gain for the DRC tool
    int8_t          skip_frame;                     ///< skip output step
    int8_t          parsed_all_subframes;           ///< all subframes decoded?

    /* subframe/block decode state */
    int16_t         subframe_len;                   ///< current subframe length
    int8_t          channels_for_cur_subframe;      ///< number of channels that contain the subframe
    int8_t          channel_indexes_for_cur_subframe[WMALL_MAX_CHANNELS];

    WmallChannelCtx channel[WMALL_MAX_CHANNELS];    ///< per channel data

    // WMA Lossless-specific

    uint8_t do_arith_coding;
    uint8_t do_ac_filter;
    uint8_t do_inter_ch_decorr;
    uint8_t do_mclms;
    uint8_t do_lpc;

    int8_t  acfilter_order;
    int8_t  acfilter_scaling;
    int64_t acfilter_coeffs[16];
    int     acfilter_prevvalues[2][16];

    int8_t  mclms_order;
    int8_t  mclms_scaling;
    int16_t mclms_coeffs[WMALL_MAX_CHANNELS * WMALL_MAX_CHANNELS * 32];
    int16_t mclms_coeffs_cur[WMALL_MAX_CHANNELS * WMALL_MAX_CHANNELS];
    int16_t mclms_prevvalues[WMALL_MAX_CHANNELS * 2 * 32];
    int16_t mclms_updates[WMALL_MAX_CHANNELS * 2 * 32];
    int     mclms_recent;

    int     movave_scaling;
    int     quant_stepsize;

    struct {
        int order;
        int scaling;
        int coefsend;
        int bitsend;
        int16_t coefs[MAX_ORDER];
        int16_t lms_prevvalues[MAX_ORDER * 2];
        int16_t lms_updates[MAX_ORDER * 2];
        int recent;
    } cdlms[2][9];

    int cdlms_ttl[2];

    int bV3RTM;

    int is_channel_coded[2];
    int update_speed[2];

    int transient[2];
    int transient_pos[2];
    int seekable_tile;

    int ave_sum[2];

    int channel_residues[2][WMALL_BLOCK_MAX_SIZE];

    int lpc_coefs[2][40];
    int lpc_order;
    int lpc_scaling;
    int lpc_intbits;

    int channel_coeffs[2][WMALL_BLOCK_MAX_SIZE];
} WmallDecodeCtx;


static av_cold int decode_init(AVCodecContext *avctx)
{
    WmallDecodeCtx *s  = avctx->priv_data;
    uint8_t *edata_ptr = avctx->extradata;
    unsigned int channel_mask;
    int i, log2_max_num_subframes;

    s->avctx = avctx;
    init_put_bits(&s->pb, s->frame_data, MAX_FRAMESIZE);

    if (avctx->extradata_size >= 18) {
        s->decode_flags    = AV_RL16(edata_ptr + 14);
        channel_mask       = AV_RL32(edata_ptr +  2);
        s->bits_per_sample = AV_RL16(edata_ptr);
        if (s->bits_per_sample == 16)
            avctx->sample_fmt = AV_SAMPLE_FMT_S16P;
        else if (s->bits_per_sample == 24) {
            avctx->sample_fmt = AV_SAMPLE_FMT_S32P;
            avpriv_report_missing_feature(avctx, "Bit-depth higher than 16");
            return AVERROR_PATCHWELCOME;
        } else {
            av_log(avctx, AV_LOG_ERROR, "Unknown bit-depth: %"PRIu8"\n",
                   s->bits_per_sample);
            return AVERROR_INVALIDDATA;
        }
        /* dump the extradata */
        for (i = 0; i < avctx->extradata_size; i++)
            ff_dlog(avctx, "[%x] ", avctx->extradata[i]);
        ff_dlog(avctx, "\n");

    } else {
        avpriv_request_sample(avctx, "Unsupported extradata size");
        return AVERROR_PATCHWELCOME;
    }

    /* generic init */
    s->log2_frame_size = av_log2(avctx->block_align) + 4;

    /* frame info */
    s->skip_frame  = 1; /* skip first frame */
    s->packet_loss = 1;
    s->len_prefix  = s->decode_flags & 0x40;

    /* get frame len */
    s->samples_per_frame = 1 << ff_wma_get_frame_len_bits(avctx->sample_rate,
                                                          3, s->decode_flags);
    av_assert0(s->samples_per_frame <= WMALL_BLOCK_MAX_SIZE);

    /* init previous block len */
    for (i = 0; i < avctx->channels; i++)
        s->channel[i].prev_block_len = s->samples_per_frame;

    /* subframe info */
    log2_max_num_subframes  = (s->decode_flags & 0x38) >> 3;
    s->max_num_subframes    = 1 << log2_max_num_subframes;
    s->max_subframe_len_bit = 0;
    s->subframe_len_bits    = av_log2(log2_max_num_subframes) + 1;

    s->min_samples_per_subframe  = s->samples_per_frame / s->max_num_subframes;
    s->dynamic_range_compression = s->decode_flags & 0x80;
    s->bV3RTM                    = s->decode_flags & 0x100;

    if (s->max_num_subframes > MAX_SUBFRAMES) {
        av_log(avctx, AV_LOG_ERROR, "invalid number of subframes %"PRIu8"\n",
               s->max_num_subframes);
        return AVERROR_INVALIDDATA;
    }

    s->num_channels = avctx->channels;

    /* extract lfe channel position */
    s->lfe_channel = -1;

    if (channel_mask & 8) {
        unsigned int mask;
        for (mask = 1; mask < 16; mask <<= 1)
            if (channel_mask & mask)
                ++s->lfe_channel;
    }

    if (s->num_channels < 0) {
        av_log(avctx, AV_LOG_ERROR, "invalid number of channels %"PRId8"\n",
               s->num_channels);
        return AVERROR_INVALIDDATA;
    } else if (s->num_channels > WMALL_MAX_CHANNELS) {
        avpriv_request_sample(avctx,
                              "More than %d channels", WMALL_MAX_CHANNELS);
        return AVERROR_PATCHWELCOME;
    }

    s->frame = av_frame_alloc();
    if (!s->frame)
        return AVERROR(ENOMEM);

    avctx->channel_layout = channel_mask;
    return 0;
}

/**
 * @brief Decode the subframe length.
 * @param s      context
 * @param offset sample offset in the frame
 * @return decoded subframe length on success, < 0 in case of an error
 */
static int decode_subframe_length(WmallDecodeCtx *s, int offset)
{
    int frame_len_ratio, subframe_len, len;

    /* no need to read from the bitstream when only one length is possible */
    if (offset == s->samples_per_frame - s->min_samples_per_subframe)
        return s->min_samples_per_subframe;

    len             = av_log2(s->max_num_subframes - 1) + 1;
    frame_len_ratio = bitstream_read(&s->bc, len);
    subframe_len    = s->min_samples_per_subframe * (frame_len_ratio + 1);

    /* sanity check the length */
    if (subframe_len < s->min_samples_per_subframe ||
        subframe_len > s->samples_per_frame) {
        av_log(s->avctx, AV_LOG_ERROR, "broken frame: subframe_len %i\n",
               subframe_len);
        return AVERROR_INVALIDDATA;
    }
    return subframe_len;
}

/**
 * @brief Decode how the data in the frame is split into subframes.
 *       Every WMA frame contains the encoded data for a fixed number of
 *       samples per channel. The data for every channel might be split
 *       into several subframes. This function will reconstruct the list of
 *       subframes for every channel.
 *
 *       If the subframes are not evenly split, the algorithm estimates the
 *       channels with the lowest number of total samples.
 *       Afterwards, for each of these channels a bit is read from the
 *       bitstream that indicates if the channel contains a subframe with the
 *       next subframe size that is going to be read from the bitstream or not.
 *       If a channel contains such a subframe, the subframe size gets added to
 *       the channel's subframe list.
 *       The algorithm repeats these steps until the frame is properly divided
 *       between the individual channels.
 *
 * @param s context
 * @return 0 on success, < 0 in case of an error
 */
static int decode_tilehdr(WmallDecodeCtx *s)
{
    uint16_t num_samples[WMALL_MAX_CHANNELS] = { 0 }; /* sum of samples for all currently known subframes of a channel */
    uint8_t  contains_subframe[WMALL_MAX_CHANNELS];   /* flag indicating if a channel contains the current subframe */
    int channels_for_cur_subframe = s->num_channels;  /* number of channels that contain the current subframe */
    int fixed_channel_layout = 0;                     /* flag indicating that all channels use the same subfra2me offsets and sizes */
    int min_channel_len = 0;                          /* smallest sum of samples (channels with this length will be processed first) */
    int c, tile_aligned;

    /* reset tiling information */
    for (c = 0; c < s->num_channels; c++)
        s->channel[c].num_subframes = 0;

    tile_aligned = bitstream_read_bit(&s->bc);
    if (s->max_num_subframes == 1 || tile_aligned)
        fixed_channel_layout = 1;

    /* loop until the frame data is split between the subframes */
    do {
        int subframe_len, in_use = 0;

        /* check which channels contain the subframe */
        for (c = 0; c < s->num_channels; c++) {
            if (num_samples[c] == min_channel_len) {
                if (fixed_channel_layout || channels_for_cur_subframe == 1 ||
                   (min_channel_len == s->samples_per_frame - s->min_samples_per_subframe)) {
                    contains_subframe[c] = in_use = 1;
                } else {
                    if (bitstream_read_bit(&s->bc))
                        contains_subframe[c] = in_use = 1;
                }
            } else
                contains_subframe[c] = 0;
        }

        if (!in_use) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "Found empty subframe\n");
            return AVERROR_INVALIDDATA;
        }

        /* get subframe length, subframe_len == 0 is not allowed */
        if ((subframe_len = decode_subframe_length(s, min_channel_len)) <= 0)
            return AVERROR_INVALIDDATA;
        /* add subframes to the individual channels and find new min_channel_len */
        min_channel_len += subframe_len;
        for (c = 0; c < s->num_channels; c++) {
            WmallChannelCtx *chan = &s->channel[c];

            if (contains_subframe[c]) {
                if (chan->num_subframes >= MAX_SUBFRAMES) {
                    av_log(s->avctx, AV_LOG_ERROR,
                           "broken frame: num subframes > 31\n");
                    return AVERROR_INVALIDDATA;
                }
                chan->subframe_len[chan->num_subframes] = subframe_len;
                num_samples[c] += subframe_len;
                ++chan->num_subframes;
                if (num_samples[c] > s->samples_per_frame) {
                    av_log(s->avctx, AV_LOG_ERROR, "broken frame: "
                           "channel len(%"PRIu16") > samples_per_frame(%"PRIu16")\n",
                           num_samples[c], s->samples_per_frame);
                    return AVERROR_INVALIDDATA;
                }
            } else if (num_samples[c] <= min_channel_len) {
                if (num_samples[c] < min_channel_len) {
                    channels_for_cur_subframe = 0;
                    min_channel_len = num_samples[c];
                }
                ++channels_for_cur_subframe;
            }
        }
    } while (min_channel_len < s->samples_per_frame);

    for (c = 0; c < s->num_channels; c++) {
        int i, offset = 0;
        for (i = 0; i < s->channel[c].num_subframes; i++) {
            s->channel[c].subframe_offsets[i] = offset;
            offset += s->channel[c].subframe_len[i];
        }
    }

    return 0;
}

static void decode_ac_filter(WmallDecodeCtx *s)
{
    int i;
    s->acfilter_order   = bitstream_read(&s->bc, 4) + 1;
    s->acfilter_scaling = bitstream_read(&s->bc, 4);

    for (i = 0; i < s->acfilter_order; i++)
        s->acfilter_coeffs[i] = bitstream_read(&s->bc, s->acfilter_scaling) + 1;
}

static void decode_mclms(WmallDecodeCtx *s)
{
    s->mclms_order   = (bitstream_read(&s->bc, 4) + 1) * 2;
    s->mclms_scaling =  bitstream_read(&s->bc, 4);
    if (bitstream_read_bit(&s->bc)) {
        int i, send_coef_bits;
        int cbits = av_log2(s->mclms_scaling + 1);
        if (1 << cbits < s->mclms_scaling + 1)
            cbits++;

        send_coef_bits = bitstream_read(&s->bc, cbits) + 2;

        for (i = 0; i < s->mclms_order * s->num_channels * s->num_channels; i++)
            s->mclms_coeffs[i] = bitstream_read(&s->bc, send_coef_bits);

        for (i = 0; i < s->num_channels; i++) {
            int c;
            for (c = 0; c < i; c++)
                s->mclms_coeffs_cur[i * s->num_channels + c] = bitstream_read(&s->bc, send_coef_bits);
        }
    }
}

static int decode_cdlms(WmallDecodeCtx *s)
{
    int c, i;
    int cdlms_send_coef = bitstream_read_bit(&s->bc);

    for (c = 0; c < s->num_channels; c++) {
        s->cdlms_ttl[c] = bitstream_read(&s->bc, 3) + 1;
        for (i = 0; i < s->cdlms_ttl[c]; i++) {
            s->cdlms[c][i].order = (bitstream_read(&s->bc, 7) + 1) * 8;
            if (s->cdlms[c][i].order > MAX_ORDER) {
                av_log(s->avctx, AV_LOG_ERROR,
                       "Order[%d][%d] %d > max (%d), not supported\n",
                       c, i, s->cdlms[c][i].order, MAX_ORDER);
                s->cdlms[0][0].order = 0;
                return AVERROR_INVALIDDATA;
            }
        }

        for (i = 0; i < s->cdlms_ttl[c]; i++)
            s->cdlms[c][i].scaling = bitstream_read(&s->bc, 4);

        if (cdlms_send_coef) {
            for (i = 0; i < s->cdlms_ttl[c]; i++) {
                int cbits, shift_l, shift_r, j;
                cbits = av_log2(s->cdlms[c][i].order);
                if ((1 << cbits) < s->cdlms[c][i].order)
                    cbits++;
                s->cdlms[c][i].coefsend = bitstream_read(&s->bc, cbits) + 1;

                cbits = av_log2(s->cdlms[c][i].scaling + 1);
                if ((1 << cbits) < s->cdlms[c][i].scaling + 1)
                    cbits++;

                s->cdlms[c][i].bitsend = bitstream_read(&s->bc, cbits) + 2;
                shift_l = 32 - s->cdlms[c][i].bitsend;
                shift_r = 32 - s->cdlms[c][i].scaling - 2;
                for (j = 0; j < s->cdlms[c][i].coefsend; j++)
                    s->cdlms[c][i].coefs[j] =
                        (bitstream_read(&s->bc, s->cdlms[c][i].bitsend) << shift_l) >> shift_r;
            }
        }
    }

    return 0;
}

static int decode_channel_residues(WmallDecodeCtx *s, int ch, int tile_size)
{
    int i = 0;
    unsigned int ave_mean;
    s->transient[ch] = bitstream_read_bit(&s->bc);
    if (s->transient[ch]) {
        s->transient_pos[ch] = bitstream_read(&s->bc, av_log2(tile_size));
        if (s->transient_pos[ch])
            s->transient[ch] = 0;
        s->channel[ch].transient_counter =
            FFMAX(s->channel[ch].transient_counter, s->samples_per_frame / 2);
    } else if (s->channel[ch].transient_counter)
        s->transient[ch] = 1;

    if (s->seekable_tile) {
        ave_mean = bitstream_read(&s->bc, s->bits_per_sample);
        s->ave_sum[ch] = ave_mean << (s->movave_scaling + 1);
    }

    if (s->seekable_tile) {
        if (s->do_inter_ch_decorr)
            s->channel_residues[ch][0] = bitstream_read_signed(&s->bc, s->bits_per_sample + 1);
        else
            s->channel_residues[ch][0] = bitstream_read_signed(&s->bc, s->bits_per_sample);
        i++;
    }
    for (; i < tile_size; i++) {
        int quo = 0, rem, rem_bits, residue;
        while (bitstream_read_bit(&s->bc)) {
            quo++;
            if (bitstream_bits_left(&s->bc) <= 0)
                return -1;
        }
        if (quo >= 32)
            quo += bitstream_read(&s->bc, bitstream_read(&s->bc, 5) + 1);

        ave_mean = (s->ave_sum[ch] + (1 << s->movave_scaling)) >> (s->movave_scaling + 1);
        if (ave_mean <= 1)
            residue = quo;
        else {
            rem_bits = av_ceil_log2(ave_mean);
            rem      = rem_bits ? bitstream_read(&s->bc, rem_bits) : 0;
            residue  = (quo << rem_bits) + rem;
        }

        s->ave_sum[ch] = residue + s->ave_sum[ch] -
                         (s->ave_sum[ch] >> s->movave_scaling);

        if (residue & 1)
            residue = -(residue >> 1) - 1;
        else
            residue = residue >> 1;
        s->channel_residues[ch][i] = residue;
    }

    return 0;

}

static void decode_lpc(WmallDecodeCtx *s)
{
    int ch, i, cbits;
    s->lpc_order   = bitstream_read(&s->bc, 5) + 1;
    s->lpc_scaling = bitstream_read(&s->bc, 4);
    s->lpc_intbits = bitstream_read(&s->bc, 3) + 1;
    cbits = s->lpc_scaling + s->lpc_intbits;
    for (ch = 0; ch < s->num_channels; ch++)
        for (i = 0; i < s->lpc_order; i++)
            s->lpc_coefs[ch][i] = bitstream_read_signed(&s->bc, cbits);
}

static void clear_codec_buffers(WmallDecodeCtx *s)
{
    int ich, ilms;

    memset(s->acfilter_coeffs,     0, sizeof(s->acfilter_coeffs));
    memset(s->acfilter_prevvalues, 0, sizeof(s->acfilter_prevvalues));
    memset(s->lpc_coefs,           0, sizeof(s->lpc_coefs));

    memset(s->mclms_coeffs,     0, sizeof(s->mclms_coeffs));
    memset(s->mclms_coeffs_cur, 0, sizeof(s->mclms_coeffs_cur));
    memset(s->mclms_prevvalues, 0, sizeof(s->mclms_prevvalues));
    memset(s->mclms_updates,    0, sizeof(s->mclms_updates));

    for (ich = 0; ich < s->num_channels; ich++) {
        for (ilms = 0; ilms < s->cdlms_ttl[ich]; ilms++) {
            memset(s->cdlms[ich][ilms].coefs, 0,
                   sizeof(s->cdlms[ich][ilms].coefs));
            memset(s->cdlms[ich][ilms].lms_prevvalues, 0,
                   sizeof(s->cdlms[ich][ilms].lms_prevvalues));
            memset(s->cdlms[ich][ilms].lms_updates, 0,
                   sizeof(s->cdlms[ich][ilms].lms_updates));
        }
        s->ave_sum[ich] = 0;
    }
}

/**
 * @brief Reset filter parameters and transient area at new seekable tile.
 */
static void reset_codec(WmallDecodeCtx *s)
{
    int ich, ilms;
    s->mclms_recent = s->mclms_order * s->num_channels;
    for (ich = 0; ich < s->num_channels; ich++) {
        for (ilms = 0; ilms < s->cdlms_ttl[ich]; ilms++)
            s->cdlms[ich][ilms].recent = s->cdlms[ich][ilms].order;
        /* first sample of a seekable subframe is considered as the starting of
            a transient area which is samples_per_frame samples long */
        s->channel[ich].transient_counter = s->samples_per_frame;
        s->transient[ich]     = 1;
        s->transient_pos[ich] = 0;
    }
}

static void mclms_update(WmallDecodeCtx *s, int icoef, int *pred)
{
    int i, j, ich, pred_error;
    int order        = s->mclms_order;
    int num_channels = s->num_channels;
    int range        = 1 << (s->bits_per_sample - 1);

    for (ich = 0; ich < num_channels; ich++) {
        pred_error = s->channel_residues[ich][icoef] - pred[ich];
        if (pred_error > 0) {
            for (i = 0; i < order * num_channels; i++)
                s->mclms_coeffs[i + ich * order * num_channels] +=
                    s->mclms_updates[s->mclms_recent + i];
            for (j = 0; j < ich; j++) {
                if (s->channel_residues[j][icoef] > 0)
                    s->mclms_coeffs_cur[ich * num_channels + j] += 1;
                else if (s->channel_residues[j][icoef] < 0)
                    s->mclms_coeffs_cur[ich * num_channels + j] -= 1;
            }
        } else if (pred_error < 0) {
            for (i = 0; i < order * num_channels; i++)
                s->mclms_coeffs[i + ich * order * num_channels] -=
                    s->mclms_updates[s->mclms_recent + i];
            for (j = 0; j < ich; j++) {
                if (s->channel_residues[j][icoef] > 0)
                    s->mclms_coeffs_cur[ich * num_channels + j] -= 1;
                else if (s->channel_residues[j][icoef] < 0)
                    s->mclms_coeffs_cur[ich * num_channels + j] += 1;
            }
        }
    }

    for (ich = num_channels - 1; ich >= 0; ich--) {
        s->mclms_recent--;
        s->mclms_prevvalues[s->mclms_recent] = s->channel_residues[ich][icoef];
        if (s->channel_residues[ich][icoef] > range - 1)
            s->mclms_prevvalues[s->mclms_recent] = range - 1;
        else if (s->channel_residues[ich][icoef] < -range)
            s->mclms_prevvalues[s->mclms_recent] = -range;

        s->mclms_updates[s->mclms_recent] = 0;
        if (s->channel_residues[ich][icoef] > 0)
            s->mclms_updates[s->mclms_recent] = 1;
        else if (s->channel_residues[ich][icoef] < 0)
            s->mclms_updates[s->mclms_recent] = -1;
    }

    if (s->mclms_recent == 0) {
        memcpy(&s->mclms_prevvalues[order * num_channels],
               s->mclms_prevvalues,
               2 * order * num_channels);
        memcpy(&s->mclms_updates[order * num_channels],
               s->mclms_updates,
               2 * order * num_channels);
        s->mclms_recent = num_channels * order;
    }
}

static void mclms_predict(WmallDecodeCtx *s, int icoef, int *pred)
{
    int ich, i;
    int order        = s->mclms_order;
    int num_channels = s->num_channels;

    for (ich = 0; ich < num_channels; ich++) {
        pred[ich] = 0;
        if (!s->is_channel_coded[ich])
            continue;
        for (i = 0; i < order * num_channels; i++)
            pred[ich] += s->mclms_prevvalues[i + s->mclms_recent] *
                         s->mclms_coeffs[i + order * num_channels * ich];
        for (i = 0; i < ich; i++)
            pred[ich] += s->channel_residues[i][icoef] *
                         s->mclms_coeffs_cur[i + num_channels * ich];
        pred[ich] += 1 << s->mclms_scaling - 1;
        pred[ich] >>= s->mclms_scaling;
        s->channel_residues[ich][icoef] += pred[ich];
    }
}

static void revert_mclms(WmallDecodeCtx *s, int tile_size)
{
    int icoef, pred[WMALL_MAX_CHANNELS] = { 0 };
    for (icoef = 0; icoef < tile_size; icoef++) {
        mclms_predict(s, icoef, pred);
        mclms_update(s, icoef, pred);
    }
}

static int lms_predict(WmallDecodeCtx *s, int ich, int ilms)
{
    int pred = 0, icoef;
    int recent = s->cdlms[ich][ilms].recent;

    for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
        pred += s->cdlms[ich][ilms].coefs[icoef] *
                s->cdlms[ich][ilms].lms_prevvalues[icoef + recent];

    return pred;
}

static void lms_update(WmallDecodeCtx *s, int ich, int ilms,
                       int input, int residue)
{
    int icoef;
    int recent = s->cdlms[ich][ilms].recent;
    int range  = 1 << s->bits_per_sample - 1;

    if (residue < 0) {
        for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
            s->cdlms[ich][ilms].coefs[icoef] -=
                s->cdlms[ich][ilms].lms_updates[icoef + recent];
    } else if (residue > 0) {
        for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
            s->cdlms[ich][ilms].coefs[icoef] +=
                s->cdlms[ich][ilms].lms_updates[icoef + recent];
    }

    if (recent)
        recent--;
    else {
        memcpy(&s->cdlms[ich][ilms].lms_prevvalues[s->cdlms[ich][ilms].order],
               s->cdlms[ich][ilms].lms_prevvalues,
               2 * s->cdlms[ich][ilms].order);
        memcpy(&s->cdlms[ich][ilms].lms_updates[s->cdlms[ich][ilms].order],
               s->cdlms[ich][ilms].lms_updates,
               2 * s->cdlms[ich][ilms].order);
        recent = s->cdlms[ich][ilms].order - 1;
    }

    s->cdlms[ich][ilms].lms_prevvalues[recent] = av_clip(input, -range, range - 1);
    if (!input)
        s->cdlms[ich][ilms].lms_updates[recent] = 0;
    else if (input < 0)
        s->cdlms[ich][ilms].lms_updates[recent] = -s->update_speed[ich];
    else
        s->cdlms[ich][ilms].lms_updates[recent] = s->update_speed[ich];

    s->cdlms[ich][ilms].lms_updates[recent + (s->cdlms[ich][ilms].order >> 4)] >>= 2;
    s->cdlms[ich][ilms].lms_updates[recent + (s->cdlms[ich][ilms].order >> 3)] >>= 1;
    s->cdlms[ich][ilms].recent = recent;
}

static void use_high_update_speed(WmallDecodeCtx *s, int ich)
{
    int ilms, recent, icoef;
    for (ilms = s->cdlms_ttl[ich] - 1; ilms >= 0; ilms--) {
        recent = s->cdlms[ich][ilms].recent;
        if (s->update_speed[ich] == 16)
            continue;
        if (s->bV3RTM) {
            for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
                s->cdlms[ich][ilms].lms_updates[icoef + recent] *= 2;
        } else {
            for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
                s->cdlms[ich][ilms].lms_updates[icoef] *= 2;
        }
    }
    s->update_speed[ich] = 16;
}

static void use_normal_update_speed(WmallDecodeCtx *s, int ich)
{
    int ilms, recent, icoef;
    for (ilms = s->cdlms_ttl[ich] - 1; ilms >= 0; ilms--) {
        recent = s->cdlms[ich][ilms].recent;
        if (s->update_speed[ich] == 8)
            continue;
        if (s->bV3RTM)
            for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
                s->cdlms[ich][ilms].lms_updates[icoef + recent] /= 2;
        else
            for (icoef = 0; icoef < s->cdlms[ich][ilms].order; icoef++)
                s->cdlms[ich][ilms].lms_updates[icoef] /= 2;
    }
    s->update_speed[ich] = 8;
}

static void revert_cdlms(WmallDecodeCtx *s, int ch,
                         int coef_begin, int coef_end)
{
    int icoef, pred, ilms, num_lms, residue, input;

    num_lms = s->cdlms_ttl[ch];
    for (ilms = num_lms - 1; ilms >= 0; ilms--) {
        for (icoef = coef_begin; icoef < coef_end; icoef++) {
            pred = 1 << (s->cdlms[ch][ilms].scaling - 1);
            residue = s->channel_residues[ch][icoef];
            pred += lms_predict(s, ch, ilms);
            input = residue + (pred >> s->cdlms[ch][ilms].scaling);
            lms_update(s, ch, ilms, input, residue);
            s->channel_residues[ch][icoef] = input;
        }
    }
}

static void revert_inter_ch_decorr(WmallDecodeCtx *s, int tile_size)
{
    if (s->num_channels != 2)
        return;
    else if (s->is_channel_coded[0] || s->is_channel_coded[1]) {
        int icoef;
        for (icoef = 0; icoef < tile_size; icoef++) {
            s->channel_residues[0][icoef] -= s->channel_residues[1][icoef] >> 1;
            s->channel_residues[1][icoef] += s->channel_residues[0][icoef];
        }
    }
}

static void revert_acfilter(WmallDecodeCtx *s, int tile_size)
{
    int ich, pred, i, j;
    int64_t *filter_coeffs = s->acfilter_coeffs;
    int scaling            = s->acfilter_scaling;
    int order              = s->acfilter_order;

    for (ich = 0; ich < s->num_channels; ich++) {
        int *prevvalues = s->acfilter_prevvalues[ich];
        for (i = 0; i < order; i++) {
            pred = 0;
            for (j = 0; j < order; j++) {
                if (i <= j)
                    pred += filter_coeffs[j] * prevvalues[j - i];
                else
                    pred += s->channel_residues[ich][i - j - 1] * filter_coeffs[j];
            }
            pred >>= scaling;
            s->channel_residues[ich][i] += pred;
        }
        for (i = order; i < tile_size; i++) {
            pred = 0;
            for (j = 0; j < order; j++)
                pred += s->channel_residues[ich][i - j - 1] * filter_coeffs[j];
            pred >>= scaling;
            s->channel_residues[ich][i] += pred;
        }
        for (j = 0; j < order; j++)
            prevvalues[j] = s->channel_residues[ich][tile_size - j - 1];
    }
}

static int decode_subframe(WmallDecodeCtx *s)
{
    int offset        = s->samples_per_frame;
    int subframe_len  = s->samples_per_frame;
    int total_samples = s->samples_per_frame * s->num_channels;
    int i, j, rawpcm_tile, padding_zeroes, res;

    s->subframe_offset = bitstream_tell(&s->bc);

    /* reset channel context and find the next block offset and size
        == the next block of the channel with the smallest number of
        decoded samples */
    for (i = 0; i < s->num_channels; i++) {
        if (offset > s->channel[i].decoded_samples) {
            offset = s->channel[i].decoded_samples;
            subframe_len =
                s->channel[i].subframe_len[s->channel[i].cur_subframe];
        }
    }

    /* get a list of all channels that contain the estimated block */
    s->channels_for_cur_subframe = 0;
    for (i = 0; i < s->num_channels; i++) {
        const int cur_subframe = s->channel[i].cur_subframe;
        /* subtract already processed samples */
        total_samples -= s->channel[i].decoded_samples;

        /* and count if there are multiple subframes that match our profile */
        if (offset == s->channel[i].decoded_samples &&
            subframe_len == s->channel[i].subframe_len[cur_subframe]) {
            total_samples -= s->channel[i].subframe_len[cur_subframe];
            s->channel[i].decoded_samples +=
                s->channel[i].subframe_len[cur_subframe];
            s->channel_indexes_for_cur_subframe[s->channels_for_cur_subframe] = i;
            ++s->channels_for_cur_subframe;
        }
    }

    /* check if the frame will be complete after processing the
        estimated block */
    if (!total_samples)
        s->parsed_all_subframes = 1;


    s->seekable_tile = bitstream_read_bit(&s->bc);
    if (s->seekable_tile) {
        clear_codec_buffers(s);

        s->do_arith_coding    = bitstream_read_bit(&s->bc);
        if (s->do_arith_coding) {
            avpriv_request_sample(s->avctx, "Arithmetic coding");
            return AVERROR_PATCHWELCOME;
        }
        s->do_ac_filter       = bitstream_read_bit(&s->bc);
        s->do_inter_ch_decorr = bitstream_read_bit(&s->bc);
        s->do_mclms           = bitstream_read_bit(&s->bc);

        if (s->do_ac_filter)
            decode_ac_filter(s);

        if (s->do_mclms)
            decode_mclms(s);

        if ((res = decode_cdlms(s)) < 0)
            return res;
        s->movave_scaling = bitstream_read(&s->bc, 3);
        s->quant_stepsize = bitstream_read(&s->bc, 8) + 1;

        reset_codec(s);
    } else if (!s->cdlms[0][0].order) {
        av_log(s->avctx, AV_LOG_DEBUG,
               "Waiting for seekable tile\n");
        av_frame_unref(s->frame);
        return -1;
    }

    rawpcm_tile = bitstream_read_bit(&s->bc);

    for (i = 0; i < s->num_channels; i++)
        s->is_channel_coded[i] = 1;

    if (!rawpcm_tile) {
        for (i = 0; i < s->num_channels; i++)
            s->is_channel_coded[i] = bitstream_read_bit(&s->bc);

        if (s->bV3RTM) {
            // LPC
            s->do_lpc = bitstream_read_bit(&s->bc);
            if (s->do_lpc) {
                decode_lpc(s);
                avpriv_request_sample(s->avctx, "Expect wrong output since "
                                      "inverse LPC filter");
            }
        } else
            s->do_lpc = 0;
    }


    if (bitstream_read_bit(&s->bc))
        padding_zeroes = bitstream_read(&s->bc, 5);
    else
        padding_zeroes = 0;

    if (rawpcm_tile) {
        int bits = s->bits_per_sample - padding_zeroes;
        if (bits <= 0) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "Invalid number of padding bits in raw PCM tile\n");
            return AVERROR_INVALIDDATA;
        }
        ff_dlog(s->avctx, "RAWPCM %d bits per sample. "
                "total %d bits, remain=%d\n", bits,
                bits * s->num_channels * subframe_len, bitstream_tell(&s->bc));
        for (i = 0; i < s->num_channels; i++)
            for (j = 0; j < subframe_len; j++)
                s->channel_coeffs[i][j] = bitstream_read_signed(&s->bc, bits);
    } else {
        for (i = 0; i < s->num_channels; i++)
            if (s->is_channel_coded[i]) {
                decode_channel_residues(s, i, subframe_len);
                if (s->seekable_tile)
                    use_high_update_speed(s, i);
                else
                    use_normal_update_speed(s, i);
                revert_cdlms(s, i, 0, subframe_len);
            } else {
                memset(s->channel_residues[i], 0, sizeof(**s->channel_residues) * subframe_len);
            }
    }
    if (s->do_mclms)
        revert_mclms(s, subframe_len);
    if (s->do_inter_ch_decorr)
        revert_inter_ch_decorr(s, subframe_len);
    if (s->do_ac_filter)
        revert_acfilter(s, subframe_len);

    /* Dequantize */
    if (s->quant_stepsize != 1)
        for (i = 0; i < s->num_channels; i++)
            for (j = 0; j < subframe_len; j++)
                s->channel_residues[i][j] *= s->quant_stepsize;

    /* Write to proper output buffer depending on bit-depth */
    for (i = 0; i < s->channels_for_cur_subframe; i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        int subframe_len = s->channel[c].subframe_len[s->channel[c].cur_subframe];

        for (j = 0; j < subframe_len; j++) {
            if (s->bits_per_sample == 16) {
                *s->samples_16[c]++ = (int16_t) s->channel_residues[c][j] << padding_zeroes;
            } else {
                *s->samples_32[c]++ = s->channel_residues[c][j] << padding_zeroes;
            }
        }
    }

    /* handled one subframe */
    for (i = 0; i < s->channels_for_cur_subframe; i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        if (s->channel[c].cur_subframe >= s->channel[c].num_subframes) {
            av_log(s->avctx, AV_LOG_ERROR, "broken subframe\n");
            return AVERROR_INVALIDDATA;
        }
        ++s->channel[c].cur_subframe;
    }
    return 0;
}

/**
 * @brief Decode one WMA frame.
 * @param s codec context
 * @return 0 if the trailer bit indicates that this is the last frame,
 *         1 if there are additional frames
 */
static int decode_frame(WmallDecodeCtx *s)
{
    BitstreamContext *bc = &s->bc;
    int more_frames = 0, len = 0, i, ret;

    s->frame->nb_samples = s->samples_per_frame;
    if ((ret = ff_get_buffer(s->avctx, s->frame, 0)) < 0) {
        /* return an error if no frame could be decoded at all */
        av_log(s->avctx, AV_LOG_ERROR,
               "not enough space for the output samples\n");
        s->packet_loss = 1;
        return ret;
    }
    for (i = 0; i < s->num_channels; i++) {
        s->samples_16[i] = (int16_t *)s->frame->extended_data[i];
        s->samples_32[i] = (int32_t *)s->frame->extended_data[i];
    }

    /* get frame length */
    if (s->len_prefix)
        len = bitstream_read(bc, s->log2_frame_size);

    /* decode tile information */
    if (decode_tilehdr(s)) {
        s->packet_loss = 1;
        return 0;
    }

    /* read drc info */
    if (s->dynamic_range_compression)
        s->drc_gain = bitstream_read(bc, 8);

    /* no idea what these are for, might be the number of samples
       that need to be skipped at the beginning or end of a stream */
    if (bitstream_read_bit(bc)) {
        int av_unused skip;

        /* usually true for the first frame */
        if (bitstream_read_bit(bc)) {
            skip = bitstream_read(bc, av_log2(s->samples_per_frame * 2));
            ff_dlog(s->avctx, "start skip: %i\n", skip);
        }

        /* sometimes true for the last frame */
        if (bitstream_read_bit(bc)) {
            skip = bitstream_read(bc, av_log2(s->samples_per_frame * 2));
            ff_dlog(s->avctx, "end skip: %i\n", skip);
        }

    }

    /* reset subframe states */
    s->parsed_all_subframes = 0;
    for (i = 0; i < s->num_channels; i++) {
        s->channel[i].decoded_samples = 0;
        s->channel[i].cur_subframe    = 0;
    }

    /* decode all subframes */
    while (!s->parsed_all_subframes) {
        if (decode_subframe(s) < 0) {
            s->packet_loss = 1;
            return 0;
        }
    }

    ff_dlog(s->avctx, "Frame done\n");

    if (s->skip_frame)
        s->skip_frame = 0;

    if (s->len_prefix) {
        if (len != (bitstream_tell(bc) - s->frame_offset) + 2) {
            /* FIXME: not sure if this is always an error */
            av_log(s->avctx, AV_LOG_ERROR,
                   "frame[%"PRIu32"] would have to skip %i bits\n",
                   s->frame_num,
                   len - (bitstream_tell(bc) - s->frame_offset) - 1);
            s->packet_loss = 1;
            return 0;
        }

        /* skip the rest of the frame data */
        bitstream_skip(bc, len - (bitstream_tell(bc) - s->frame_offset) - 1);
    }

    /* decode trailer bit */
    more_frames = bitstream_read_bit(bc);
    ++s->frame_num;
    return more_frames;
}

/**
 * @brief Calculate remaining input buffer length.
 * @param s  codec context
 * @param bc bitstream reader context
 * @return remaining size in bits
 */
static int remaining_bits(WmallDecodeCtx *s, BitstreamContext *bc)
{
    return s->buf_bit_size - bitstream_tell(bc);
}

/**
 * @brief Fill the bit reservoir with a (partial) frame.
 * @param s      codec context
 * @param bc     bitstream reader context
 * @param len    length of the partial frame
 * @param append decides whether to reset the buffer or not
 */
static void save_bits(WmallDecodeCtx *s, BitstreamContext *bc, int len,
                      int append)
{
    int buflen;
    PutBitContext tmp;

    /* when the frame data does not need to be concatenated, the input buffer
        is reset and additional bits from the previous frame are copied
        and skipped later so that a fast byte copy is possible */

    if (!append) {
        s->frame_offset   = bitstream_tell(bc) & 7;
        s->num_saved_bits = s->frame_offset;
        init_put_bits(&s->pb, s->frame_data, MAX_FRAMESIZE);
    }

    buflen = (s->num_saved_bits + len + 8) >> 3;

    if (len <= 0 || buflen > MAX_FRAMESIZE) {
        avpriv_request_sample(s->avctx, "Too small input buffer");
        s->packet_loss = 1;
        return;
    }

    s->num_saved_bits += len;
    if (!append) {
        avpriv_copy_bits(&s->pb, bc->buffer + (bitstream_tell(bc) >> 3),
                         s->num_saved_bits);
    } else {
        int align = 8 - (bitstream_tell(bc) & 7);
        align = FFMIN(align, len);
        put_bits(&s->pb, align, bitstream_read(bc, align));
        len -= align;
        avpriv_copy_bits(&s->pb, bc->buffer + (bitstream_tell(bc) >> 3), len);
    }
    bitstream_skip(bc, len);

    tmp = s->pb;
    flush_put_bits(&tmp);

    bitstream_init(&s->bc, s->frame_data, s->num_saved_bits);
    bitstream_skip(&s->bc, s->frame_offset);
}

static int decode_packet(AVCodecContext *avctx, void *data, int *got_frame_ptr,
                         AVPacket* avpkt)
{
    WmallDecodeCtx *s = avctx->priv_data;
    BitstreamContext *bc = &s->pbc;
    const uint8_t* buf = avpkt->data;
    int buf_size       = avpkt->size;
    int num_bits_prev_frame, packet_sequence_number, spliced_packet;

    s->frame->nb_samples = 0;

    if (s->packet_done || s->packet_loss) {
        s->packet_done = 0;

        /* sanity check for the buffer length */
        if (buf_size < avctx->block_align)
            return 0;

        s->next_packet_start = buf_size - avctx->block_align;
        buf_size             = avctx->block_align;
        s->buf_bit_size      = buf_size << 3;

        /* parse packet header */
        bitstream_init(bc, buf, s->buf_bit_size);
        packet_sequence_number = bitstream_read(bc, 4);
        bitstream_skip(bc, 1); // Skip seekable_frame_in_packet, currently ununused
        spliced_packet = bitstream_read_bit(bc);
        if (spliced_packet)
            avpriv_request_sample(avctx, "Bitstream splicing");

        /* get number of bits that need to be added to the previous frame */
        num_bits_prev_frame = bitstream_read(bc, s->log2_frame_size);

        /* check for packet loss */
        if (!s->packet_loss &&
            ((s->packet_sequence_number + 1) & 0xF) != packet_sequence_number) {
            s->packet_loss = 1;
            av_log(avctx, AV_LOG_ERROR,
                   "Packet loss detected! seq %"PRIx8" vs %x\n",
                   s->packet_sequence_number, packet_sequence_number);
        }
        s->packet_sequence_number = packet_sequence_number;

        if (num_bits_prev_frame > 0) {
            int remaining_packet_bits = s->buf_bit_size - bitstream_tell(bc);
            if (num_bits_prev_frame >= remaining_packet_bits) {
                num_bits_prev_frame = remaining_packet_bits;
                s->packet_done = 1;
            }

            /* Append the previous frame data to the remaining data from the
             * previous packet to create a full frame. */
            save_bits(s, bc, num_bits_prev_frame, 1);

            /* decode the cross packet frame if it is valid */
            if (num_bits_prev_frame < remaining_packet_bits && !s->packet_loss)
                decode_frame(s);
        } else if (s->num_saved_bits - s->frame_offset) {
            ff_dlog(avctx, "ignoring %x previously saved bits\n",
                    s->num_saved_bits - s->frame_offset);
        }

        if (s->packet_loss) {
            /* Reset number of saved bits so that the decoder does not start
             * to decode incomplete frames in the s->len_prefix == 0 case. */
            s->num_saved_bits = 0;
            s->packet_loss    = 0;
            init_put_bits(&s->pb, s->frame_data, MAX_FRAMESIZE);
        }

    } else {
        int frame_size;

        s->buf_bit_size = (avpkt->size - s->next_packet_start) << 3;
        bitstream_init(bc, avpkt->data, s->buf_bit_size);
        bitstream_skip(bc, s->packet_offset);

        if (s->len_prefix && remaining_bits(s, bc) > s->log2_frame_size &&
            (frame_size = bitstream_peek(bc, s->log2_frame_size)) &&
            frame_size <= remaining_bits(s, bc)) {
            save_bits(s, bc, frame_size, 0);
            s->packet_done = !decode_frame(s);
        } else if (!s->len_prefix
                   && s->num_saved_bits > bitstream_tell(&s->bc)) {
            /* when the frames do not have a length prefix, we don't know the
             * compressed length of the individual frames however, we know what
             * part of a new packet belongs to the previous frame therefore we
             * save the incoming packet first, then we append the "previous
             * frame" data from the next packet so that we get a buffer that
             * only contains full frames */
            s->packet_done = !decode_frame(s);
        } else {
            s->packet_done = 1;
        }
    }

    if (s->packet_done && !s->packet_loss &&
        remaining_bits(s, bc) > 0) {
        /* save the rest of the data so that it can be decoded
         * with the next packet */
        save_bits(s, bc, remaining_bits(s, bc), 0);
    }

    *got_frame_ptr   = s->frame->nb_samples > 0;
    av_frame_move_ref(data, s->frame);

    s->packet_offset = bitstream_tell(bc) & 7;

    return (s->packet_loss) ? AVERROR_INVALIDDATA : bitstream_tell(bc) >> 3;
}

static void flush(AVCodecContext *avctx)
{
    WmallDecodeCtx *s    = avctx->priv_data;
    s->packet_loss       = 1;
    s->packet_done       = 0;
    s->num_saved_bits    = 0;
    s->frame_offset      = 0;
    s->next_packet_start = 0;
    s->cdlms[0][0].order = 0;
    s->frame->nb_samples = 0;
    init_put_bits(&s->pb, s->frame_data, MAX_FRAMESIZE);
}

static av_cold int decode_close(AVCodecContext *avctx)
{
    WmallDecodeCtx *s = avctx->priv_data;

    av_frame_free(&s->frame);

    return 0;
}

AVCodec ff_wmalossless_decoder = {
    .name           = "wmalossless",
    .long_name      = NULL_IF_CONFIG_SMALL("Windows Media Audio Lossless"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_WMALOSSLESS,
    .priv_data_size = sizeof(WmallDecodeCtx),
    .init           = decode_init,
    .close          = decode_close,
    .decode         = decode_packet,
    .flush          = flush,
    .capabilities   = AV_CODEC_CAP_SUBFRAMES | AV_CODEC_CAP_DR1 | AV_CODEC_CAP_DELAY,
    .sample_fmts    = (const enum AVSampleFormat[]) { AV_SAMPLE_FMT_S16P,
                                                      AV_SAMPLE_FMT_S32P,
                                                      AV_SAMPLE_FMT_NONE },
};
