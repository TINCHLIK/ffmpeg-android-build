/*
 * ARIB STD-B24 caption decoder using the libaribb24 library
 * Copyright (c) 2019 Jan Ekström
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
#include "libavcodec/ass.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"

#include <aribb24/aribb24.h>
#include <aribb24/parser.h>
#include <aribb24/decoder.h>

typedef struct Libaribb24Context {
    AVClass *class;

    arib_instance_t *lib_instance;
    arib_parser_t *parser;
    arib_decoder_t *decoder;

    int read_order;

    char        *aribb24_base_path;
    unsigned int aribb24_skip_ruby;
} Libaribb24Context;

static unsigned int get_profile_font_size(int profile)
{
    switch (profile) {
    case FF_PROFILE_ARIB_PROFILE_A:
        return 36;
    case FF_PROFILE_ARIB_PROFILE_C:
        return 18;
    default:
        return 0;
    }
}

static void libaribb24_log(void *p, const char *msg)
{
    av_log((AVCodecContext *)p, AV_LOG_INFO, "%s\n", msg);
}

static int libaribb24_generate_ass_header(AVCodecContext *avctx)
{
    unsigned int plane_width = 0;
    unsigned int plane_height = 0;
    unsigned int font_size = 0;

    switch (avctx->profile) {
    case FF_PROFILE_ARIB_PROFILE_A:
        plane_width = 960;
        plane_height = 540;
        font_size = get_profile_font_size(avctx->profile);
        break;
    case FF_PROFILE_ARIB_PROFILE_C:
        plane_width = 320;
        plane_height = 180;
        font_size = get_profile_font_size(avctx->profile);
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Unknown or unsupported profile set!\n");
        return AVERROR(EINVAL);
    }

    avctx->subtitle_header = av_asprintf(
             "[Script Info]\r\n"
             "; Script generated by FFmpeg/Lavc%s\r\n"
             "ScriptType: v4.00+\r\n"
             "PlayResX: %d\r\n"
             "PlayResY: %d\r\n"
             "\r\n"
             "[V4+ Styles]\r\n"

             /* ASSv4 header */
             "Format: Name, "
             "Fontname, Fontsize, "
             "PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
             "Bold, Italic, Underline, StrikeOut, "
             "ScaleX, ScaleY, "
             "Spacing, Angle, "
             "BorderStyle, Outline, Shadow, "
             "Alignment, MarginL, MarginR, MarginV, "
             "Encoding\r\n"

             "Style: "
             "Default,"             /* Name */
             "%s,%d,"               /* Font{name,size} */
             "&H%x,&H%x,&H%x,&H%x," /* {Primary,Secondary,Outline,Back}Colour */
             "%d,%d,%d,0,"          /* Bold, Italic, Underline, StrikeOut */
             "100,100,"             /* Scale{X,Y} */
             "0,0,"                 /* Spacing, Angle */
             "%d,1,0,"              /* BorderStyle, Outline, Shadow */
             "%d,10,10,10,"         /* Alignment, Margin[LRV] */
             "0\r\n"                /* Encoding */

             "\r\n"
             "[Events]\r\n"
             "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n",
             !(avctx->flags & AV_CODEC_FLAG_BITEXACT) ? AV_STRINGIFY(LIBAVCODEC_VERSION) : "",
             plane_width, plane_height,
             ASS_DEFAULT_FONT, font_size, ASS_DEFAULT_COLOR,
             ASS_DEFAULT_COLOR, ASS_DEFAULT_BACK_COLOR, ASS_DEFAULT_BACK_COLOR,
             -ASS_DEFAULT_BOLD, -ASS_DEFAULT_ITALIC, -ASS_DEFAULT_UNDERLINE,
             ASS_DEFAULT_BORDERSTYLE, ASS_DEFAULT_ALIGNMENT);

    if (!avctx->subtitle_header)
        return AVERROR(ENOMEM);

    avctx->subtitle_header_size = strlen(avctx->subtitle_header);

    return 0;
}

static int libaribb24_init(AVCodecContext *avctx)
{
    Libaribb24Context *b24 = avctx->priv_data;
    void(* arib_dec_init)(arib_decoder_t* decoder) = NULL;
    int ret_code = AVERROR_EXTERNAL;

    if (!(b24->lib_instance = arib_instance_new(avctx))) {
        av_log(avctx, AV_LOG_ERROR, "Failed to initialize libaribb24!\n");
        goto init_fail;
    }

    if (b24->aribb24_base_path) {
        av_log(avctx, AV_LOG_INFO, "Setting the libaribb24 base path to '%s'\n",
               b24->aribb24_base_path);
        arib_set_base_path(b24->lib_instance, b24->aribb24_base_path);
    }

    arib_register_messages_callback(b24->lib_instance, libaribb24_log);

    if (!(b24->parser = arib_get_parser(b24->lib_instance))) {
        av_log(avctx, AV_LOG_ERROR, "Failed to initialize libaribb24 PES parser!\n");
        goto init_fail;
    }
    if (!(b24->decoder = arib_get_decoder(b24->lib_instance))) {
        av_log(avctx, AV_LOG_ERROR, "Failed to initialize libaribb24 decoder!\n");
        goto init_fail;
    }

    switch (avctx->profile) {
    case FF_PROFILE_ARIB_PROFILE_A:
        arib_dec_init = arib_initialize_decoder_a_profile;
        break;
    case FF_PROFILE_ARIB_PROFILE_C:
        arib_dec_init = arib_initialize_decoder_c_profile;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Unknown or unsupported profile set!\n");
        ret_code = AVERROR(EINVAL);
        goto init_fail;
    }

    arib_dec_init(b24->decoder);

    if (libaribb24_generate_ass_header(avctx) < 0) {
        ret_code = AVERROR(ENOMEM);
        goto init_fail;
    }

    return 0;

init_fail:
    if (b24->decoder)
        arib_finalize_decoder(b24->decoder);

    if (b24->lib_instance)
        arib_instance_destroy(b24->lib_instance);

    return ret_code;
}

static int libaribb24_close(AVCodecContext *avctx)
{
    Libaribb24Context *b24 = avctx->priv_data;

    if (b24->decoder)
        arib_finalize_decoder(b24->decoder);

    if (b24->lib_instance)
        arib_instance_destroy(b24->lib_instance);

    return 0;
}

#define RGB_TO_BGR(c) ((c & 0xff) << 16 | (c & 0xff00) | ((c >> 16) & 0xff))

static void libaribb24_handle_regions(AVCodecContext *avctx, AVSubtitle *sub)
{
    Libaribb24Context *b24 = avctx->priv_data;
    const arib_buf_region_t *region = arib_decoder_get_regions(b24->decoder);
    unsigned int profile_font_size = get_profile_font_size(avctx->profile);
    AVBPrint buf = { 0 };

    av_bprint_init(&buf, 0, AV_BPRINT_SIZE_UNLIMITED);

    while (region) {
        ptrdiff_t region_length = region->p_end - region->p_start;
        unsigned int ruby_region =
            region->i_fontheight == (profile_font_size / 2);

        // ASS requires us to make the colors BGR, so we convert here
        int foreground_bgr_color = RGB_TO_BGR(region->i_foreground_color);
        int background_bgr_color = RGB_TO_BGR(region->i_background_color);

        if (region_length < 0) {
            av_log(avctx, AV_LOG_ERROR, "Invalid negative region length!\n");
            break;
        }

        if (region_length == 0 || (ruby_region && b24->aribb24_skip_ruby)) {
            goto next_region;
        }

        // color and alpha
        if (foreground_bgr_color != ASS_DEFAULT_COLOR)
            av_bprintf(&buf, "{\\1c&H%06x&}", foreground_bgr_color);

        if (region->i_foreground_alpha != 0)
            av_bprintf(&buf, "{\\1a&H%02x&}", region->i_foreground_alpha);

        if (background_bgr_color != ASS_DEFAULT_BACK_COLOR)
            av_bprintf(&buf, "{\\3c&H%06x&}", background_bgr_color);

        if (region->i_background_alpha != 0)
            av_bprintf(&buf, "{\\3a&H%02x&}", region->i_background_alpha);

        // font size
        if (region->i_fontwidth  != profile_font_size ||
            region->i_fontheight != profile_font_size) {
            av_bprintf(&buf, "{\\fscx%d\\fscy%d}",
                       (int)round(((double)region->i_fontwidth /
                                   (double)profile_font_size) * 100),
                       (int)round(((double)region->i_fontheight /
                                   (double)profile_font_size) * 100));
        }

        // TODO: positioning

        av_bprint_append_data(&buf, region->p_start, region_length);

        av_bprintf(&buf, "{\\r}");

next_region:
        region = region->p_next;
    }

    av_log(avctx, AV_LOG_DEBUG, "Styled ASS line: %s\n",
           buf.str);
    ff_ass_add_rect(sub, buf.str, b24->read_order++,
                    0, NULL, NULL);

    av_bprint_finalize(&buf, NULL);
}

static int libaribb24_decode(AVCodecContext *avctx, void *data, int *got_sub_ptr, AVPacket *pkt)
{
    Libaribb24Context *b24 = avctx->priv_data;
    AVSubtitle *sub = data;
    size_t parsed_data_size = 0;
    size_t decoded_subtitle_size = 0;
    const unsigned char *parsed_data = NULL;
    char *decoded_subtitle = NULL;
    time_t subtitle_duration = 0;

    if (pkt->size <= 0)
        return pkt->size;

    arib_parse_pes(b24->parser, pkt->data, pkt->size);

    parsed_data = arib_parser_get_data(b24->parser,
                                       &parsed_data_size);
    if (!parsed_data || !parsed_data_size) {
        av_log(avctx, AV_LOG_DEBUG, "No decode'able data was received from "
                                    "packet (dts: %"PRId64", pts: %"PRId64").\n",
               pkt->dts, pkt->pts);
        return pkt->size;
    }

    decoded_subtitle_size = parsed_data_size * 4;
    if (!(decoded_subtitle = av_mallocz(decoded_subtitle_size + 1))) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to allocate buffer for decoded subtitle!\n");
        return AVERROR(ENOMEM);
    }

    decoded_subtitle_size = arib_decode_buffer(b24->decoder,
                                               parsed_data,
                                               parsed_data_size,
                                               decoded_subtitle,
                                               decoded_subtitle_size);

    subtitle_duration = arib_decoder_get_time(b24->decoder);

    if (avctx->pkt_timebase.num && pkt->pts != AV_NOPTS_VALUE)
        sub->pts = av_rescale_q(pkt->pts,
                                avctx->pkt_timebase, AV_TIME_BASE_Q);

    sub->end_display_time = subtitle_duration ?
                            av_rescale_q(subtitle_duration,
                                         AV_TIME_BASE_Q,
                                         (AVRational){1, 1000}) :
                            UINT32_MAX;

    av_log(avctx, AV_LOG_DEBUG,
           "Result: '%s' (size: %zu, pkt_pts: %"PRId64", sub_pts: %"PRId64" "
           "duration: %"PRIu32", pkt_timebase: %d/%d, time_base: %d/%d')\n",
           decoded_subtitle ? decoded_subtitle : "<no subtitle>",
           decoded_subtitle_size,
           pkt->pts, sub->pts,
           sub->end_display_time,
           avctx->pkt_timebase.num, avctx->pkt_timebase.den,
           avctx->time_base.num, avctx->time_base.den);

    if (decoded_subtitle)
        libaribb24_handle_regions(avctx, sub);

    *got_sub_ptr = sub->num_rects > 0;

    av_free(decoded_subtitle);

    // flush the region buffers, otherwise the linked list keeps getting
    // longer and longer...
    arib_finalize_decoder(b24->decoder);

    return pkt->size;
}

static void libaribb24_flush(AVCodecContext *avctx)
{
    Libaribb24Context *b24 = avctx->priv_data;
    if (!(avctx->flags2 & AV_CODEC_FLAG2_RO_FLUSH_NOOP))
        b24->read_order = 0;
}

#define OFFSET(x) offsetof(Libaribb24Context, x)
#define SD AV_OPT_FLAG_SUBTITLE_PARAM | AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
    { "aribb24-base-path", "set the base path for the libaribb24 library",
      OFFSET(aribb24_base_path), AV_OPT_TYPE_STRING, { 0 }, 0, 0, SD },
    { "aribb24-skip-ruby-text", "skip ruby text blocks during decoding",
      OFFSET(aribb24_skip_ruby), AV_OPT_TYPE_BOOL, { 1 }, 0, 1, SD },
    { NULL }
};

static const AVClass aribb24_class = {
    .class_name = "libaribb24 decoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_libaribb24_decoder = {
    .name      = "libaribb24",
    .long_name = NULL_IF_CONFIG_SMALL("libaribb24 ARIB STD-B24 caption decoder"),
    .type      = AVMEDIA_TYPE_SUBTITLE,
    .id        = AV_CODEC_ID_ARIB_CAPTION,
    .priv_data_size = sizeof(Libaribb24Context),
    .init      = libaribb24_init,
    .close     = libaribb24_close,
    .decode    = libaribb24_decode,
    .flush     = libaribb24_flush,
    .priv_class= &aribb24_class,
    .wrapper_name = "libaribb24",
};
