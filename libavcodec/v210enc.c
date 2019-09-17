/*
 * V210 encoder
 *
 * Copyright (C) 2009 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2009 Baptiste Coudurier <baptiste dot coudurier at gmail dot com>
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

#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "v210enc.h"

#define CLIP(v, depth) av_clip(v, 1 << (depth-8), ((1 << depth)-(1 << (depth-8)) -1))
#define WRITE_PIXELS(a, b, c, depth)                      \
    do {                                                  \
        val  =  CLIP(*a++, depth)  << (10-depth);         \
        val |=  (CLIP(*b++, depth) << (20-depth)) |       \
                (CLIP(*c++, depth) << (30-depth));        \
        AV_WL32(dst, val);                                \
        dst += 4;                                         \
    } while (0)

static void v210_planar_pack_8_c(const uint8_t *y, const uint8_t *u,
                                 const uint8_t *v, uint8_t *dst,
                                 ptrdiff_t width)
{
    uint32_t val;
    int i;

    /* unroll this to match the assembly */
    for (i = 0; i < width - 11; i += 12) {
        WRITE_PIXELS(u, y, v, 8);
        WRITE_PIXELS(y, u, y, 8);
        WRITE_PIXELS(v, y, u, 8);
        WRITE_PIXELS(y, v, y, 8);
        WRITE_PIXELS(u, y, v, 8);
        WRITE_PIXELS(y, u, y, 8);
        WRITE_PIXELS(v, y, u, 8);
        WRITE_PIXELS(y, v, y, 8);
    }
}

static void v210_planar_pack_10_c(const uint16_t *y, const uint16_t *u,
                                  const uint16_t *v, uint8_t *dst,
                                  ptrdiff_t width)
{
    uint32_t val;
    int i;

    for (i = 0; i < width - 5; i += 6) {
        WRITE_PIXELS(u, y, v, 10);
        WRITE_PIXELS(y, u, y, 10);
        WRITE_PIXELS(v, y, u, 10);
        WRITE_PIXELS(y, v, y, 10);
    }
}

av_cold void ff_v210enc_init(V210EncContext *s)
{
    s->pack_line_8  = v210_planar_pack_8_c;
    s->pack_line_10 = v210_planar_pack_10_c;
    s->sample_factor_8  = 1;
    s->sample_factor_10 = 1;

    if (ARCH_X86)
        ff_v210enc_init_x86(s);
}

static av_cold int encode_init(AVCodecContext *avctx)
{
    V210EncContext *s = avctx->priv_data;

    if (avctx->width & 1) {
        av_log(avctx, AV_LOG_ERROR, "v210 needs even width\n");
        return AVERROR(EINVAL);
    }

#if FF_API_CODED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
FF_ENABLE_DEPRECATION_WARNINGS
#endif

    ff_v210enc_init(s);

    avctx->bits_per_coded_sample = 20;
    avctx->bit_rate = ff_guess_coded_bitrate(avctx) * 16 / 15;

    return 0;
}

static int encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                        const AVFrame *pic, int *got_packet)
{
    V210EncContext *s = avctx->priv_data;
    int aligned_width = ((avctx->width + 47) / 48) * 48;
    int stride = aligned_width * 8 / 3;
    int line_padding = stride - ((avctx->width * 8 + 11) / 12) * 4;
    AVFrameSideData *side_data;
    int h, w, ret;
    uint8_t *dst;

    ret = ff_alloc_packet2(avctx, pkt, avctx->height * stride, avctx->height * stride);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error getting output packet.\n");
        return ret;
    }
    dst = pkt->data;

    if (pic->format == AV_PIX_FMT_YUV422P10) {
        const uint16_t *y = (const uint16_t *)pic->data[0];
        const uint16_t *u = (const uint16_t *)pic->data[1];
        const uint16_t *v = (const uint16_t *)pic->data[2];

        const int sample_size = 6 * s->sample_factor_10;
        const int sample_w    = avctx->width / sample_size;

        for (h = 0; h < avctx->height; h++) {
            uint32_t val;
            w = sample_w * sample_size;
            s->pack_line_10(y, u, v, dst, w);

            y += w;
            u += w >> 1;
            v += w >> 1;
            dst += sample_w * 16 * s->sample_factor_10;

            for (; w < avctx->width - 5; w += 6) {
                WRITE_PIXELS(u, y, v, 10);
                WRITE_PIXELS(y, u, y, 10);
                WRITE_PIXELS(v, y, u, 10);
                WRITE_PIXELS(y, v, y, 10);
            }
            if (w < avctx->width - 1) {
                WRITE_PIXELS(u, y, v, 10);

                val = CLIP(*y++, 10);
                if (w == avctx->width - 2) {
                    AV_WL32(dst, val);
                    dst += 4;
                }
            }
            if (w < avctx->width - 3) {
                val |= (CLIP(*u++, 10) << 10) | (CLIP(*y++, 10) << 20);
                AV_WL32(dst, val);
                dst += 4;

                val = CLIP(*v++, 10) | (CLIP(*y++, 10) << 10);
                AV_WL32(dst, val);
                dst += 4;
            }

            memset(dst, 0, line_padding);
            dst += line_padding;
            y += pic->linesize[0] / 2 - avctx->width;
            u += pic->linesize[1] / 2 - avctx->width / 2;
            v += pic->linesize[2] / 2 - avctx->width / 2;
        }
    } else if(pic->format == AV_PIX_FMT_YUV422P) {
        const uint8_t *y = pic->data[0];
        const uint8_t *u = pic->data[1];
        const uint8_t *v = pic->data[2];

        const int sample_size = 12 * s->sample_factor_8;
        const int sample_w    = avctx->width / sample_size;

        for (h = 0; h < avctx->height; h++) {
            uint32_t val;
            w = sample_w * sample_size;
            s->pack_line_8(y, u, v, dst, w);

            y += w;
            u += w >> 1;
            v += w >> 1;
            dst += sample_w * 32 * s->sample_factor_8;

            for (; w < avctx->width - 5; w += 6) {
                WRITE_PIXELS(u, y, v, 8);
                WRITE_PIXELS(y, u, y, 8);
                WRITE_PIXELS(v, y, u, 8);
                WRITE_PIXELS(y, v, y, 8);
            }
            if (w < avctx->width - 1) {
                WRITE_PIXELS(u, y, v, 8);

                val = CLIP(*y++, 8) << 2;
                if (w == avctx->width - 2) {
                    AV_WL32(dst, val);
                    dst += 4;
                }
            }
            if (w < avctx->width - 3) {
                val |= (CLIP(*u++, 8) << 12) | (CLIP(*y++, 8) << 22);
                AV_WL32(dst, val);
                dst += 4;

                val = (CLIP(*v++, 8) << 2) | (CLIP(*y++, 8) << 12);
                AV_WL32(dst, val);
                dst += 4;
            }
            memset(dst, 0, line_padding);
            dst += line_padding;

            y += pic->linesize[0] - avctx->width;
            u += pic->linesize[1] - avctx->width / 2;
            v += pic->linesize[2] - avctx->width / 2;
        }
    }

    side_data = av_frame_get_side_data(pic, AV_FRAME_DATA_A53_CC);
    if (side_data && side_data->size) {
        uint8_t *buf = av_packet_new_side_data(pkt, AV_PKT_DATA_A53_CC, side_data->size);
        if (!buf)
            return AVERROR(ENOMEM);
        memcpy(buf, side_data->data, side_data->size);
    }

    side_data = av_frame_get_side_data(pic, AV_FRAME_DATA_AFD);
    if (side_data && side_data->size) {
        uint8_t *buf = av_packet_new_side_data(pkt, AV_PKT_DATA_AFD, side_data->size);
        if (!buf)
            return AVERROR(ENOMEM);
        memcpy(buf, side_data->data, side_data->size);
    }

    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;
    return 0;
}

AVCodec ff_v210_encoder = {
    .name           = "v210",
    .long_name      = NULL_IF_CONFIG_SMALL("Uncompressed 4:2:2 10-bit"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_V210,
    .priv_data_size = sizeof(V210EncContext),
    .init           = encode_init,
    .encode2        = encode_frame,
    .pix_fmts       = (const enum AVPixelFormat[]){ AV_PIX_FMT_YUV422P10, AV_PIX_FMT_YUV422P, AV_PIX_FMT_NONE },
};
