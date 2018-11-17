/*
 * Copyright (c) 2018 Paul B Mahol
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

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"

#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "framesync.h"
#include "video.h"

typedef struct ChromaShiftContext {
    const AVClass *class;
    int cbh, cbv;
    int crh, crv;
    int edge;

    int depth;
    int height[4];
    int width[4];
    int linesize[4];

    AVFrame *in;

    int (*filter_slice)(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs);
} ChromaShiftContext;

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pix_fmts[] = {
        AV_PIX_FMT_YUVA444P, AV_PIX_FMT_YUVA422P, AV_PIX_FMT_YUVA420P,
        AV_PIX_FMT_YUVJ444P, AV_PIX_FMT_YUVJ440P, AV_PIX_FMT_YUVJ422P,AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_YUVJ411P,
        AV_PIX_FMT_YUV444P, AV_PIX_FMT_YUV440P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV411P, AV_PIX_FMT_YUV410P,
        AV_PIX_FMT_YUV420P9, AV_PIX_FMT_YUV422P9, AV_PIX_FMT_YUV444P9,
        AV_PIX_FMT_YUV420P10, AV_PIX_FMT_YUV422P10, AV_PIX_FMT_YUV444P10, AV_PIX_FMT_YUV440P10,
        AV_PIX_FMT_YUVA420P10, AV_PIX_FMT_YUVA422P10, AV_PIX_FMT_YUVA444P10,
        AV_PIX_FMT_YUV420P12, AV_PIX_FMT_YUV422P12, AV_PIX_FMT_YUV444P12, AV_PIX_FMT_YUV440P12,
        AV_PIX_FMT_YUV444P14, AV_PIX_FMT_YUV422P14, AV_PIX_FMT_YUV420P14,
        AV_PIX_FMT_YUV420P16, AV_PIX_FMT_YUV422P16, AV_PIX_FMT_YUV444P16,
        AV_PIX_FMT_YUVA420P16, AV_PIX_FMT_YUVA422P16, AV_PIX_FMT_YUVA444P16,
        AV_PIX_FMT_NONE
    };

    AVFilterFormats *fmts_list = ff_make_format_list(pix_fmts);
    if (!fmts_list)
        return AVERROR(ENOMEM);
    return ff_set_common_formats(ctx, fmts_list);
}

#define DEFINE_SMEAR(depth, type, div)                                                    \
static int smear_slice ## depth(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs)  \
{                                                                                         \
    ChromaShiftContext *s = ctx->priv;                                                    \
    AVFrame *in = s->in;                                                                  \
    AVFrame *out = arg;                                                                   \
    const int sulinesize = in->linesize[1] / div;                                         \
    const int svlinesize = in->linesize[2] / div;                                         \
    const int ulinesize = out->linesize[1] / div;                                         \
    const int vlinesize = out->linesize[2] / div;                                         \
    const int cbh = s->cbh;                                                               \
    const int cbv = s->cbv;                                                               \
    const int crh = s->crh;                                                               \
    const int crv = s->crv;                                                               \
    const int h = s->height[1];                                                           \
    const int w = s->width[1];                                                            \
    const int slice_start = (h * jobnr) / nb_jobs;                                        \
    const int slice_end = (h * (jobnr+1)) / nb_jobs;                                      \
    const type *su = (const type *)in->data[1];                                           \
    const type *sv = (const type *)in->data[2];                                           \
    type *du = (type *)out->data[1] + slice_start * ulinesize;                            \
    type *dv = (type *)out->data[2] + slice_start * vlinesize;                            \
                                                                                          \
    for (int y = slice_start; y < slice_end; y++) {                                       \
        const int duy = av_clip(y - cbv, 0, h-1) * sulinesize;                            \
        const int dvy = av_clip(y - crv, 0, h-1) * svlinesize;                            \
                                                                                          \
        for (int x = 0; x < w; x++) {                                                     \
            du[x] = su[av_clip(x - cbh, 0, w - 1) + duy];                                 \
            dv[x] = sv[av_clip(x - crh, 0, w - 1) + dvy];                                 \
        }                                                                                 \
                                                                                          \
        du += ulinesize;                                                                  \
        dv += vlinesize;                                                                  \
    }                                                                                     \
                                                                                          \
    return 0;                                                                             \
}

DEFINE_SMEAR(8, uint8_t, 1)
DEFINE_SMEAR(16, uint16_t, 2)

#define DEFINE_WRAP(depth, type, div)                                                     \
static int wrap_slice ## depth(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs)   \
{                                                                                         \
    ChromaShiftContext *s = ctx->priv;                                                    \
    AVFrame *in = s->in;                                                                  \
    AVFrame *out = arg;                                                                   \
    const int sulinesize = in->linesize[1] / div;                                         \
    const int svlinesize = in->linesize[2] / div;                                         \
    const int ulinesize = out->linesize[1] / div;                                         \
    const int vlinesize = out->linesize[2] / div;                                         \
    const int cbh = s->cbh;                                                               \
    const int cbv = s->cbv;                                                               \
    const int crh = s->crh;                                                               \
    const int crv = s->crv;                                                               \
    const int h = s->height[1];                                                           \
    const int w = s->width[1];                                                            \
    const int slice_start = (h * jobnr) / nb_jobs;                                        \
    const int slice_end = (h * (jobnr+1)) / nb_jobs;                                      \
    const type *su = (const type *)in->data[1];                                           \
    const type *sv = (const type *)in->data[2];                                           \
    type *du = (type *)out->data[1] + slice_start * ulinesize;                            \
    type *dv = (type *)out->data[2] + slice_start * vlinesize;                            \
                                                                                          \
    for (int y = slice_start; y < slice_end; y++) {                                       \
        int uy = (y - cbv) % h;                                                           \
        int vy = (y - crv) % h;                                                           \
                                                                                          \
        if (uy < 0)                                                                       \
            uy += h;                                                                      \
        if (vy < 0)                                                                       \
            vy += h;                                                                      \
                                                                                          \
        for (int x = 0; x < w; x++) {                                                     \
            int ux = (x - cbh) % w;                                                       \
            int vx = (x - crh) % w;                                                       \
                                                                                          \
            if (ux < 0)                                                                   \
                ux += w;                                                                  \
            if (vx < 0)                                                                   \
                vx += w;                                                                  \
                                                                                          \
            du[x] = su[ux + uy * sulinesize];                                             \
            dv[x] = sv[vx + vy * svlinesize];                                             \
        }                                                                                 \
                                                                                          \
        du += ulinesize;                                                                  \
        dv += vlinesize;                                                                  \
    }                                                                                     \
                                                                                          \
    return 0;                                                                             \
}

DEFINE_WRAP(8, uint8_t, 1)
DEFINE_WRAP(16, uint16_t, 2)

static int filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    AVFilterLink *outlink = ctx->outputs[0];
    ChromaShiftContext *s = ctx->priv;
    AVFrame *out;

    out = ff_get_video_buffer(outlink, outlink->w, outlink->h);
    if (!out) {
        av_frame_free(&in);
        return AVERROR(ENOMEM);
    }
    av_frame_copy_props(out, in);

    s->in = in;
    av_image_copy_plane(out->data[0] + out->linesize[0],
                        out->linesize[0],
                        in->data[0], in->linesize[0],
                        s->linesize[0], s->height[0]);
    ctx->internal->execute(ctx, s->filter_slice, out, NULL,
                           FFMIN3(s->height[1],
                                  s->height[2],
                                  ff_filter_get_nb_threads(ctx)));
    s->in = NULL;
    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
}

static int config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    ChromaShiftContext *s = ctx->priv;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(inlink->format);

    s->depth = desc->comp[0].depth;
    if (s->edge)
        s->filter_slice = s->depth > 8 ? wrap_slice16 : wrap_slice8;
    else
        s->filter_slice = s->depth > 8 ? smear_slice16 : smear_slice8;
    s->height[1] = s->height[2] = AV_CEIL_RSHIFT(inlink->h, desc->log2_chroma_h);
    s->height[0] = s->height[3] = inlink->h;
    s->width[1] = s->width[2] = AV_CEIL_RSHIFT(inlink->w, desc->log2_chroma_w);
    s->width[0] = s->width[3] = inlink->w;

    return av_image_fill_linesizes(s->linesize, inlink->format, inlink->w);
}

#define OFFSET(x) offsetof(ChromaShiftContext, x)
#define VF AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_FILTERING_PARAM

static const AVOption chromashift_options[] = {
    { "cbh", "shift chroma-blue horizontally", OFFSET(cbh),  AV_OPT_TYPE_INT,   {.i64=0}, -255, 255, .flags = VF },
    { "cbv", "shift chroma-blue vertically",   OFFSET(cbv),  AV_OPT_TYPE_INT,   {.i64=0}, -255, 255, .flags = VF },
    { "crh", "shift chroma-red horizontally",  OFFSET(crh),  AV_OPT_TYPE_INT,   {.i64=0}, -255, 255, .flags = VF },
    { "crv", "shift chroma-red vertically",    OFFSET(crv),  AV_OPT_TYPE_INT,   {.i64=0}, -255, 255, .flags = VF },
    { "edge", "set edge operation",            OFFSET(edge), AV_OPT_TYPE_INT,   {.i64=0},    0,   1, .flags = VF, "edge" },
    { "smear",                              0,            0, AV_OPT_TYPE_CONST, {.i64=0},    0,   0, .flags = VF, "edge" },
    { "wrap",                               0,            0, AV_OPT_TYPE_CONST, {.i64=1},    0,   0, .flags = VF, "edge" },
    { NULL },
};

static const AVFilterPad inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,
        .config_props = config_input,
    },
    { NULL }
};

static const AVFilterPad outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

AVFILTER_DEFINE_CLASS(chromashift);

AVFilter ff_vf_chromashift = {
    .name          = "chromashift",
    .description   = NULL_IF_CONFIG_SMALL("Shift chroma."),
    .priv_size     = sizeof(ChromaShiftContext),
    .priv_class    = &chromashift_class,
    .query_formats = query_formats,
    .outputs       = outputs,
    .inputs        = inputs,
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC | AVFILTER_FLAG_SLICE_THREADS,
};
