/*
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2011 Mina Nagy Zaki
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
 * resampling audio filter
 */

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libswresample/swresample.h"
#include "avfilter.h"
#include "audio.h"
#include "filters.h"
#include "formats.h"
#include "internal.h"

typedef struct AResampleContext {
    const AVClass *class;
    int sample_rate_arg;
    double ratio;
    struct SwrContext *swr;
    int64_t next_pts;
    int more_data;
    int eof;
} AResampleContext;

static av_cold int preinit(AVFilterContext *ctx)
{
    AResampleContext *aresample = ctx->priv;

    aresample->next_pts = AV_NOPTS_VALUE;
    aresample->swr = swr_alloc();
    if (!aresample->swr)
        return AVERROR(ENOMEM);

    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    AResampleContext *aresample = ctx->priv;
    swr_free(&aresample->swr);
}

static int query_formats(AVFilterContext *ctx)
{
    AResampleContext *aresample = ctx->priv;
    enum AVSampleFormat out_format;
    AVChannelLayout out_layout = { 0 };
    int64_t out_rate;

    AVFilterLink *inlink  = ctx->inputs[0];
    AVFilterLink *outlink = ctx->outputs[0];

    AVFilterFormats        *in_formats, *out_formats;
    AVFilterFormats        *in_samplerates, *out_samplerates;
    AVFilterChannelLayouts *in_layouts, *out_layouts;
    int ret;

    if (aresample->sample_rate_arg > 0)
        av_opt_set_int(aresample->swr, "osr", aresample->sample_rate_arg, 0);
    av_opt_get_sample_fmt(aresample->swr, "osf", 0, &out_format);
    av_opt_get_int(aresample->swr, "osr", 0, &out_rate);

    in_formats      = ff_all_formats(AVMEDIA_TYPE_AUDIO);
    if ((ret = ff_formats_ref(in_formats, &inlink->outcfg.formats)) < 0)
        return ret;

    in_samplerates  = ff_all_samplerates();
    if ((ret = ff_formats_ref(in_samplerates, &inlink->outcfg.samplerates)) < 0)
        return ret;

    in_layouts      = ff_all_channel_counts();
    if ((ret = ff_channel_layouts_ref(in_layouts, &inlink->outcfg.channel_layouts)) < 0)
        return ret;

    if(out_rate > 0) {
        int ratelist[] = { out_rate, -1 };
        out_samplerates = ff_make_format_list(ratelist);
    } else {
        out_samplerates = ff_all_samplerates();
    }

    if ((ret = ff_formats_ref(out_samplerates, &outlink->incfg.samplerates)) < 0)
        return ret;

    if(out_format != AV_SAMPLE_FMT_NONE) {
        int formatlist[] = { out_format, -1 };
        out_formats = ff_make_format_list(formatlist);
    } else
        out_formats = ff_all_formats(AVMEDIA_TYPE_AUDIO);
    if ((ret = ff_formats_ref(out_formats, &outlink->incfg.formats)) < 0)
        return ret;

    av_opt_get_chlayout(aresample->swr, "ochl", 0, &out_layout);
    if (av_channel_layout_check(&out_layout)) {
        const AVChannelLayout layout_list[] = { out_layout, { 0 } };
        out_layouts = ff_make_channel_layout_list(layout_list);
    } else
        out_layouts = ff_all_channel_counts();
    av_channel_layout_uninit(&out_layout);

    return ff_channel_layouts_ref(out_layouts, &outlink->incfg.channel_layouts);
}


static int config_output(AVFilterLink *outlink)
{
    int ret;
    AVFilterContext *ctx = outlink->src;
    AVFilterLink *inlink = ctx->inputs[0];
    AResampleContext *aresample = ctx->priv;
    AVChannelLayout out_layout = { 0 };
    int64_t out_rate;
    enum AVSampleFormat out_format;
    char inchl_buf[128], outchl_buf[128];

    ret = swr_alloc_set_opts2(&aresample->swr,
                              &outlink->ch_layout, outlink->format, outlink->sample_rate,
                              &inlink->ch_layout, inlink->format, inlink->sample_rate,
                                         0, ctx);
    if (ret < 0)
        return ret;

    ret = swr_init(aresample->swr);
    if (ret < 0)
        return ret;

    av_opt_get_int(aresample->swr, "osr", 0, &out_rate);
    av_opt_get_chlayout(aresample->swr, "ochl", 0, &out_layout);
    av_opt_get_sample_fmt(aresample->swr, "osf", 0, &out_format);
    outlink->time_base = (AVRational) {1, out_rate};

    av_assert0(outlink->sample_rate == out_rate);
    av_assert0(!av_channel_layout_compare(&outlink->ch_layout, &out_layout));
    av_assert0(outlink->format == out_format);

    av_channel_layout_uninit(&out_layout);

    aresample->ratio = (double)outlink->sample_rate / inlink->sample_rate;

    av_channel_layout_describe(&inlink ->ch_layout, inchl_buf,  sizeof(inchl_buf));
    av_channel_layout_describe(&outlink->ch_layout, outchl_buf, sizeof(outchl_buf));

    av_log(ctx, AV_LOG_VERBOSE, "ch:%d chl:%s fmt:%s r:%dHz -> ch:%d chl:%s fmt:%s r:%dHz\n",
           inlink ->ch_layout.nb_channels, inchl_buf,  av_get_sample_fmt_name(inlink->format),  inlink->sample_rate,
           outlink->ch_layout.nb_channels, outchl_buf, av_get_sample_fmt_name(outlink->format), outlink->sample_rate);
    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *insamplesref)
{
    AVFilterContext *ctx = inlink->dst;
    AResampleContext *aresample = ctx->priv;
    const int n_in  = insamplesref->nb_samples;
    int64_t delay;
    int n_out       = n_in * aresample->ratio + 32;
    AVFilterLink *const outlink = inlink->dst->outputs[0];
    AVFrame *outsamplesref;
    int ret;

    delay = swr_get_delay(aresample->swr, outlink->sample_rate);
    if (delay > 0)
        n_out += FFMIN(delay, FFMAX(4096, n_out));

    outsamplesref = ff_get_audio_buffer(outlink, n_out);

    if(!outsamplesref) {
        av_frame_free(&insamplesref);
        return AVERROR(ENOMEM);
    }

    av_frame_copy_props(outsamplesref, insamplesref);
    outsamplesref->format                = outlink->format;
    ret = av_channel_layout_copy(&outsamplesref->ch_layout, &outlink->ch_layout);
    if (ret < 0) {
        av_frame_free(&outsamplesref);
        av_frame_free(&insamplesref);
        return ret;
    }
    outsamplesref->sample_rate           = outlink->sample_rate;

    if(insamplesref->pts != AV_NOPTS_VALUE) {
        int64_t inpts = av_rescale(insamplesref->pts, inlink->time_base.num * (int64_t)outlink->sample_rate * inlink->sample_rate, inlink->time_base.den);
        int64_t outpts= swr_next_pts(aresample->swr, inpts);
        aresample->next_pts =
        outsamplesref->pts  = ROUNDED_DIV(outpts, inlink->sample_rate);
    } else {
        outsamplesref->pts  = AV_NOPTS_VALUE;
    }
    n_out = swr_convert(aresample->swr, outsamplesref->extended_data, n_out,
                                 (void *)insamplesref->extended_data, n_in);
    if (n_out <= 0) {
        av_frame_free(&outsamplesref);
        av_frame_free(&insamplesref);
        ff_inlink_request_frame(inlink);
        return 0;
    }

    aresample->more_data = outsamplesref->nb_samples == n_out; // Indicate that there is probably more data in our buffers

    outsamplesref->nb_samples  = n_out;

    ret = ff_filter_frame(outlink, outsamplesref);
    av_frame_free(&insamplesref);
    return ret;
}

static int flush_frame(AVFilterLink *outlink, int final, AVFrame **outsamplesref_ret)
{
    AVFilterContext *ctx = outlink->src;
    AResampleContext *aresample = ctx->priv;
    AVFilterLink *const inlink = outlink->src->inputs[0];
    AVFrame *outsamplesref;
    int n_out = 4096;
    int64_t pts;

    outsamplesref = ff_get_audio_buffer(outlink, n_out);
    *outsamplesref_ret = outsamplesref;
    if (!outsamplesref)
        return AVERROR(ENOMEM);

    pts = swr_next_pts(aresample->swr, INT64_MIN);
    pts = ROUNDED_DIV(pts, inlink->sample_rate);

    n_out = swr_convert(aresample->swr, outsamplesref->extended_data, n_out, final ? NULL : (void*)outsamplesref->extended_data, 0);
    if (n_out <= 0) {
        av_frame_free(&outsamplesref);
        return (n_out == 0) ? AVERROR_EOF : n_out;
    }

    outsamplesref->sample_rate = outlink->sample_rate;
    outsamplesref->nb_samples  = n_out;

    outsamplesref->pts = pts;

    return 0;
}

static int request_frame(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    AVFilterLink *inlink = ctx->inputs[0];
    AResampleContext *aresample = ctx->priv;
    int ret = 0, status;
    int64_t pts;

    // First try to get data from the internal buffers
    if (aresample->more_data) {
        AVFrame *outsamplesref;

        if (flush_frame(outlink, 0, &outsamplesref) >= 0) {
            return ff_filter_frame(outlink, outsamplesref);
        }
    }
    aresample->more_data = 0;

    if (!aresample->eof && ff_inlink_acknowledge_status(inlink, &status, &pts))
        aresample->eof = 1;

    // Second request more data from the input
    if (!aresample->eof)
        FF_FILTER_FORWARD_WANTED(outlink, inlink);

    // Third if we hit the end flush
    if (aresample->eof) {
        AVFrame *outsamplesref;

        if ((ret = flush_frame(outlink, 1, &outsamplesref)) < 0) {
            if (ret == AVERROR_EOF) {
                ff_outlink_set_status(outlink, AVERROR_EOF, aresample->next_pts);
                return 0;
            }
            return ret;
        }

        return ff_filter_frame(outlink, outsamplesref);
    }

    ff_filter_set_ready(ctx, 100);
    return 0;
}

static int activate(AVFilterContext *ctx)
{
    AResampleContext *aresample = ctx->priv;
    AVFilterLink *inlink = ctx->inputs[0];
    AVFilterLink *outlink = ctx->outputs[0];

    FF_FILTER_FORWARD_STATUS_BACK(outlink, inlink);

    if (!aresample->eof && ff_inlink_queued_frames(inlink)) {
        AVFrame *frame = NULL;
        int ret;

        ret = ff_inlink_consume_frame(inlink, &frame);
        if (ret < 0)
            return ret;
        if (ret > 0)
            return filter_frame(inlink, frame);
    }

    return request_frame(outlink);
}

static const AVClass *resample_child_class_iterate(void **iter)
{
    const AVClass *c = *iter ? NULL : swr_get_class();
    *iter = (void*)(uintptr_t)c;
    return c;
}

static void *resample_child_next(void *obj, void *prev)
{
    AResampleContext *s = obj;
    return prev ? NULL : s->swr;
}

#define OFFSET(x) offsetof(AResampleContext, x)
#define FLAGS AV_OPT_FLAG_AUDIO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption options[] = {
    {"sample_rate", NULL, OFFSET(sample_rate_arg), AV_OPT_TYPE_INT, {.i64=0},  0,        INT_MAX, FLAGS },
    {NULL}
};

static const AVClass aresample_class = {
    .class_name       = "aresample",
    .item_name        = av_default_item_name,
    .option           = options,
    .version          = LIBAVUTIL_VERSION_INT,
    .child_class_iterate = resample_child_class_iterate,
    .child_next       = resample_child_next,
};

static const AVFilterPad aresample_outputs[] = {
    {
        .name          = "default",
        .config_props  = config_output,
        .type          = AVMEDIA_TYPE_AUDIO,
    },
};

const AVFilter ff_af_aresample = {
    .name          = "aresample",
    .description   = NULL_IF_CONFIG_SMALL("Resample audio data."),
    .preinit       = preinit,
    .activate      = activate,
    .uninit        = uninit,
    .priv_size     = sizeof(AResampleContext),
    .priv_class    = &aresample_class,
    FILTER_INPUTS(ff_audio_default_filterpad),
    FILTER_OUTPUTS(aresample_outputs),
    FILTER_QUERY_FUNC(query_formats),
};
