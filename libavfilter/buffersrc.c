/*
 * Copyright (c) 2008 Vitor Sessak
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

/**
 * @file
 * memory buffer source filter
 */

#include <float.h>

#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "audio.h"
#include "avfilter.h"
#include "buffersrc.h"
#include "filters.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

typedef struct BufferSourceContext {
    const AVClass    *class;
    AVRational        time_base;     ///< time_base to set in the output link
    AVRational        frame_rate;    ///< frame_rate to set in the output link
    unsigned          nb_failed_requests;

    /* video only */
    int               w, h;
    enum AVPixelFormat  pix_fmt;
    AVRational        pixel_aspect;

    AVBufferRef *hw_frames_ctx;

    /* audio only */
    int sample_rate;
    enum AVSampleFormat sample_fmt;
    int channels;
    char    *channel_layout_str;
    AVChannelLayout ch_layout;

    int eof;
    int64_t last_pts;
} BufferSourceContext;

#define CHECK_VIDEO_PARAM_CHANGE(s, c, width, height, format, pts)\
    if (c->w != width || c->h != height || c->pix_fmt != format) {\
        av_log(s, AV_LOG_INFO, "filter context - w: %d h: %d fmt: %d, incoming frame - w: %d h: %d fmt: %d pts_time: %s\n",\
               c->w, c->h, c->pix_fmt, width, height, format, av_ts2timestr(pts, &s->outputs[0]->time_base));\
        av_log(s, AV_LOG_WARNING, "Changing video frame properties on the fly is not supported by all filters.\n");\
    }

#define CHECK_AUDIO_PARAM_CHANGE(s, c, srate, layout, format, pts)\
    if (c->sample_fmt != format || c->sample_rate != srate ||\
        av_channel_layout_compare(&c->ch_layout, &layout) || c->channels != layout.nb_channels) {\
        av_log(s, AV_LOG_INFO, "filter context - fmt: %s r: %d layout: %"PRIX64" ch: %d, incoming frame - fmt: %s r: %d layout: %"PRIX64" ch: %d pts_time: %s\n",\
               av_get_sample_fmt_name(c->sample_fmt), c->sample_rate, c->ch_layout.order == AV_CHANNEL_ORDER_NATIVE ? c->ch_layout.u.mask : 0, c->channels,\
               av_get_sample_fmt_name(format), srate, layout.order == AV_CHANNEL_ORDER_NATIVE ? layout.u.mask : 0, layout.nb_channels, av_ts2timestr(pts, &s->outputs[0]->time_base));\
        av_log(s, AV_LOG_ERROR, "Changing audio frame properties on the fly is not supported.\n");\
        return AVERROR(EINVAL);\
    }

AVBufferSrcParameters *av_buffersrc_parameters_alloc(void)
{
    AVBufferSrcParameters *par = av_mallocz(sizeof(*par));
    if (!par)
        return NULL;

    par->format = -1;

    return par;
}

int av_buffersrc_parameters_set(AVFilterContext *ctx, AVBufferSrcParameters *param)
{
    BufferSourceContext *s = ctx->priv;

    if (param->time_base.num > 0 && param->time_base.den > 0)
        s->time_base = param->time_base;

    switch (ctx->filter->outputs[0].type) {
    case AVMEDIA_TYPE_VIDEO:
        if (param->format != AV_PIX_FMT_NONE) {
            s->pix_fmt = param->format;
        }
        if (param->width > 0)
            s->w = param->width;
        if (param->height > 0)
            s->h = param->height;
        if (param->sample_aspect_ratio.num > 0 && param->sample_aspect_ratio.den > 0)
            s->pixel_aspect = param->sample_aspect_ratio;
        if (param->frame_rate.num > 0 && param->frame_rate.den > 0)
            s->frame_rate = param->frame_rate;
        if (param->hw_frames_ctx) {
            av_buffer_unref(&s->hw_frames_ctx);
            s->hw_frames_ctx = av_buffer_ref(param->hw_frames_ctx);
            if (!s->hw_frames_ctx)
                return AVERROR(ENOMEM);
        }
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (param->format != AV_SAMPLE_FMT_NONE) {
            s->sample_fmt = param->format;
        }
        if (param->sample_rate > 0)
            s->sample_rate = param->sample_rate;
#if FF_API_OLD_CHANNEL_LAYOUT
FF_DISABLE_DEPRECATION_WARNINGS
        // if the old/new fields are set inconsistently, prefer the old ones
        if (param->channel_layout && (param->ch_layout.order != AV_CHANNEL_ORDER_NATIVE ||
                                      param->ch_layout.u.mask != param->channel_layout)) {
            av_channel_layout_uninit(&s->ch_layout);
            av_channel_layout_from_mask(&s->ch_layout, param->channel_layout);
FF_ENABLE_DEPRECATION_WARNINGS
        } else
#endif
        if (param->ch_layout.nb_channels) {
            int ret = av_channel_layout_copy(&s->ch_layout, &param->ch_layout);
            if (ret < 0)
                return ret;
        }
        break;
    default:
        return AVERROR_BUG;
    }

    return 0;
}

int attribute_align_arg av_buffersrc_write_frame(AVFilterContext *ctx, const AVFrame *frame)
{
    return av_buffersrc_add_frame_flags(ctx, (AVFrame *)frame,
                                        AV_BUFFERSRC_FLAG_KEEP_REF);
}

int attribute_align_arg av_buffersrc_add_frame(AVFilterContext *ctx, AVFrame *frame)
{
    return av_buffersrc_add_frame_flags(ctx, frame, 0);
}

static int push_frame(AVFilterGraph *graph)
{
    int ret;

    while (1) {
        ret = ff_filter_graph_run_once(graph);
        if (ret == AVERROR(EAGAIN))
            break;
        if (ret < 0)
            return ret;
    }
    return 0;
}

int attribute_align_arg av_buffersrc_add_frame_flags(AVFilterContext *ctx, AVFrame *frame, int flags)
{
    BufferSourceContext *s = ctx->priv;
    AVFrame *copy;
    int refcounted, ret;

#if FF_API_OLD_CHANNEL_LAYOUT
FF_DISABLE_DEPRECATION_WARNINGS
    if (frame && frame->channel_layout &&
        av_get_channel_layout_nb_channels(frame->channel_layout) != frame->channels) {
        av_log(ctx, AV_LOG_ERROR, "Layout indicates a different number of channels than actually present\n");
        return AVERROR(EINVAL);
    }
FF_ENABLE_DEPRECATION_WARNINGS
#endif

    s->nb_failed_requests = 0;

    if (!frame)
        return av_buffersrc_close(ctx, s->last_pts, flags);
    if (s->eof)
        return AVERROR_EOF;

    s->last_pts = frame->pts + frame->duration;

    refcounted = !!frame->buf[0];

    if (!(flags & AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT)) {

        switch (ctx->outputs[0]->type) {
        case AVMEDIA_TYPE_VIDEO:
            CHECK_VIDEO_PARAM_CHANGE(ctx, s, frame->width, frame->height,
                                     frame->format, frame->pts);
            break;
        case AVMEDIA_TYPE_AUDIO:
            /* For layouts unknown on input but known on link after negotiation. */
#if FF_API_OLD_CHANNEL_LAYOUT
FF_DISABLE_DEPRECATION_WARNINGS
            if (!frame->channel_layout)
                frame->channel_layout = s->ch_layout.order == AV_CHANNEL_ORDER_NATIVE ?
                                        s->ch_layout.u.mask : 0;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
            if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
                ret = av_channel_layout_copy(&frame->ch_layout, &s->ch_layout);
                if (ret < 0)
                    return ret;
            }
            CHECK_AUDIO_PARAM_CHANGE(ctx, s, frame->sample_rate, frame->ch_layout,
                                     frame->format, frame->pts);
            break;
        default:
            return AVERROR(EINVAL);
        }

    }

    if (refcounted && !(flags & AV_BUFFERSRC_FLAG_KEEP_REF)) {
        if (!(copy = av_frame_alloc()))
            return AVERROR(ENOMEM);
        av_frame_move_ref(copy, frame);
    } else {
        copy = av_frame_clone(frame);
        if (!copy)
            return AVERROR(ENOMEM);
    }

#if FF_API_PKT_DURATION
FF_DISABLE_DEPRECATION_WARNINGS
    if (copy->pkt_duration && copy->pkt_duration != copy->duration)
        copy->duration = copy->pkt_duration;
FF_ENABLE_DEPRECATION_WARNINGS
#endif

#if FF_API_INTERLACED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
    if (copy->interlaced_frame)
        copy->flags |= AV_FRAME_FLAG_INTERLACED;
    if (copy->top_field_first)
        copy->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
FF_ENABLE_DEPRECATION_WARNINGS
#endif

#if FF_API_FRAME_KEY
FF_DISABLE_DEPRECATION_WARNINGS
    if (copy->key_frame)
        copy->flags |= AV_FRAME_FLAG_KEY;
FF_ENABLE_DEPRECATION_WARNINGS
#endif

    ret = ff_filter_frame(ctx->outputs[0], copy);
    if (ret < 0)
        return ret;

    if ((flags & AV_BUFFERSRC_FLAG_PUSH)) {
        ret = push_frame(ctx->graph);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int av_buffersrc_close(AVFilterContext *ctx, int64_t pts, unsigned flags)
{
    BufferSourceContext *s = ctx->priv;

    s->eof = 1;
    ff_avfilter_link_set_in_status(ctx->outputs[0], AVERROR_EOF, pts);
    return (flags & AV_BUFFERSRC_FLAG_PUSH) ? push_frame(ctx->graph) : 0;
}

static av_cold int init_video(AVFilterContext *ctx)
{
    BufferSourceContext *c = ctx->priv;

    if (c->pix_fmt == AV_PIX_FMT_NONE) {
        av_log(ctx, AV_LOG_ERROR, "Unspecified pixel format\n");
        return AVERROR(EINVAL);
    }
    if (c->w <= 0 || c->h <= 0) {
        av_log(ctx, AV_LOG_ERROR, "Invalid size %dx%d\n", c->w, c->h);
        return AVERROR(EINVAL);
    }
    if (av_q2d(c->time_base) <= 0) {
        av_log(ctx, AV_LOG_ERROR, "Invalid time base %d/%d\n", c->time_base.num, c->time_base.den);
        return AVERROR(EINVAL);
    }

    av_log(ctx, AV_LOG_VERBOSE, "w:%d h:%d pixfmt:%s tb:%d/%d fr:%d/%d sar:%d/%d\n",
           c->w, c->h, av_get_pix_fmt_name(c->pix_fmt),
           c->time_base.num, c->time_base.den, c->frame_rate.num, c->frame_rate.den,
           c->pixel_aspect.num, c->pixel_aspect.den);

    return 0;
}

unsigned av_buffersrc_get_nb_failed_requests(AVFilterContext *buffer_src)
{
    return ((BufferSourceContext *)buffer_src->priv)->nb_failed_requests;
}

#define OFFSET(x) offsetof(BufferSourceContext, x)
#define A AV_OPT_FLAG_FILTERING_PARAM|AV_OPT_FLAG_AUDIO_PARAM
#define V AV_OPT_FLAG_FILTERING_PARAM|AV_OPT_FLAG_VIDEO_PARAM

static const AVOption buffer_options[] = {
    { "width",         NULL,                     OFFSET(w),                AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, INT_MAX, V },
    { "video_size",    NULL,                     OFFSET(w),                AV_OPT_TYPE_IMAGE_SIZE,                .flags = V },
    { "height",        NULL,                     OFFSET(h),                AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, INT_MAX, V },
    { "pix_fmt",       NULL,                     OFFSET(pix_fmt),          AV_OPT_TYPE_PIXEL_FMT, { .i64 = AV_PIX_FMT_NONE }, .min = AV_PIX_FMT_NONE, .max = INT_MAX, .flags = V },
    { "sar",           "sample aspect ratio",    OFFSET(pixel_aspect),     AV_OPT_TYPE_RATIONAL, { .dbl = 0 }, 0, DBL_MAX, V },
    { "pixel_aspect",  "sample aspect ratio",    OFFSET(pixel_aspect),     AV_OPT_TYPE_RATIONAL, { .dbl = 0 }, 0, DBL_MAX, V },
    { "time_base",     NULL,                     OFFSET(time_base),        AV_OPT_TYPE_RATIONAL, { .dbl = 0 }, 0, DBL_MAX, V },
    { "frame_rate",    NULL,                     OFFSET(frame_rate),       AV_OPT_TYPE_RATIONAL, { .dbl = 0 }, 0, DBL_MAX, V },
    { NULL },
};

AVFILTER_DEFINE_CLASS(buffer);

static const AVOption abuffer_options[] = {
    { "time_base",      NULL, OFFSET(time_base),           AV_OPT_TYPE_RATIONAL, { .dbl = 0 }, 0, INT_MAX, A },
    { "sample_rate",    NULL, OFFSET(sample_rate),         AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, INT_MAX, A },
    { "sample_fmt",     NULL, OFFSET(sample_fmt),          AV_OPT_TYPE_SAMPLE_FMT, { .i64 = AV_SAMPLE_FMT_NONE }, .min = AV_SAMPLE_FMT_NONE, .max = INT_MAX, .flags = A },
    { "channel_layout", NULL, OFFSET(channel_layout_str),  AV_OPT_TYPE_STRING,             .flags = A },
    { "channels",       NULL, OFFSET(channels),            AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, INT_MAX, A },
    { NULL },
};

AVFILTER_DEFINE_CLASS(abuffer);

static av_cold int init_audio(AVFilterContext *ctx)
{
    BufferSourceContext *s = ctx->priv;
    char buf[128];
    int ret = 0;

    if (s->sample_fmt == AV_SAMPLE_FMT_NONE) {
        av_log(ctx, AV_LOG_ERROR, "Sample format was not set or was invalid\n");
        return AVERROR(EINVAL);
    }

    if (s->channel_layout_str || s->ch_layout.nb_channels) {
        int n;

        if (!s->ch_layout.nb_channels) {
            ret = av_channel_layout_from_string(&s->ch_layout, s->channel_layout_str);
            if (ret < 0) {
#if FF_API_OLD_CHANNEL_LAYOUT
                uint64_t mask;
FF_DISABLE_DEPRECATION_WARNINGS
                mask = av_get_channel_layout(s->channel_layout_str);
                if (!mask) {
#endif
                    av_log(ctx, AV_LOG_ERROR, "Invalid channel layout %s.\n",
                           s->channel_layout_str);
                    return AVERROR(EINVAL);
#if FF_API_OLD_CHANNEL_LAYOUT
                }
FF_ENABLE_DEPRECATION_WARNINGS
                av_log(ctx, AV_LOG_WARNING, "Channel layout '%s' uses a deprecated syntax.\n",
                       s->channel_layout_str);
                av_channel_layout_from_mask(&s->ch_layout, mask);
#endif
            }
        }

        n = s->ch_layout.nb_channels;
        av_channel_layout_describe(&s->ch_layout, buf, sizeof(buf));
        if (s->channels) {
            if (n != s->channels) {
                av_log(ctx, AV_LOG_ERROR,
                       "Mismatching channel count %d and layout '%s' "
                       "(%d channels)\n",
                       s->channels, buf, n);
                return AVERROR(EINVAL);
            }
        }
        s->channels = n;
    } else if (!s->channels) {
        av_log(ctx, AV_LOG_ERROR, "Neither number of channels nor "
                                  "channel layout specified\n");
        return AVERROR(EINVAL);
    } else {
        s->ch_layout = FF_COUNT2LAYOUT(s->channels);
        av_channel_layout_describe(&s->ch_layout, buf, sizeof(buf));
    }

    if (!s->time_base.num)
        s->time_base = (AVRational){1, s->sample_rate};

    av_log(ctx, AV_LOG_VERBOSE,
           "tb:%d/%d samplefmt:%s samplerate:%d chlayout:%s\n",
           s->time_base.num, s->time_base.den, av_get_sample_fmt_name(s->sample_fmt),
           s->sample_rate, buf);

    return ret;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    BufferSourceContext *s = ctx->priv;
    av_buffer_unref(&s->hw_frames_ctx);
    av_channel_layout_uninit(&s->ch_layout);
}

static int query_formats(AVFilterContext *ctx)
{
    BufferSourceContext *c = ctx->priv;
    AVFilterChannelLayouts *channel_layouts = NULL;
    AVFilterFormats *formats = NULL;
    AVFilterFormats *samplerates = NULL;
    int ret;

    switch (ctx->outputs[0]->type) {
    case AVMEDIA_TYPE_VIDEO:
        if ((ret = ff_add_format         (&formats, c->pix_fmt)) < 0 ||
            (ret = ff_set_common_formats (ctx     , formats   )) < 0)
            return ret;
        break;
    case AVMEDIA_TYPE_AUDIO:
        if ((ret = ff_add_format             (&formats    , c->sample_fmt )) < 0 ||
            (ret = ff_set_common_formats     (ctx         , formats       )) < 0 ||
            (ret = ff_add_format             (&samplerates, c->sample_rate)) < 0 ||
            (ret = ff_set_common_samplerates (ctx         , samplerates   )) < 0)
            return ret;

        if ((ret = ff_add_channel_layout(&channel_layouts, &c->ch_layout)) < 0)
            return ret;
        if ((ret = ff_set_common_channel_layouts(ctx, channel_layouts)) < 0)
            return ret;
        break;
    default:
        return AVERROR(EINVAL);
    }

    return 0;
}

static int config_props(AVFilterLink *link)
{
    BufferSourceContext *c = link->src->priv;

    switch (link->type) {
    case AVMEDIA_TYPE_VIDEO:
        link->w = c->w;
        link->h = c->h;
        link->sample_aspect_ratio = c->pixel_aspect;

        if (c->hw_frames_ctx) {
            link->hw_frames_ctx = av_buffer_ref(c->hw_frames_ctx);
            if (!link->hw_frames_ctx)
                return AVERROR(ENOMEM);
        }
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (!c->ch_layout.nb_channels) {
            int ret = av_channel_layout_copy(&c->ch_layout, &link->ch_layout);
            if (ret < 0)
                return ret;
        }
        break;
    default:
        return AVERROR(EINVAL);
    }

    link->time_base = c->time_base;
    link->frame_rate = c->frame_rate;
    return 0;
}

static int activate(AVFilterContext *ctx)
{
    AVFilterLink *outlink = ctx->outputs[0];
    BufferSourceContext *c = ctx->priv;

    if (!c->eof && ff_outlink_get_status(outlink)) {
        c->eof = 1;
        return 0;
    }

    if (c->eof) {
        ff_outlink_set_status(outlink, AVERROR_EOF, c->last_pts);
        return 0;
    }
    c->nb_failed_requests++;
    return FFERROR_NOT_READY;
}

static const AVFilterPad avfilter_vsrc_buffer_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_props,
    },
};

const AVFilter ff_vsrc_buffer = {
    .name      = "buffer",
    .description = NULL_IF_CONFIG_SMALL("Buffer video frames, and make them accessible to the filterchain."),
    .priv_size = sizeof(BufferSourceContext),
    .activate  = activate,
    .init      = init_video,
    .uninit    = uninit,

    .inputs    = NULL,
    FILTER_OUTPUTS(avfilter_vsrc_buffer_outputs),
    FILTER_QUERY_FUNC(query_formats),
    .priv_class = &buffer_class,
};

static const AVFilterPad avfilter_asrc_abuffer_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_AUDIO,
        .config_props  = config_props,
    },
};

const AVFilter ff_asrc_abuffer = {
    .name          = "abuffer",
    .description   = NULL_IF_CONFIG_SMALL("Buffer audio frames, and make them accessible to the filterchain."),
    .priv_size     = sizeof(BufferSourceContext),
    .activate  = activate,
    .init      = init_audio,
    .uninit    = uninit,

    .inputs    = NULL,
    FILTER_OUTPUTS(avfilter_asrc_abuffer_outputs),
    FILTER_QUERY_FUNC(query_formats),
    .priv_class = &abuffer_class,
};
