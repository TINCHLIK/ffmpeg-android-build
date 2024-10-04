/*
 * Avid DNxUncomressed / SMPTE RDD 50 parser
 * Copyright (c) 2024 Martin Schitter
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

/*
 This parser for DNxUncompressed video data is mostly based on
 reverse engineering of output generated by DaVinci Resolve 19
 but was later also checked against the SMPTE RDD 50 specification.

 Limitations: Multiple image planes are not supported.
*/

#include "avcodec.h"
#include "libavutil/intreadwrite.h"

typedef struct DNxUcParseContext {
    uint32_t fourcc_tag;
    uint32_t width;
    uint32_t height;
    uint32_t nr_bytes;
} DNxUcParseContext;

/*
DNxUncompressed frame data comes wrapped in nested boxes of metadata
(box structure: len + fourcc marker + data):

[0-4]   len of outer essence unit box (typically 37 bytes of header + frame data)
[4-7]   fourcc 'pack'

[8-11]  len of "signal info box" (always 21)
[12-15] fourcc 'sinf'
[16-19] frame width / line packing size
[20-23] frame hight / nr of lines
[24-27] fourcc pixel format indicator
[28]    frame_layout (0: progressive, 1: interlaced)

[29-32] len of "signal data box" (nr of frame data bytes + 8)
[33-36] fourcc 'sdat'
[37-..] frame data

A sequence of 'signal info'+'signal data' box pairs wrapped in
'icmp'(=image component) boxes can be utilized to compose more
complex multi plane images.
This feature is only partially supported in the present implementation.
We never pick more than the first pair of info and image data enclosed
in this way.
*/

static int dnxuc_parse(AVCodecParserContext *s,
                    AVCodecContext *avctx,
                    const uint8_t **poutbuf, int *poutbuf_size,
                    const uint8_t *buf, int buf_size)
{
    char fourcc_buf[5];
    const int HEADER_SIZE = 37;
    int icmp_offset = 0;

    DNxUcParseContext *pc;
    pc = (DNxUcParseContext *) s->priv_data;

    if (!buf_size) {
        return 0;
    }
    if (buf_size > 16 && MKTAG('i','c','m','p') == AV_RL32(buf+12)){
        icmp_offset += 8;
    }
    if ( buf_size < 37 + icmp_offset /* check metadata structure expectations */
        || MKTAG('p','a','c','k') != AV_RL32(buf+4+icmp_offset)
        || MKTAG('s','i','n','f') != AV_RL32(buf+12+icmp_offset)
        || MKTAG('s','d','a','t') != AV_RL32(buf+33+icmp_offset)){
            av_log(avctx, AV_LOG_ERROR, "can't read DNxUncompressed metadata.\n");
            *poutbuf_size = 0;
            return buf_size;
    }

    pc->fourcc_tag = AV_RL32(buf+24+icmp_offset);
    pc->width = AV_RL32(buf+16+icmp_offset);
    pc->height = AV_RL32(buf+20+icmp_offset);
    pc->nr_bytes = AV_RL32(buf+29+icmp_offset) - 8;

    if (!avctx->codec_tag) {
        av_fourcc_make_string(fourcc_buf, pc->fourcc_tag);
        av_log(avctx, AV_LOG_INFO, "dnxuc_parser: '%s' %dx%d %dbpp %d\n",
            fourcc_buf,
            pc->width, pc->height,
            (pc->nr_bytes*8)/(pc->width*pc->height),
            pc->nr_bytes);
        avctx->codec_tag = pc->fourcc_tag;
    }

    if (pc->nr_bytes > buf_size - HEADER_SIZE + icmp_offset){
        av_log(avctx, AV_LOG_ERROR, "Insufficient size of image essence data.\n");
        *poutbuf_size = 0;
        return buf_size;
    }

    *poutbuf = buf + HEADER_SIZE + icmp_offset;
    *poutbuf_size = pc->nr_bytes;

    return buf_size;
}

const AVCodecParser ff_dnxuc_parser = {
    .codec_ids      = { AV_CODEC_ID_DNXUC },
    .priv_data_size = sizeof(DNxUcParseContext),
    .parser_parse   = dnxuc_parse,
};
