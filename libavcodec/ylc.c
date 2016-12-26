/*
 * YUY2 Lossless Codec
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"
#include "avcodec.h"
#include "bswapdsp.h"
#include "get_bits.h"
#include "huffyuvdsp.h"
#include "internal.h"
#include "unary.h"

typedef struct YLCContext {
    VLC vlc[4];
    uint32_t table[1024];
    uint8_t *table_bits;
    uint8_t *bitstream_bits;
    int table_bits_size;
    int bitstream_bits_size;
    BswapDSPContext bdsp;
} YLCContext;

static av_cold int decode_init(AVCodecContext *avctx)
{
    YLCContext *s = avctx->priv_data;

    avctx->pix_fmt = AV_PIX_FMT_YUYV422;
    ff_bswapdsp_init(&s->bdsp);

    return 0;
}

typedef struct Node {
    int16_t  sym;
    int16_t  n0;
    uint32_t count;
    int16_t  l, r;
} Node;

static void get_tree_codes(uint32_t *bits, int16_t *lens, uint8_t *xlat,
                           Node *nodes, int node,
                           uint32_t pfx, int pl, int *pos)
{
    int s;

    s = nodes[node].sym;
    if (s != -1) {
        bits[*pos] = (~pfx) & ((1 << FFMAX(pl, 1)) - 1);
        lens[*pos] = FFMAX(pl, 1);
        xlat[*pos] = s + (pl == 0);
        (*pos)++;
    } else {
        pfx <<= 1;
        pl++;
        get_tree_codes(bits, lens, xlat, nodes, nodes[node].l, pfx, pl,
                       pos);
        pfx |= 1;
        get_tree_codes(bits, lens, xlat, nodes, nodes[node].r, pfx, pl,
                       pos);
    }
}

static int build_vlc(AVCodecContext *avctx, VLC *vlc, const uint32_t *table)
{
    Node nodes[512];
    uint32_t bits[256];
    int16_t lens[256];
    uint8_t xlat[256];
    int cur_node, i, j, pos = 0;

    ff_free_vlc(vlc);

    for (i = 0; i < 256; i++) {
        nodes[i].count = table[i];
        nodes[i].sym   = i;
        nodes[i].n0    = -2;
        nodes[i].l     = i;
        nodes[i].r     = i;
    }

    cur_node = 256;
    j = 0;
    do {
        for (i = 0; ; i++) {
            int new_node = j;
            int first_node = cur_node;
            int second_node = cur_node;
            int nd, st;

            nodes[cur_node].count = -1;

            do {
                int val = nodes[new_node].count;
                if (val && (val < nodes[first_node].count)) {
                    if (val >= nodes[second_node].count) {
                        first_node = new_node;
                    } else {
                        first_node = second_node;
                        second_node = new_node;
                    }
                }
                new_node += 1;
            } while (new_node != cur_node);

            if (first_node == cur_node)
                break;

            nd = nodes[second_node].count;
            st = nodes[first_node].count;
            nodes[second_node].count = 0;
            nodes[first_node].count  = 0;
            nodes[cur_node].count = nd + st;
            nodes[cur_node].sym = -1;
            nodes[cur_node].n0 = cur_node;
            nodes[cur_node].l = first_node;
            nodes[cur_node].r = second_node;
            cur_node++;
        }
        j++;
    } while (cur_node - 256 == j);

    get_tree_codes(bits, lens, xlat, nodes, cur_node - 1, 0, 0, &pos);

    return ff_init_vlc_sparse(vlc, 10, pos, lens, 2, 2, bits, 4, 4, xlat, 1, 1, 0);
}

static const uint8_t table_y1[] = {
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x00,
};

static const uint8_t table_u[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00,
};

static const uint8_t table_y2[] = {
    0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE,
    0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFC,
    0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE,
    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFC, 0xFC,
    0xFC, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFD, 0xFD, 0xFD,
    0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x01, 0xFD, 0xFD, 0xFD, 0xFE,
    0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE,
    0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02,
    0x02, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02,
    0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0xFF, 0xFF,
    0xFF, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02,
    0x02, 0x02, 0x03, 0x03, 0x03, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04,
    0x04, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04,
    0x04, 0x00,
};

static const uint8_t table_v[] = {
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF,
    0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01,
    0xFF, 0x00, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0x00,
    0x01, 0x00,
};

static int decode_frame(AVCodecContext *avctx,
                        void *data, int *got_frame,
                        AVPacket *avpkt)
{
    int TL[4] = { 128, 128, 128, 128 };
    int L[4]  = { 128, 128, 128, 128 };
    YLCContext *s = avctx->priv_data;
    const uint8_t *buf = avpkt->data;
    int ret, x, y, toffset, boffset;
    AVFrame * const p = data;
    GetBitContext gb;
    uint8_t *dst;

    if (avpkt->size <= 16)
        return AVERROR_INVALIDDATA;

    if (AV_RL32(buf) != MKTAG('Y', 'L', 'C', '0') ||
        AV_RL32(buf + 4) != 0)
        return AVERROR_INVALIDDATA;

    toffset = AV_RL32(buf + 8);
    if (toffset < 16 || toffset >= avpkt->size)
        return AVERROR_INVALIDDATA;

    boffset = AV_RL32(buf + 12);
    if (toffset >= boffset || boffset >= avpkt->size)
        return AVERROR_INVALIDDATA;

    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;

    av_fast_malloc(&s->table_bits, &s->table_bits_size,
                   boffset - toffset + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!s->table_bits)
        return AVERROR(ENOMEM);

    memcpy(s->table_bits, avpkt->data + toffset, boffset - toffset);
    memset(s->table_bits + boffset - toffset, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    s->bdsp.bswap_buf((uint32_t *) s->table_bits,
                      (uint32_t *) s->table_bits,
                      (boffset - toffset + 3) >> 2);
    if ((ret = init_get_bits8(&gb, s->table_bits, boffset - toffset)) < 0)
        return ret;

    for (x = 0; x < 1024; x++) {
        unsigned len = get_unary(&gb, 1, 31);
        uint32_t val = ((1U << len) - 1) + get_bits_long(&gb, len);

        s->table[x] = val;
    }

    ret = build_vlc(avctx, &s->vlc[0], &s->table[0  ]);
    if (ret < 0)
        return ret;
    ret = build_vlc(avctx, &s->vlc[1], &s->table[256]);
    if (ret < 0)
        return ret;
    ret = build_vlc(avctx, &s->vlc[2], &s->table[512]);
    if (ret < 0)
        return ret;
    ret = build_vlc(avctx, &s->vlc[3], &s->table[768]);
    if (ret < 0)
        return ret;

    av_fast_malloc(&s->bitstream_bits, &s->bitstream_bits_size,
                   avpkt->size - boffset + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!s->bitstream_bits)
        return AVERROR(ENOMEM);

    memcpy(s->bitstream_bits, avpkt->data + boffset, avpkt->size - boffset);
    memset(s->bitstream_bits + avpkt->size - boffset, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    s->bdsp.bswap_buf((uint32_t *) s->bitstream_bits,
                      (uint32_t *) s->bitstream_bits,
                      (avpkt->size - boffset) >> 2);
    if ((ret = init_get_bits8(&gb, s->bitstream_bits, avpkt->size - boffset)) < 0)
        return ret;

    dst = p->data[0];
    for (y = 0; y < avctx->height; y++) {
        memset(dst, 0, avctx->width * 2);
        dst += p->linesize[0];
    }

    dst = p->data[0];
    for (y = 0; y < avctx->height; y++) {
        for (x = 0; x < avctx->width * 2 && y < avctx->height;) {
            if (get_bits_left(&gb) <= 0)
                return AVERROR_INVALIDDATA;

            if (get_bits1(&gb)) {
                int val = get_vlc2(&gb, s->vlc[0].table, s->vlc[0].bits, 3);
                if (val < 0) {
                    return AVERROR_INVALIDDATA;
                } else if (val < 0xE1) {
                    dst[x    ] = table_y1[val];
                    dst[x + 1] = table_u[val];
                    dst[x + 2] = table_y2[val];
                    dst[x + 3] = table_v[val];
                    x += 4;
                } else {
                    int incr = (val - 0xDF) * 4;
                    if (x + incr >= avctx->width * 2) {
                        int iy = ((x + incr) / (avctx->width * 2));
                        x  = (x + incr) % (avctx->width * 2);
                        y += iy;
                        dst += iy * p->linesize[0];
                    } else {
                        x += incr;
                    }
                }
            } else {
                int y1, y2, u, v;

                y1 = get_vlc2(&gb, s->vlc[1].table, s->vlc[1].bits, 3);
                u  = get_vlc2(&gb, s->vlc[2].table, s->vlc[2].bits, 3);
                y2 = get_vlc2(&gb, s->vlc[1].table, s->vlc[1].bits, 3);
                v  = get_vlc2(&gb, s->vlc[3].table, s->vlc[3].bits, 3);
                if (y1 < 0 || y2 < 0 || u < 0 || v < 0)
                    return AVERROR_INVALIDDATA;
                dst[x    ] = y1;
                dst[x + 1] = u;
                dst[x + 2] = y1 + y2;
                dst[x + 3] = v;
                x += 4;
            }
        }
        dst += p->linesize[0];
    }

    dst = p->data[0];
    for (x = 0; x < avctx->width * 2; x += 4) {
        dst[x    ] =        dst[x    ] + L[0];
        dst[x + 2] = L[0] = dst[x + 2] + L[0];
        L[1] = dst[x + 1] + L[1];
        dst[x + 1] = L[1];
        L[2] = dst[x + 3] + L[2];
        dst[x + 3] = L[2];
    }
    dst += p->linesize[0];

    for (y = 1; y < avctx->height; y++) {
        x = 0;
        dst[x    ] =        dst[x    ] + L[0] + dst[x + 0 - p->linesize[0]] - TL[0];
        dst[x + 2] = L[0] = dst[x + 2] + L[0] + dst[x + 2 - p->linesize[0]] - TL[0];
        TL[0] = dst[x + 2 - p->linesize[0]];
        L[1] = dst[x + 1] + L[1] + dst[x + 1 - p->linesize[0]] - TL[1];
        dst[x + 1] = L[1];
        TL[1] = dst[x + 1 - p->linesize[0]];
        L[2] = dst[x + 3] + L[2] + dst[x + 3 - p->linesize[0]] - TL[2];
        dst[x + 3] = L[2];
        TL[2] = dst[x + 3 - p->linesize[0]];
        for (x = 4; x < avctx->width * 2; x += 4) {
            dst[x    ] =        dst[x    ] + L[0] + dst[x + 0 - p->linesize[0]] - TL[0];
            dst[x + 2] = L[0] = dst[x + 2] + L[0] + dst[x + 2 - p->linesize[0]] - TL[0];
            TL[0] = dst[x + 2 - p->linesize[0]];
            L[1] = dst[x + 1] + L[1] + dst[x + 1 - p->linesize[0]] - TL[1];
            dst[x + 1] = L[1];
            TL[1] = dst[x + 1 - p->linesize[0]];
            L[2] = dst[x + 3] + L[2] + dst[x + 3 - p->linesize[0]] - TL[2];
            dst[x + 3] = L[2];
            TL[2] = dst[x + 3 - p->linesize[0]];
        }
        dst += p->linesize[0];
    }

    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;
    *got_frame   = 1;

    return avpkt->size;
}

static av_cold int decode_end(AVCodecContext *avctx)
{
    YLCContext *s = avctx->priv_data;

    ff_free_vlc(&s->vlc[0]);
    ff_free_vlc(&s->vlc[1]);
    ff_free_vlc(&s->vlc[2]);
    ff_free_vlc(&s->vlc[3]);
    av_freep(&s->table_bits);
    s->table_bits_size = 0;
    av_freep(&s->bitstream_bits);
    s->bitstream_bits_size = 0;

    return 0;
}

AVCodec ff_ylc_decoder = {
    .name           = "ylc",
    .long_name      = NULL_IF_CONFIG_SMALL("YUY2 Lossless Codec"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_YLC,
    .priv_data_size = sizeof(YLCContext),
    .init           = decode_init,
    .close          = decode_end,
    .decode         = decode_frame,
    .capabilities   = AV_CODEC_CAP_DR1,
};
