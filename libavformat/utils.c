/*
 * various utility functions for use within FFmpeg
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
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

#include <stdint.h>

#include "config.h"

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/dict.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavutil/pixfmt.h"
#include "libavutil/thread.h"
#include "libavutil/time.h"

#include "libavcodec/bsf.h"
#include "libavcodec/internal.h"
#include "libavcodec/packet_internal.h"

#include "avformat.h"
#include "avio_internal.h"
#include "demux.h"
#include "internal.h"
#if CONFIG_NETWORK
#include "network.h"
#endif

static AVMutex avformat_mutex = AV_MUTEX_INITIALIZER;

/**
 * @file
 * various utility functions for use within FFmpeg
 */

int ff_lock_avformat(void)
{
    return ff_mutex_lock(&avformat_mutex) ? -1 : 0;
}

int ff_unlock_avformat(void)
{
    return ff_mutex_unlock(&avformat_mutex) ? -1 : 0;
}

int ff_copy_whiteblacklists(AVFormatContext *dst, const AVFormatContext *src)
{
    av_assert0(!dst->codec_whitelist &&
               !dst->format_whitelist &&
               !dst->protocol_whitelist &&
               !dst->protocol_blacklist);
    dst-> codec_whitelist = av_strdup(src->codec_whitelist);
    dst->format_whitelist = av_strdup(src->format_whitelist);
    dst->protocol_whitelist = av_strdup(src->protocol_whitelist);
    dst->protocol_blacklist = av_strdup(src->protocol_blacklist);
    if (   (src-> codec_whitelist && !dst-> codec_whitelist)
        || (src->  format_whitelist && !dst->  format_whitelist)
        || (src->protocol_whitelist && !dst->protocol_whitelist)
        || (src->protocol_blacklist && !dst->protocol_blacklist)) {
        av_log(dst, AV_LOG_ERROR, "Failed to duplicate black/whitelist\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

const AVCodec *ff_find_decoder(AVFormatContext *s, const AVStream *st,
                               enum AVCodecID codec_id)
{
    switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        if (s->video_codec)    return s->video_codec;
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (s->audio_codec)    return s->audio_codec;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        if (s->subtitle_codec) return s->subtitle_codec;
        break;
    }

    return avcodec_find_decoder(codec_id);
}

/* an arbitrarily chosen "sane" max packet size -- 50M */
#define SANE_CHUNK_SIZE (50000000)

/* Read the data in sane-sized chunks and append to pkt.
 * Return the number of bytes read or an error. */
static int append_packet_chunked(AVIOContext *s, AVPacket *pkt, int size)
{
    int orig_size      = pkt->size;
    int ret;

    do {
        int prev_size = pkt->size;
        int read_size;

        /* When the caller requests a lot of data, limit it to the amount
         * left in file or SANE_CHUNK_SIZE when it is not known. */
        read_size = size;
        if (read_size > SANE_CHUNK_SIZE/10) {
            read_size = ffio_limit(s, read_size);
            // If filesize/maxsize is unknown, limit to SANE_CHUNK_SIZE
            if (ffiocontext(s)->maxsize < 0)
                read_size = FFMIN(read_size, SANE_CHUNK_SIZE);
        }

        ret = av_grow_packet(pkt, read_size);
        if (ret < 0)
            break;

        ret = avio_read(s, pkt->data + prev_size, read_size);
        if (ret != read_size) {
            av_shrink_packet(pkt, prev_size + FFMAX(ret, 0));
            break;
        }

        size -= read_size;
    } while (size > 0);
    if (size > 0)
        pkt->flags |= AV_PKT_FLAG_CORRUPT;

    if (!pkt->size)
        av_packet_unref(pkt);
    return pkt->size > orig_size ? pkt->size - orig_size : ret;
}

int av_get_packet(AVIOContext *s, AVPacket *pkt, int size)
{
#if FF_API_INIT_PACKET
FF_DISABLE_DEPRECATION_WARNINGS
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
FF_ENABLE_DEPRECATION_WARNINGS
#else
    av_packet_unref(pkt);
#endif
    pkt->pos  = avio_tell(s);

    return append_packet_chunked(s, pkt, size);
}

int av_append_packet(AVIOContext *s, AVPacket *pkt, int size)
{
    if (!pkt->size)
        return av_get_packet(s, pkt, size);
    return append_packet_chunked(s, pkt, size);
}

int av_filename_number_test(const char *filename)
{
    char buf[1024];
    return filename &&
           (av_get_frame_filename(buf, sizeof(buf), filename, 1) >= 0);
}

/**********************************************************/

int ff_is_intra_only(enum AVCodecID id)
{
    const AVCodecDescriptor *d = avcodec_descriptor_get(id);
    if (!d)
        return 0;
    if ((d->type == AVMEDIA_TYPE_VIDEO || d->type == AVMEDIA_TYPE_AUDIO) &&
        !(d->props & AV_CODEC_PROP_INTRA_ONLY))
        return 0;
    return 1;
}

/* XXX: suppress the packet queue */
void ff_flush_packet_queue(AVFormatContext *s)
{
    FFFormatContext *const si = ffformatcontext(s);
    avpriv_packet_list_free(&si->parse_queue);
    avpriv_packet_list_free(&si->packet_buffer);
    avpriv_packet_list_free(&si->raw_packet_buffer);

    si->raw_packet_buffer_size = 0;
}

int av_find_default_stream_index(AVFormatContext *s)
{
    int best_stream = 0;
    int best_score = INT_MIN;

    if (s->nb_streams <= 0)
        return -1;
    for (unsigned i = 0; i < s->nb_streams; i++) {
        const AVStream *const st = s->streams[i];
        const FFStream *const sti = cffstream(st);
        int score = 0;
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (st->disposition & AV_DISPOSITION_ATTACHED_PIC)
                score -= 400;
            if (st->codecpar->width && st->codecpar->height)
                score += 50;
            score+= 25;
        }
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (st->codecpar->sample_rate)
                score += 50;
        }
        if (sti->codec_info_nb_frames)
            score += 12;

        if (st->discard != AVDISCARD_ALL)
            score += 200;

        if (score > best_score) {
            best_score = score;
            best_stream = i;
        }
    }
    return best_stream;
}

/*******************************************************/

unsigned int ff_codec_get_tag(const AVCodecTag *tags, enum AVCodecID id)
{
    while (tags->id != AV_CODEC_ID_NONE) {
        if (tags->id == id)
            return tags->tag;
        tags++;
    }
    return 0;
}

enum AVCodecID ff_codec_get_id(const AVCodecTag *tags, unsigned int tag)
{
    for (int i = 0; tags[i].id != AV_CODEC_ID_NONE; i++)
        if (tag == tags[i].tag)
            return tags[i].id;
    for (int i = 0; tags[i].id != AV_CODEC_ID_NONE; i++)
        if (ff_toupper4(tag) == ff_toupper4(tags[i].tag))
            return tags[i].id;
    return AV_CODEC_ID_NONE;
}

enum AVCodecID ff_get_pcm_codec_id(int bps, int flt, int be, int sflags)
{
    if (bps <= 0 || bps > 64)
        return AV_CODEC_ID_NONE;

    if (flt) {
        switch (bps) {
        case 32:
            return be ? AV_CODEC_ID_PCM_F32BE : AV_CODEC_ID_PCM_F32LE;
        case 64:
            return be ? AV_CODEC_ID_PCM_F64BE : AV_CODEC_ID_PCM_F64LE;
        default:
            return AV_CODEC_ID_NONE;
        }
    } else {
        bps  += 7;
        bps >>= 3;
        if (sflags & (1 << (bps - 1))) {
            switch (bps) {
            case 1:
                return AV_CODEC_ID_PCM_S8;
            case 2:
                return be ? AV_CODEC_ID_PCM_S16BE : AV_CODEC_ID_PCM_S16LE;
            case 3:
                return be ? AV_CODEC_ID_PCM_S24BE : AV_CODEC_ID_PCM_S24LE;
            case 4:
                return be ? AV_CODEC_ID_PCM_S32BE : AV_CODEC_ID_PCM_S32LE;
            case 8:
                return be ? AV_CODEC_ID_PCM_S64BE : AV_CODEC_ID_PCM_S64LE;
            default:
                return AV_CODEC_ID_NONE;
            }
        } else {
            switch (bps) {
            case 1:
                return AV_CODEC_ID_PCM_U8;
            case 2:
                return be ? AV_CODEC_ID_PCM_U16BE : AV_CODEC_ID_PCM_U16LE;
            case 3:
                return be ? AV_CODEC_ID_PCM_U24BE : AV_CODEC_ID_PCM_U24LE;
            case 4:
                return be ? AV_CODEC_ID_PCM_U32BE : AV_CODEC_ID_PCM_U32LE;
            default:
                return AV_CODEC_ID_NONE;
            }
        }
    }
}

unsigned int av_codec_get_tag(const AVCodecTag *const *tags, enum AVCodecID id)
{
    unsigned int tag;
    if (!av_codec_get_tag2(tags, id, &tag))
        return 0;
    return tag;
}

int av_codec_get_tag2(const AVCodecTag * const *tags, enum AVCodecID id,
                      unsigned int *tag)
{
    for (int i = 0; tags && tags[i]; i++) {
        const AVCodecTag *codec_tags = tags[i];
        while (codec_tags->id != AV_CODEC_ID_NONE) {
            if (codec_tags->id == id) {
                *tag = codec_tags->tag;
                return 1;
            }
            codec_tags++;
        }
    }
    return 0;
}

enum AVCodecID av_codec_get_id(const AVCodecTag *const *tags, unsigned int tag)
{
    for (int i = 0; tags && tags[i]; i++) {
        enum AVCodecID id = ff_codec_get_id(tags[i], tag);
        if (id != AV_CODEC_ID_NONE)
            return id;
    }
    return AV_CODEC_ID_NONE;
}

int ff_alloc_extradata(AVCodecParameters *par, int size)
{
    av_freep(&par->extradata);
    par->extradata_size = 0;

    if (size < 0 || size >= INT32_MAX - AV_INPUT_BUFFER_PADDING_SIZE)
        return AVERROR(EINVAL);

    par->extradata = av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!par->extradata)
        return AVERROR(ENOMEM);

    memset(par->extradata + size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    par->extradata_size = size;

    return 0;
}

AVProgram *av_find_program_from_stream(AVFormatContext *ic, AVProgram *last, int s)
{
    for (unsigned i = 0; i < ic->nb_programs; i++) {
        if (ic->programs[i] == last) {
            last = NULL;
        } else {
            if (!last)
                for (unsigned j = 0; j < ic->programs[i]->nb_stream_indexes; j++)
                    if (ic->programs[i]->stream_index[j] == s)
                        return ic->programs[i];
        }
    }
    return NULL;
}

int av_find_best_stream(AVFormatContext *ic, enum AVMediaType type,
                        int wanted_stream_nb, int related_stream,
                        const AVCodec **decoder_ret, int flags)
{
    int nb_streams = ic->nb_streams;
    int ret = AVERROR_STREAM_NOT_FOUND;
    int best_count = -1, best_multiframe = -1, best_disposition = -1;
    int count, multiframe, disposition;
    int64_t best_bitrate = -1;
    int64_t bitrate;
    unsigned *program = NULL;
    const AVCodec *decoder = NULL, *best_decoder = NULL;

    if (related_stream >= 0 && wanted_stream_nb < 0) {
        AVProgram *p = av_find_program_from_stream(ic, NULL, related_stream);
        if (p) {
            program    = p->stream_index;
            nb_streams = p->nb_stream_indexes;
        }
    }
    for (unsigned i = 0; i < nb_streams; i++) {
        int real_stream_index = program ? program[i] : i;
        AVStream *st          = ic->streams[real_stream_index];
        AVCodecParameters *par = st->codecpar;
        if (par->codec_type != type)
            continue;
        if (wanted_stream_nb >= 0 && real_stream_index != wanted_stream_nb)
            continue;
        if (type == AVMEDIA_TYPE_AUDIO && !(par->ch_layout.nb_channels && par->sample_rate))
            continue;
        if (decoder_ret) {
            decoder = ff_find_decoder(ic, st, par->codec_id);
            if (!decoder) {
                if (ret < 0)
                    ret = AVERROR_DECODER_NOT_FOUND;
                continue;
            }
        }
        disposition = !(st->disposition & (AV_DISPOSITION_HEARING_IMPAIRED | AV_DISPOSITION_VISUAL_IMPAIRED))
                      + !! (st->disposition & AV_DISPOSITION_DEFAULT);
        count = ffstream(st)->codec_info_nb_frames;
        bitrate = par->bit_rate;
        multiframe = FFMIN(5, count);
        if ((best_disposition >  disposition) ||
            (best_disposition == disposition && best_multiframe >  multiframe) ||
            (best_disposition == disposition && best_multiframe == multiframe && best_bitrate >  bitrate) ||
            (best_disposition == disposition && best_multiframe == multiframe && best_bitrate == bitrate && best_count >= count))
            continue;
        best_disposition = disposition;
        best_count   = count;
        best_bitrate = bitrate;
        best_multiframe = multiframe;
        ret          = real_stream_index;
        best_decoder = decoder;
        if (program && i == nb_streams - 1 && ret < 0) {
            program    = NULL;
            nb_streams = ic->nb_streams;
            /* no related stream found, try again with everything */
            i = 0;
        }
    }
    if (decoder_ret)
        *decoder_ret = best_decoder;
    return ret;
}

/*******************************************************/

int ff_stream_side_data_copy(AVStream *dst, const AVStream *src)
{
    /* Free existing side data*/
    for (int i = 0; i < dst->nb_side_data; i++)
        av_free(dst->side_data[i].data);
    av_freep(&dst->side_data);
    dst->nb_side_data = 0;

    /* Copy side data if present */
    if (src->nb_side_data) {
        dst->side_data = av_calloc(src->nb_side_data,
                                   sizeof(*dst->side_data));
        if (!dst->side_data)
            return AVERROR(ENOMEM);
        dst->nb_side_data = src->nb_side_data;

        for (int i = 0; i < src->nb_side_data; i++) {
            uint8_t *data = av_memdup(src->side_data[i].data,
                                      src->side_data[i].size);
            if (!data)
                return AVERROR(ENOMEM);
            dst->side_data[i].type = src->side_data[i].type;
            dst->side_data[i].size = src->side_data[i].size;
            dst->side_data[i].data = data;
        }
    }

    return 0;
}

void ff_free_stream(AVStream **pst)
{
    AVStream *st = *pst;
    FFStream *const sti = ffstream(st);

    if (!st)
        return;

    for (int i = 0; i < st->nb_side_data; i++)
        av_freep(&st->side_data[i].data);
    av_freep(&st->side_data);

    if (st->attached_pic.data)
        av_packet_unref(&st->attached_pic);

    av_parser_close(sti->parser);
    avcodec_free_context(&sti->avctx);
    av_bsf_free(&sti->bsfc);
    av_freep(&sti->priv_pts);
    av_freep(&sti->index_entries);
    av_freep(&sti->probe_data.buf);

    av_bsf_free(&sti->extract_extradata.bsf);

    if (sti->info) {
        av_freep(&sti->info->duration_error);
        av_freep(&sti->info);
    }

    av_dict_free(&st->metadata);
    avcodec_parameters_free(&st->codecpar);
    av_freep(&st->priv_data);

    av_freep(pst);
}

void ff_remove_stream(AVFormatContext *s, AVStream *st)
{
    av_assert0(s->nb_streams>0);
    av_assert0(s->streams[ s->nb_streams - 1 ] == st);

    ff_free_stream(&s->streams[ --s->nb_streams ]);
}

void avformat_free_context(AVFormatContext *s)
{
    FFFormatContext *si;

    if (!s)
        return;
    si = ffformatcontext(s);

    if (s->oformat && s->oformat->deinit && si->initialized)
        s->oformat->deinit(s);

    av_opt_free(s);
    if (s->iformat && s->iformat->priv_class && s->priv_data)
        av_opt_free(s->priv_data);
    if (s->oformat && s->oformat->priv_class && s->priv_data)
        av_opt_free(s->priv_data);

    for (unsigned i = 0; i < s->nb_streams; i++)
        ff_free_stream(&s->streams[i]);
    s->nb_streams = 0;

    for (unsigned i = 0; i < s->nb_programs; i++) {
        av_dict_free(&s->programs[i]->metadata);
        av_freep(&s->programs[i]->stream_index);
        av_freep(&s->programs[i]);
    }
    s->nb_programs = 0;

    av_freep(&s->programs);
    av_freep(&s->priv_data);
    while (s->nb_chapters--) {
        av_dict_free(&s->chapters[s->nb_chapters]->metadata);
        av_freep(&s->chapters[s->nb_chapters]);
    }
    av_freep(&s->chapters);
    av_dict_free(&s->metadata);
    av_dict_free(&si->id3v2_meta);
    av_packet_free(&si->pkt);
    av_packet_free(&si->parse_pkt);
    av_freep(&s->streams);
    ff_flush_packet_queue(s);
    av_freep(&s->url);
    av_free(s);
}

AVProgram *av_new_program(AVFormatContext *ac, int id)
{
    AVProgram *program = NULL;
    int ret;

    av_log(ac, AV_LOG_TRACE, "new_program: id=0x%04x\n", id);

    for (unsigned i = 0; i < ac->nb_programs; i++)
        if (ac->programs[i]->id == id)
            program = ac->programs[i];

    if (!program) {
        program = av_mallocz(sizeof(AVProgram));
        if (!program)
            return NULL;
        ret = av_dynarray_add_nofree(&ac->programs, &ac->nb_programs, program);
        if (ret < 0) {
            av_free(program);
            return NULL;
        }
        program->discard = AVDISCARD_NONE;
        program->pmt_version = -1;
        program->id = id;
        program->pts_wrap_reference = AV_NOPTS_VALUE;
        program->pts_wrap_behavior = AV_PTS_WRAP_IGNORE;
        program->start_time =
        program->end_time   = AV_NOPTS_VALUE;
    }
    return program;
}

void av_program_add_stream_index(AVFormatContext *ac, int progid, unsigned idx)
{
    AVProgram *program = NULL;
    void *tmp;

    if (idx >= ac->nb_streams) {
        av_log(ac, AV_LOG_ERROR, "stream index %d is not valid\n", idx);
        return;
    }

    for (unsigned i = 0; i < ac->nb_programs; i++) {
        if (ac->programs[i]->id != progid)
            continue;
        program = ac->programs[i];
        for (unsigned j = 0; j < program->nb_stream_indexes; j++)
            if (program->stream_index[j] == idx)
                return;

        tmp = av_realloc_array(program->stream_index, program->nb_stream_indexes+1, sizeof(unsigned int));
        if (!tmp)
            return;
        program->stream_index = tmp;
        program->stream_index[program->nb_stream_indexes++] = idx;
        return;
    }
}

uint64_t ff_ntp_time(void)
{
    return (av_gettime() / 1000) * 1000 + NTP_OFFSET_US;
}

uint64_t ff_get_formatted_ntp_time(uint64_t ntp_time_us)
{
    uint64_t ntp_ts, frac_part, sec;
    uint32_t usec;

    //current ntp time in seconds and micro seconds
    sec = ntp_time_us / 1000000;
    usec = ntp_time_us % 1000000;

    //encoding in ntp timestamp format
    frac_part = usec * 0xFFFFFFFFULL;
    frac_part /= 1000000;

    if (sec > 0xFFFFFFFFULL)
        av_log(NULL, AV_LOG_WARNING, "NTP time format roll over detected\n");

    ntp_ts = sec << 32;
    ntp_ts |= frac_part;

    return ntp_ts;
}

uint64_t ff_parse_ntp_time(uint64_t ntp_ts)
{
    uint64_t sec = ntp_ts >> 32;
    uint64_t frac_part = ntp_ts & 0xFFFFFFFFULL;
    uint64_t usec = (frac_part * 1000000) / 0xFFFFFFFFULL;

    return (sec * 1000000) + usec;
}

int av_get_frame_filename2(char *buf, int buf_size, const char *path, int number, int flags)
{
    const char *p;
    char *q, buf1[20], c;
    int nd, len, percentd_found;

    q = buf;
    p = path;
    percentd_found = 0;
    for (;;) {
        c = *p++;
        if (c == '\0')
            break;
        if (c == '%') {
            do {
                nd = 0;
                while (av_isdigit(*p)) {
                    if (nd >= INT_MAX / 10 - 255)
                        goto fail;
                    nd = nd * 10 + *p++ - '0';
                }
                c = *p++;
            } while (av_isdigit(c));

            switch (c) {
            case '%':
                goto addchar;
            case 'd':
                if (!(flags & AV_FRAME_FILENAME_FLAGS_MULTIPLE) && percentd_found)
                    goto fail;
                percentd_found = 1;
                if (number < 0)
                    nd += 1;
                snprintf(buf1, sizeof(buf1), "%0*d", nd, number);
                len = strlen(buf1);
                if ((q - buf + len) > buf_size - 1)
                    goto fail;
                memcpy(q, buf1, len);
                q += len;
                break;
            default:
                goto fail;
            }
        } else {
addchar:
            if ((q - buf) < buf_size - 1)
                *q++ = c;
        }
    }
    if (!percentd_found)
        goto fail;
    *q = '\0';
    return 0;
fail:
    *q = '\0';
    return -1;
}

int av_get_frame_filename(char *buf, int buf_size, const char *path, int number)
{
    return av_get_frame_filename2(buf, buf_size, path, number, 0);
}

void av_url_split(char *proto, int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname, int hostname_size,
                  int *port_ptr, char *path, int path_size, const char *url)
{
    const char *p, *ls, *at, *at2, *col, *brk;

    if (port_ptr)
        *port_ptr = -1;
    if (proto_size > 0)
        proto[0] = 0;
    if (authorization_size > 0)
        authorization[0] = 0;
    if (hostname_size > 0)
        hostname[0] = 0;
    if (path_size > 0)
        path[0] = 0;

    /* parse protocol */
    if ((p = strchr(url, ':'))) {
        av_strlcpy(proto, url, FFMIN(proto_size, p + 1 - url));
        p++; /* skip ':' */
        if (*p == '/')
            p++;
        if (*p == '/')
            p++;
    } else {
        /* no protocol means plain filename */
        av_strlcpy(path, url, path_size);
        return;
    }

    /* separate path from hostname */
    ls = p + strcspn(p, "/?#");
    av_strlcpy(path, ls, path_size);

    /* the rest is hostname, use that to parse auth/port */
    if (ls != p) {
        /* authorization (user[:pass]@hostname) */
        at2 = p;
        while ((at = strchr(p, '@')) && at < ls) {
            av_strlcpy(authorization, at2,
                       FFMIN(authorization_size, at + 1 - at2));
            p = at + 1; /* skip '@' */
        }

        if (*p == '[' && (brk = strchr(p, ']')) && brk < ls) {
            /* [host]:port */
            av_strlcpy(hostname, p + 1,
                       FFMIN(hostname_size, brk - p));
            if (brk[1] == ':' && port_ptr)
                *port_ptr = atoi(brk + 2);
        } else if ((col = strchr(p, ':')) && col < ls) {
            av_strlcpy(hostname, p,
                       FFMIN(col + 1 - p, hostname_size));
            if (port_ptr)
                *port_ptr = atoi(col + 1);
        } else
            av_strlcpy(hostname, p,
                       FFMIN(ls + 1 - p, hostname_size));
    }
}

int ff_mkdir_p(const char *path)
{
    int ret = 0;
    char *temp = av_strdup(path);
    char *pos = temp;
    char tmp_ch = '\0';

    if (!path || !temp) {
        return -1;
    }

    if (!av_strncasecmp(temp, "/", 1) || !av_strncasecmp(temp, "\\", 1)) {
        pos++;
    } else if (!av_strncasecmp(temp, "./", 2) || !av_strncasecmp(temp, ".\\", 2)) {
        pos += 2;
    }

    for ( ; *pos != '\0'; ++pos) {
        if (*pos == '/' || *pos == '\\') {
            tmp_ch = *pos;
            *pos = '\0';
            ret = mkdir(temp, 0755);
            *pos = tmp_ch;
        }
    }

    if ((*(pos - 1) != '/') && (*(pos - 1) != '\\')) {
        ret = mkdir(temp, 0755);
    }

    av_free(temp);
    return ret;
}

char *ff_data_to_hex(char *buff, const uint8_t *src, int s, int lowercase)
{
    static const char hex_table_uc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F' };
    static const char hex_table_lc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'a', 'b',
                                           'c', 'd', 'e', 'f' };
    const char *hex_table = lowercase ? hex_table_lc : hex_table_uc;

    for (int i = 0; i < s; i++) {
        buff[i * 2]     = hex_table[src[i] >> 4];
        buff[i * 2 + 1] = hex_table[src[i] & 0xF];
    }
    buff[2 * s] = '\0';

    return buff;
}

int ff_hex_to_data(uint8_t *data, const char *p)
{
    int c, len, v;

    len = 0;
    v   = 1;
    for (;;) {
        p += strspn(p, SPACE_CHARS);
        if (*p == '\0')
            break;
        c = av_toupper((unsigned char) *p++);
        if (c >= '0' && c <= '9')
            c = c - '0';
        else if (c >= 'A' && c <= 'F')
            c = c - 'A' + 10;
        else
            break;
        v = (v << 4) | c;
        if (v & 0x100) {
            if (data)
                data[len] = v;
            len++;
            v = 1;
        }
    }
    return len;
}

void avpriv_set_pts_info(AVStream *st, int pts_wrap_bits,
                         unsigned int pts_num, unsigned int pts_den)
{
    FFStream *const sti = ffstream(st);
    AVRational new_tb;
    if (av_reduce(&new_tb.num, &new_tb.den, pts_num, pts_den, INT_MAX)) {
        if (new_tb.num != pts_num)
            av_log(NULL, AV_LOG_DEBUG,
                   "st:%d removing common factor %d from timebase\n",
                   st->index, pts_num / new_tb.num);
    } else
        av_log(NULL, AV_LOG_WARNING,
               "st:%d has too large timebase, reducing\n", st->index);

    if (new_tb.num <= 0 || new_tb.den <= 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Ignoring attempt to set invalid timebase %d/%d for st:%d\n",
               new_tb.num, new_tb.den,
               st->index);
        return;
    }
    st->time_base     = new_tb;
    sti->avctx->pkt_timebase = new_tb;
    st->pts_wrap_bits = pts_wrap_bits;
}

void ff_parse_key_value(const char *str, ff_parse_key_val_cb callback_get_buf,
                        void *context)
{
    const char *ptr = str;

    /* Parse key=value pairs. */
    for (;;) {
        const char *key;
        char *dest = NULL, *dest_end;
        int key_len, dest_len = 0;

        /* Skip whitespace and potential commas. */
        while (*ptr && (av_isspace(*ptr) || *ptr == ','))
            ptr++;
        if (!*ptr)
            break;

        key = ptr;

        if (!(ptr = strchr(key, '=')))
            break;
        ptr++;
        key_len = ptr - key;

        callback_get_buf(context, key, key_len, &dest, &dest_len);
        dest_end = dest ? dest + dest_len - 1 : NULL;

        if (*ptr == '\"') {
            ptr++;
            while (*ptr && *ptr != '\"') {
                if (*ptr == '\\') {
                    if (!ptr[1])
                        break;
                    if (dest && dest < dest_end)
                        *dest++ = ptr[1];
                    ptr += 2;
                } else {
                    if (dest && dest < dest_end)
                        *dest++ = *ptr;
                    ptr++;
                }
            }
            if (*ptr == '\"')
                ptr++;
        } else {
            for (; *ptr && !(av_isspace(*ptr) || *ptr == ','); ptr++)
                if (dest && dest < dest_end)
                    *dest++ = *ptr;
        }
        if (dest)
            *dest = 0;
    }
}

int ff_find_stream_index(const AVFormatContext *s, int id)
{
    for (unsigned i = 0; i < s->nb_streams; i++)
        if (s->streams[i]->id == id)
            return i;
    return -1;
}

int avformat_network_init(void)
{
#if CONFIG_NETWORK
    int ret;
    if ((ret = ff_network_init()) < 0)
        return ret;
    if ((ret = ff_tls_init()) < 0)
        return ret;
#endif
    return 0;
}

int avformat_network_deinit(void)
{
#if CONFIG_NETWORK
    ff_network_close();
    ff_tls_deinit();
#endif
    return 0;
}

AVRational av_guess_sample_aspect_ratio(AVFormatContext *format, AVStream *stream, AVFrame *frame)
{
    AVRational undef = {0, 1};
    AVRational stream_sample_aspect_ratio = stream ? stream->sample_aspect_ratio : undef;
    AVRational codec_sample_aspect_ratio  = stream && stream->codecpar ? stream->codecpar->sample_aspect_ratio : undef;
    AVRational frame_sample_aspect_ratio  = frame  ? frame->sample_aspect_ratio  : codec_sample_aspect_ratio;

    av_reduce(&stream_sample_aspect_ratio.num, &stream_sample_aspect_ratio.den,
               stream_sample_aspect_ratio.num,  stream_sample_aspect_ratio.den, INT_MAX);
    if (stream_sample_aspect_ratio.num <= 0 || stream_sample_aspect_ratio.den <= 0)
        stream_sample_aspect_ratio = undef;

    av_reduce(&frame_sample_aspect_ratio.num, &frame_sample_aspect_ratio.den,
               frame_sample_aspect_ratio.num,  frame_sample_aspect_ratio.den, INT_MAX);
    if (frame_sample_aspect_ratio.num <= 0 || frame_sample_aspect_ratio.den <= 0)
        frame_sample_aspect_ratio = undef;

    if (stream_sample_aspect_ratio.num)
        return stream_sample_aspect_ratio;
    else
        return frame_sample_aspect_ratio;
}

AVRational av_guess_frame_rate(AVFormatContext *format, AVStream *st, AVFrame *frame)
{
    AVRational fr = st->r_frame_rate;
    AVCodecContext *const avctx = ffstream(st)->avctx;
    AVRational codec_fr = avctx->framerate;
    AVRational   avg_fr = st->avg_frame_rate;

    if (avg_fr.num > 0 && avg_fr.den > 0 && fr.num > 0 && fr.den > 0 &&
        av_q2d(avg_fr) < 70 && av_q2d(fr) > 210) {
        fr = avg_fr;
    }


    if (avctx->ticks_per_frame > 1) {
        if (   codec_fr.num > 0 && codec_fr.den > 0 &&
            (fr.num == 0 || av_q2d(codec_fr) < av_q2d(fr)*0.7 && fabs(1.0 - av_q2d(av_div_q(avg_fr, fr))) > 0.1))
            fr = codec_fr;
    }

    return fr;
}

/**
 * Matches a stream specifier (but ignores requested index).
 *
 * @param indexptr set to point to the requested stream index if there is one
 *
 * @return <0 on error
 *         0  if st is NOT a matching stream
 *         >0 if st is a matching stream
 */
static int match_stream_specifier(const AVFormatContext *s, const AVStream *st,
                                  const char *spec, const char **indexptr,
                                  const AVProgram **p)
{
    int match = 1;                      /* Stores if the specifier matches so far. */
    while (*spec) {
        if (*spec <= '9' && *spec >= '0') { /* opt:index */
            if (indexptr)
                *indexptr = spec;
            return match;
        } else if (*spec == 'v' || *spec == 'a' || *spec == 's' || *spec == 'd' ||
                   *spec == 't' || *spec == 'V') { /* opt:[vasdtV] */
            enum AVMediaType type;
            int nopic = 0;

            switch (*spec++) {
            case 'v': type = AVMEDIA_TYPE_VIDEO;      break;
            case 'a': type = AVMEDIA_TYPE_AUDIO;      break;
            case 's': type = AVMEDIA_TYPE_SUBTITLE;   break;
            case 'd': type = AVMEDIA_TYPE_DATA;       break;
            case 't': type = AVMEDIA_TYPE_ATTACHMENT; break;
            case 'V': type = AVMEDIA_TYPE_VIDEO; nopic = 1; break;
            default:  av_assert0(0);
            }
            if (*spec && *spec++ != ':')         /* If we are not at the end, then another specifier must follow. */
                return AVERROR(EINVAL);

            if (type != st->codecpar->codec_type)
                match = 0;
            if (nopic && (st->disposition & AV_DISPOSITION_ATTACHED_PIC))
                match = 0;
        } else if (*spec == 'p' && *(spec + 1) == ':') {
            int prog_id;
            int found = 0;
            char *endptr;
            spec += 2;
            prog_id = strtol(spec, &endptr, 0);
            /* Disallow empty id and make sure that if we are not at the end, then another specifier must follow. */
            if (spec == endptr || (*endptr && *endptr++ != ':'))
                return AVERROR(EINVAL);
            spec = endptr;
            if (match) {
                for (unsigned i = 0; i < s->nb_programs; i++) {
                    if (s->programs[i]->id != prog_id)
                        continue;

                    for (unsigned j = 0; j < s->programs[i]->nb_stream_indexes; j++) {
                        if (st->index == s->programs[i]->stream_index[j]) {
                            found = 1;
                            if (p)
                                *p = s->programs[i];
                            i = s->nb_programs;
                            break;
                        }
                    }
                }
            }
            if (!found)
                match = 0;
        } else if (*spec == '#' ||
                   (*spec == 'i' && *(spec + 1) == ':')) {
            int stream_id;
            char *endptr;
            spec += 1 + (*spec == 'i');
            stream_id = strtol(spec, &endptr, 0);
            if (spec == endptr || *endptr)                /* Disallow empty id and make sure we are at the end. */
                return AVERROR(EINVAL);
            return match && (stream_id == st->id);
        } else if (*spec == 'm' && *(spec + 1) == ':') {
            const AVDictionaryEntry *tag;
            char *key, *val;
            int ret;

            if (match) {
                spec += 2;
                val = strchr(spec, ':');

                key = val ? av_strndup(spec, val - spec) : av_strdup(spec);
                if (!key)
                    return AVERROR(ENOMEM);

                tag = av_dict_get(st->metadata, key, NULL, 0);
                if (tag) {
                    if (!val || !strcmp(tag->value, val + 1))
                        ret = 1;
                    else
                        ret = 0;
                } else
                    ret = 0;

                av_freep(&key);
            }
            return match && ret;
        } else if (*spec == 'u' && *(spec + 1) == '\0') {
            const AVCodecParameters *par = st->codecpar;
            int val;
            switch (par->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                val = par->sample_rate && par->ch_layout.nb_channels;
                if (par->format == AV_SAMPLE_FMT_NONE)
                    return 0;
                break;
            case AVMEDIA_TYPE_VIDEO:
                val = par->width && par->height;
                if (par->format == AV_PIX_FMT_NONE)
                    return 0;
                break;
            case AVMEDIA_TYPE_UNKNOWN:
                val = 0;
                break;
            default:
                val = 1;
                break;
            }
            return match && (par->codec_id != AV_CODEC_ID_NONE && val != 0);
        } else {
            return AVERROR(EINVAL);
        }
    }

    return match;
}


int avformat_match_stream_specifier(AVFormatContext *s, AVStream *st,
                                    const char *spec)
{
    int ret, index;
    char *endptr;
    const char *indexptr = NULL;
    const AVProgram *p = NULL;
    int nb_streams;

    ret = match_stream_specifier(s, st, spec, &indexptr, &p);
    if (ret < 0)
        goto error;

    if (!indexptr)
        return ret;

    index = strtol(indexptr, &endptr, 0);
    if (*endptr) {                  /* We can't have anything after the requested index. */
        ret = AVERROR(EINVAL);
        goto error;
    }

    /* This is not really needed but saves us a loop for simple stream index specifiers. */
    if (spec == indexptr)
        return (index == st->index);

    /* If we requested a matching stream index, we have to ensure st is that. */
    nb_streams = p ? p->nb_stream_indexes : s->nb_streams;
    for (int i = 0; i < nb_streams && index >= 0; i++) {
        const AVStream *candidate = s->streams[p ? p->stream_index[i] : i];
        ret = match_stream_specifier(s, candidate, spec, NULL, NULL);
        if (ret < 0)
            goto error;
        if (ret > 0 && index-- == 0 && st == candidate)
            return 1;
    }
    return 0;

error:
    if (ret == AVERROR(EINVAL))
        av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return ret;
}

uint8_t *av_stream_get_side_data(const AVStream *st,
                                 enum AVPacketSideDataType type, size_t *size)
{
    for (int i = 0; i < st->nb_side_data; i++) {
        if (st->side_data[i].type == type) {
            if (size)
                *size = st->side_data[i].size;
            return st->side_data[i].data;
        }
    }
    if (size)
        *size = 0;
    return NULL;
}

int av_stream_add_side_data(AVStream *st, enum AVPacketSideDataType type,
                            uint8_t *data, size_t size)
{
    AVPacketSideData *sd, *tmp;

    for (int i = 0; i < st->nb_side_data; i++) {
        sd = &st->side_data[i];

        if (sd->type == type) {
            av_freep(&sd->data);
            sd->data = data;
            sd->size = size;
            return 0;
        }
    }

    if (st->nb_side_data + 1U > FFMIN(INT_MAX, SIZE_MAX / sizeof(*tmp)))
        return AVERROR(ERANGE);

    tmp = av_realloc_array(st->side_data, st->nb_side_data + 1, sizeof(*tmp));
    if (!tmp) {
        return AVERROR(ENOMEM);
    }

    st->side_data = tmp;
    st->nb_side_data++;

    sd = &st->side_data[st->nb_side_data - 1];
    sd->type = type;
    sd->data = data;
    sd->size = size;

    return 0;
}

uint8_t *av_stream_new_side_data(AVStream *st, enum AVPacketSideDataType type,
                                 size_t size)
{
    int ret;
    uint8_t *data = av_malloc(size);

    if (!data)
        return NULL;

    ret = av_stream_add_side_data(st, type, data, size);
    if (ret < 0) {
        av_freep(&data);
        return NULL;
    }

    return data;
}

void ff_format_io_close_default(AVFormatContext *s, AVIOContext *pb)
{
    avio_close(pb);
}

int ff_format_io_close(AVFormatContext *s, AVIOContext **pb)
{
    int ret = 0;
    if (*pb) {
        if (s->io_close == ff_format_io_close_default || s->io_close == NULL)
            ret = s->io_close2(s, *pb);
        else
            s->io_close(s, *pb);
    }
    *pb = NULL;
    return ret;
}

int ff_is_http_proto(const char *filename) {
    const char *proto = avio_find_protocol_name(filename);
    return proto ? (!av_strcasecmp(proto, "http") || !av_strcasecmp(proto, "https")) : 0;
}

int ff_bprint_to_codecpar_extradata(AVCodecParameters *par, struct AVBPrint *buf)
{
    int ret;
    char *str;

    ret = av_bprint_finalize(buf, &str);
    if (ret < 0)
        return ret;
    if (!av_bprint_is_complete(buf)) {
        av_free(str);
        return AVERROR(ENOMEM);
    }

    par->extradata = str;
    /* Note: the string is NUL terminated (so extradata can be read as a
     * string), but the ending character is not accounted in the size (in
     * binary formats you are likely not supposed to mux that character). When
     * extradata is copied, it is also padded with AV_INPUT_BUFFER_PADDING_SIZE
     * zeros. */
    par->extradata_size = buf->len;
    return 0;
}

int avformat_transfer_internal_stream_timing_info(const AVOutputFormat *ofmt,
                                                  AVStream *ost, const AVStream *ist,
                                                  enum AVTimebaseSource copy_tb)
{
    const AVCodecContext *const dec_ctx = cffstream(ist)->avctx;
    AVCodecContext       *const enc_ctx =  ffstream(ost)->avctx;

    enc_ctx->time_base = ist->time_base;
    /*
     * Avi is a special case here because it supports variable fps but
     * having the fps and timebase differe significantly adds quite some
     * overhead
     */
    if (!strcmp(ofmt->name, "avi")) {
#if FF_API_R_FRAME_RATE
        if (copy_tb == AVFMT_TBCF_AUTO && ist->r_frame_rate.num
            && av_q2d(ist->r_frame_rate) >= av_q2d(ist->avg_frame_rate)
            && 0.5/av_q2d(ist->r_frame_rate) > av_q2d(ist->time_base)
            && 0.5/av_q2d(ist->r_frame_rate) > av_q2d(dec_ctx->time_base)
            && av_q2d(ist->time_base) < 1.0/500 && av_q2d(dec_ctx->time_base) < 1.0/500
            || copy_tb == AVFMT_TBCF_R_FRAMERATE) {
            enc_ctx->time_base.num = ist->r_frame_rate.den;
            enc_ctx->time_base.den = 2*ist->r_frame_rate.num;
            enc_ctx->ticks_per_frame = 2;
        } else
#endif
            if (copy_tb == AVFMT_TBCF_AUTO && av_q2d(dec_ctx->time_base)*dec_ctx->ticks_per_frame > 2*av_q2d(ist->time_base)
                   && av_q2d(ist->time_base) < 1.0/500
                   || copy_tb == AVFMT_TBCF_DECODER) {
            enc_ctx->time_base = dec_ctx->time_base;
            enc_ctx->time_base.num *= dec_ctx->ticks_per_frame;
            enc_ctx->time_base.den *= 2;
            enc_ctx->ticks_per_frame = 2;
        }
    } else if (!(ofmt->flags & AVFMT_VARIABLE_FPS)
               && !av_match_name(ofmt->name, "mov,mp4,3gp,3g2,psp,ipod,ismv,f4v")) {
        if (copy_tb == AVFMT_TBCF_AUTO && dec_ctx->time_base.den
            && av_q2d(dec_ctx->time_base)*dec_ctx->ticks_per_frame > av_q2d(ist->time_base)
            && av_q2d(ist->time_base) < 1.0/500
            || copy_tb == AVFMT_TBCF_DECODER) {
            enc_ctx->time_base = dec_ctx->time_base;
            enc_ctx->time_base.num *= dec_ctx->ticks_per_frame;
        }
    }

    if ((enc_ctx->codec_tag == AV_RL32("tmcd") || ost->codecpar->codec_tag == AV_RL32("tmcd"))
        && dec_ctx->time_base.num < dec_ctx->time_base.den
        && dec_ctx->time_base.num > 0
        && 121LL*dec_ctx->time_base.num > dec_ctx->time_base.den) {
        enc_ctx->time_base = dec_ctx->time_base;
    }

    av_reduce(&enc_ctx->time_base.num, &enc_ctx->time_base.den,
              enc_ctx->time_base.num, enc_ctx->time_base.den, INT_MAX);

    return 0;
}

AVRational av_stream_get_codec_timebase(const AVStream *st)
{
    // See avformat_transfer_internal_stream_timing_info() TODO.
    return cffstream(st)->avctx->time_base;
}

void ff_format_set_url(AVFormatContext *s, char *url)
{
    av_assert0(url);
    av_freep(&s->url);
    s->url = url;
}
