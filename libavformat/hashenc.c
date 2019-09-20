/*
 * Hash/MD5 encoder (for codec/format testing)
 * Copyright (c) 2009 Reimar Döffinger, based on crcenc (c) 2002 Fabrice Bellard
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

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/hash.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "avformat.h"
#include "internal.h"

struct HashContext {
    const AVClass *avclass;
    struct AVHashContext **hashes;
    char *hash_name;
    int format_version;
};

#define OFFSET(x) offsetof(struct HashContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
#define HASH_OPT(defaulttype) \
    { "hash", "set hash to use", OFFSET(hash_name), AV_OPT_TYPE_STRING, {.str = defaulttype}, 0, 0, ENC }
#define FORMAT_VERSION_OPT \
    { "format_version", "file format version", OFFSET(format_version), AV_OPT_TYPE_INT, {.i64 = 2}, 1, 2, ENC }

#if CONFIG_HASH_MUXER
static const AVOption hash_options[] = {
    HASH_OPT("sha256"),
    { NULL },
};
#endif

#if CONFIG_FRAMEHASH_MUXER
static const AVOption framehash_options[] = {
    HASH_OPT("sha256"),
    FORMAT_VERSION_OPT,
    { NULL },
};
#endif

#if CONFIG_MD5_MUXER
static const AVOption md5_options[] = {
    HASH_OPT("md5"),
    { NULL },
};
#endif

#if CONFIG_FRAMEMD5_MUXER
static const AVOption framemd5_options[] = {
    HASH_OPT("md5"),
    FORMAT_VERSION_OPT,
    { NULL },
};
#endif

#if CONFIG_HASH_MUXER || CONFIG_MD5_MUXER
static int hash_init(struct AVFormatContext *s)
{
    int res;
    struct HashContext *c = s->priv_data;
    c->hashes = av_mallocz_array(1, sizeof(c->hashes));
    if (!c->hashes)
        return AVERROR(ENOMEM);
    res = av_hash_alloc(&c->hashes[0], c->hash_name);
    if (res < 0)
        return res;
    av_hash_init(c->hashes[0]);
    return 0;
}

static int hash_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    struct HashContext *c = s->priv_data;
    av_hash_update(c->hashes[0], pkt->data, pkt->size);
    return 0;
}

static int hash_write_trailer(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    char buf[AV_HASH_MAX_SIZE*2+128];
    snprintf(buf, sizeof(buf) - 200, "%s=", av_hash_get_name(c->hashes[0]));

    av_hash_final_hex(c->hashes[0], buf + strlen(buf), sizeof(buf) - strlen(buf));
    av_strlcatf(buf, sizeof(buf), "\n");
    avio_write(s->pb, buf, strlen(buf));
    avio_flush(s->pb);

    return 0;
}

static void hash_free(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    if (c->hashes)
        av_hash_freep(&c->hashes[0]);
    av_freep(&c->hashes);
}
#endif

#if CONFIG_HASH_MUXER
static const AVClass hashenc_class = {
    .class_name = "hash muxer",
    .item_name  = av_default_item_name,
    .option     = hash_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_hash_muxer = {
    .name              = "hash",
    .long_name         = NULL_IF_CONFIG_SMALL("Hash testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .init              = hash_init,
    .write_packet      = hash_write_packet,
    .write_trailer     = hash_write_trailer,
    .deinit            = hash_free,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &hashenc_class,
};
#endif

#if CONFIG_MD5_MUXER
static const AVClass md5enc_class = {
    .class_name = "MD5 muxer",
    .item_name  = av_default_item_name,
    .option     = md5_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_md5_muxer = {
    .name              = "md5",
    .long_name         = NULL_IF_CONFIG_SMALL("MD5 testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .init              = hash_init,
    .write_packet      = hash_write_packet,
    .write_trailer     = hash_write_trailer,
    .deinit            = hash_free,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &md5enc_class,
};
#endif

#if CONFIG_FRAMEHASH_MUXER || CONFIG_FRAMEMD5_MUXER
static void framehash_print_extradata(struct AVFormatContext *s)
{
    int i;

    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st = s->streams[i];
        AVCodecParameters *par = st->codecpar;
        if (par->extradata) {
            struct HashContext *c = s->priv_data;
            char buf[AV_HASH_MAX_SIZE*2+1];

            avio_printf(s->pb, "#extradata %d, %31d, ", i, par->extradata_size);
            av_hash_init(c->hashes[0]);
            av_hash_update(c->hashes[0], par->extradata, par->extradata_size);
            av_hash_final_hex(c->hashes[0], buf, sizeof(buf));
            avio_write(s->pb, buf, strlen(buf));
            avio_printf(s->pb, "\n");
        }
    }
}

static int framehash_init(struct AVFormatContext *s)
{
    int res;
    struct HashContext *c = s->priv_data;
    c->hashes = av_mallocz_array(1, sizeof(c->hashes));
    if (!c->hashes)
        return AVERROR(ENOMEM);
    res = av_hash_alloc(&c->hashes[0], c->hash_name);
    if (res < 0)
        return res;
    return 0;
}

static int framehash_write_header(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    avio_printf(s->pb, "#format: frame checksums\n");
    avio_printf(s->pb, "#version: %d\n", c->format_version);
    avio_printf(s->pb, "#hash: %s\n", av_hash_get_name(c->hashes[0]));
    framehash_print_extradata(s);
    ff_framehash_write_header(s);
    avio_printf(s->pb, "#stream#, dts,        pts, duration,     size, hash\n");
    return 0;
}

static int framehash_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    struct HashContext *c = s->priv_data;
    char buf[AV_HASH_MAX_SIZE*2+128];
    int len;
    av_hash_init(c->hashes[0]);
    av_hash_update(c->hashes[0], pkt->data, pkt->size);

    snprintf(buf, sizeof(buf) - (AV_HASH_MAX_SIZE * 2 + 1), "%d, %10"PRId64", %10"PRId64", %8"PRId64", %8d, ",
             pkt->stream_index, pkt->dts, pkt->pts, pkt->duration, pkt->size);
    len = strlen(buf);
    av_hash_final_hex(c->hashes[0], buf + len, sizeof(buf) - len);
    avio_write(s->pb, buf, strlen(buf));

    if (c->format_version > 1 && pkt->side_data_elems) {
        int i, j;
        avio_printf(s->pb, ", S=%d", pkt->side_data_elems);
        for (i = 0; i < pkt->side_data_elems; i++) {
            av_hash_init(c->hashes[0]);
            if (HAVE_BIGENDIAN && pkt->side_data[i].type == AV_PKT_DATA_PALETTE) {
                for (j = 0; j < pkt->side_data[i].size; j += sizeof(uint32_t)) {
                    uint32_t data = AV_RL32(pkt->side_data[i].data + j);
                    av_hash_update(c->hashes[0], (uint8_t *)&data, sizeof(uint32_t));
                }
            } else
                av_hash_update(c->hashes[0], pkt->side_data[i].data, pkt->side_data[i].size);
            snprintf(buf, sizeof(buf) - (AV_HASH_MAX_SIZE * 2 + 1), ", %8d, ", pkt->side_data[i].size);
            len = strlen(buf);
            av_hash_final_hex(c->hashes[0], buf + len, sizeof(buf) - len);
            avio_write(s->pb, buf, strlen(buf));
        }
    }

    avio_printf(s->pb, "\n");
    avio_flush(s->pb);
    return 0;
}

static void framehash_free(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    if (c->hashes)
        av_hash_freep(&c->hashes[0]);
    av_freep(&c->hashes);
}
#endif

#if CONFIG_FRAMEHASH_MUXER
static const AVClass framehash_class = {
    .class_name = "frame hash muxer",
    .item_name  = av_default_item_name,
    .option     = framehash_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_framehash_muxer = {
    .name              = "framehash",
    .long_name         = NULL_IF_CONFIG_SMALL("Per-frame hash testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .init              = framehash_init,
    .write_header      = framehash_write_header,
    .write_packet      = framehash_write_packet,
    .deinit            = framehash_free,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &framehash_class,
};
#endif

#if CONFIG_FRAMEMD5_MUXER
static const AVClass framemd5_class = {
    .class_name = "frame MD5 muxer",
    .item_name  = av_default_item_name,
    .option     = framemd5_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_framemd5_muxer = {
    .name              = "framemd5",
    .long_name         = NULL_IF_CONFIG_SMALL("Per-frame MD5 testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .init              = framehash_init,
    .write_header      = framehash_write_header,
    .write_packet      = framehash_write_packet,
    .deinit            = framehash_free,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &framemd5_class,
};
#endif
