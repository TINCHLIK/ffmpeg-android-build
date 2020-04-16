/*
 * SSA/ASS common functions
 * Copyright (c) 2010  Aurelien Jacobs <aurel@gnuage.org>
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
#include "ass.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/common.h"

int ff_ass_subtitle_header_full(AVCodecContext *avctx,
                                int play_res_x, int play_res_y,
                                const char *font, int font_size,
                                int primary_color, int secondary_color,
                                int outline_color, int back_color,
                                int bold, int italic, int underline,
                                int border_style, int alignment)
{
    avctx->subtitle_header = av_asprintf(
             "[Script Info]\r\n"
             "; Script generated by FFmpeg/Lavc%s\r\n"
             "ScriptType: v4.00+\r\n"
             "PlayResX: %d\r\n"
             "PlayResY: %d\r\n"
             "ScaledBorderAndShadow: yes\r\n"
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
             play_res_x, play_res_y, font, font_size,
             primary_color, secondary_color, outline_color, back_color,
             -bold, -italic, -underline, border_style, alignment);

    if (!avctx->subtitle_header)
        return AVERROR(ENOMEM);
    avctx->subtitle_header_size = strlen(avctx->subtitle_header);
    return 0;
}

int ff_ass_subtitle_header(AVCodecContext *avctx,
                           const char *font, int font_size,
                           int color, int back_color,
                           int bold, int italic, int underline,
                           int border_style, int alignment)
{
    return ff_ass_subtitle_header_full(avctx,
                               ASS_DEFAULT_PLAYRESX, ASS_DEFAULT_PLAYRESY,
                               font, font_size, color, color,
                               back_color, back_color,
                               bold, italic, underline,
                               border_style, alignment);
}

int ff_ass_subtitle_header_default(AVCodecContext *avctx)
{
    return ff_ass_subtitle_header(avctx, ASS_DEFAULT_FONT,
                               ASS_DEFAULT_FONT_SIZE,
                               ASS_DEFAULT_COLOR,
                               ASS_DEFAULT_BACK_COLOR,
                               ASS_DEFAULT_BOLD,
                               ASS_DEFAULT_ITALIC,
                               ASS_DEFAULT_UNDERLINE,
                               ASS_DEFAULT_BORDERSTYLE,
                               ASS_DEFAULT_ALIGNMENT);
}

char *ff_ass_get_dialog(int readorder, int layer, const char *style,
                        const char *speaker, const char *text)
{
    return av_asprintf("%d,%d,%s,%s,0,0,0,,%s",
                       readorder, layer, style ? style : "Default",
                       speaker ? speaker : "", text);
}

int ff_ass_add_rect(AVSubtitle *sub, const char *dialog,
                    int readorder, int layer, const char *style,
                    const char *speaker)
{
    char *ass_str;
    AVSubtitleRect **rects;

    rects = av_realloc_array(sub->rects, sub->num_rects+1, sizeof(*sub->rects));
    if (!rects)
        return AVERROR(ENOMEM);
    sub->rects = rects;
    rects[sub->num_rects]       = av_mallocz(sizeof(*rects[0]));
    if (!rects[sub->num_rects])
        return AVERROR(ENOMEM);
    rects[sub->num_rects]->type = SUBTITLE_ASS;
    ass_str = ff_ass_get_dialog(readorder, layer, style, speaker, dialog);
    if (!ass_str)
        return AVERROR(ENOMEM);
    rects[sub->num_rects]->ass = ass_str;
    sub->num_rects++;
    return 0;
}

void ff_ass_decoder_flush(AVCodecContext *avctx)
{
    FFASSDecoderContext *s = avctx->priv_data;
    if (!(avctx->flags2 & AV_CODEC_FLAG2_RO_FLUSH_NOOP))
        s->readorder = 0;
}

void ff_ass_bprint_text_event(AVBPrint *buf, const char *p, int size,
                             const char *linebreaks, int keep_ass_markup)
{
    const char *p_end = p + size;

    for (; p < p_end && *p; p++) {

        /* forced custom line breaks, not accounted as "normal" EOL */
        if (linebreaks && strchr(linebreaks, *p)) {
            av_bprintf(buf, "\\N");

        /* standard ASS escaping so random characters don't get mis-interpreted
         * as ASS */
        } else if (!keep_ass_markup && strchr("{}\\", *p)) {
            av_bprintf(buf, "\\%c", *p);

        /* some packets might end abruptly (no \0 at the end, like for example
         * in some cases of demuxing from a classic video container), some
         * might be terminated with \n or \r\n which we have to remove (for
         * consistency with those who haven't), and we also have to deal with
         * evil cases such as \r at the end of the buffer (and no \0 terminated
         * character) */
        } else if (p[0] == '\n') {
            /* some stuff left so we can insert a line break */
            if (p < p_end - 1)
                av_bprintf(buf, "\\N");
        } else if (p[0] == '\r' && p < p_end - 1 && p[1] == '\n') {
            /* \r followed by a \n, we can skip it. We don't insert the \N yet
             * because we don't know if it is followed by more text */
            continue;

        /* finally, a sane character */
        } else {
            av_bprint_chars(buf, *p, 1);
        }
    }
}
