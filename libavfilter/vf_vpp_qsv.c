/*
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 ** @file
 ** Hardware accelerated common filters based on Intel Quick Sync Video VPP
 **/

#include <float.h>

#include "libavutil/opt.h"
#include "libavutil/eval.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/mathematics.h"

#include "formats.h"
#include "internal.h"
#include "avfilter.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

#include "qsvvpp.h"

#define OFFSET(x) offsetof(VPPContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM

/* number of video enhancement filters */
#define ENH_FILTERS_COUNT (5)

typedef struct VPPContext{
    const AVClass *class;

    QSVVPPContext *qsv;

    /* Video Enhancement Algorithms */
    mfxExtVPPDeinterlacing  deinterlace_conf;
    mfxExtVPPFrameRateConversion frc_conf;
    mfxExtVPPDenoise denoise_conf;
    mfxExtVPPDetail detail_conf;
    mfxExtVPPProcAmp procamp_conf;

    int out_width;
    int out_height;

    AVRational framerate;       /* target framerate */
    int use_frc;                /* use framerate conversion */
    int deinterlace;            /* deinterlace mode : 0=off, 1=bob, 2=advanced */
    int denoise;                /* Enable Denoise algorithm. Value [0, 100] */
    int detail;                 /* Enable Detail Enhancement algorithm. */
                                /* Level is the optional, value [0, 100] */
    int use_crop;               /* 1 = use crop; 0=none */
    int crop_w;
    int crop_h;
    int crop_x;
    int crop_y;

    /* param for the procamp */
    int    procamp;            /* enable procamp */
    float  hue;
    float  saturation;
    float  contrast;
    float  brightness;

    char *cx, *cy, *cw, *ch;
    char *ow, *oh;
} VPPContext;

static const AVOption options[] = {
    { "deinterlace", "deinterlace mode: 0=off, 1=bob, 2=advanced", OFFSET(deinterlace), AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, MFX_DEINTERLACING_ADVANCED, .flags = FLAGS, "deinterlace" },
    { "bob",         "Bob deinterlace mode.",                      0,                   AV_OPT_TYPE_CONST,    { .i64 = MFX_DEINTERLACING_BOB },            .flags = FLAGS, "deinterlace" },
    { "advanced",    "Advanced deinterlace mode. ",                0,                   AV_OPT_TYPE_CONST,    { .i64 = MFX_DEINTERLACING_ADVANCED },       .flags = FLAGS, "deinterlace" },

    { "denoise",     "denoise level [0, 100]",       OFFSET(denoise),     AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, 100, .flags = FLAGS },
    { "detail",      "enhancement level [0, 100]",   OFFSET(detail),      AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, 100, .flags = FLAGS },
    { "framerate",   "output framerate",             OFFSET(framerate),   AV_OPT_TYPE_RATIONAL, { .dbl = 0.0 },0, DBL_MAX, .flags = FLAGS },
    { "procamp",     "Enable ProcAmp",               OFFSET(procamp),     AV_OPT_TYPE_INT,      { .i64 = 0 }, 0, 1, .flags = FLAGS},
    { "hue",         "ProcAmp hue",                  OFFSET(hue),         AV_OPT_TYPE_FLOAT,    { .dbl = 0.0 }, -180.0, 180.0, .flags = FLAGS},
    { "saturation",  "ProcAmp saturation",           OFFSET(saturation),  AV_OPT_TYPE_FLOAT,    { .dbl = 1.0 }, 0.0, 10.0, .flags = FLAGS},
    { "contrast",    "ProcAmp contrast",             OFFSET(contrast),    AV_OPT_TYPE_FLOAT,    { .dbl = 1.0 }, 0.0, 10.0, .flags = FLAGS},
    { "brightness",  "ProcAmp brightness",           OFFSET(brightness),  AV_OPT_TYPE_FLOAT,    { .dbl = 0.0 }, -100.0, 100.0, .flags = FLAGS},

    { "cw",   "set the width crop area expression",   OFFSET(cw), AV_OPT_TYPE_STRING, { .str = "iw" }, CHAR_MIN, CHAR_MAX, FLAGS },
    { "ch",   "set the height crop area expression",  OFFSET(ch), AV_OPT_TYPE_STRING, { .str = "ih" }, CHAR_MIN, CHAR_MAX, FLAGS },
    { "cx",   "set the x crop area expression",       OFFSET(cx), AV_OPT_TYPE_STRING, { .str = "(in_w-out_w)/2" }, CHAR_MIN, CHAR_MAX, FLAGS },
    { "cy",   "set the y crop area expression",       OFFSET(cy), AV_OPT_TYPE_STRING, { .str = "(in_h-out_h)/2" }, CHAR_MIN, CHAR_MAX, FLAGS },

    { "w",      "Output video width",  OFFSET(ow), AV_OPT_TYPE_STRING, { .str="cw" }, 0, 255, .flags = FLAGS },
    { "width",  "Output video width",  OFFSET(ow), AV_OPT_TYPE_STRING, { .str="cw" }, 0, 255, .flags = FLAGS },
    { "h",      "Output video height", OFFSET(oh), AV_OPT_TYPE_STRING, { .str="w*ch/cw" }, 0, 255, .flags = FLAGS },
    { "height", "Output video height", OFFSET(oh), AV_OPT_TYPE_STRING, { .str="w*ch/cw" }, 0, 255, .flags = FLAGS },
    { NULL }
};

static const char *const var_names[] = {
    "iw", "in_w",
    "ih", "in_h",
    "ow", "out_w", "w",
    "oh", "out_h", "h",
    "cw",
    "ch",
    "cx",
    "cy",
    NULL
};

enum var_name {
    VAR_iW, VAR_IN_W,
    VAR_iH, VAR_IN_H,
    VAR_oW, VAR_OUT_W, VAR_W,
    VAR_oH, VAR_OUT_H, VAR_H,
    CW,
    CH,
    CX,
    CY,
    VAR_VARS_NB
};

static int eval_expr(AVFilterContext *ctx)
{
#define PASS_EXPR(e, s) {\
    ret = av_expr_parse(&e, s, var_names, NULL, NULL, NULL, NULL, 0, ctx); \
    if (ret < 0) {\
        av_log(ctx, AV_LOG_ERROR, "Error when passing '%s'.\n", s);\
        goto release;\
    }\
}
#define CALC_EXPR(e, v, i) {\
    i = v = av_expr_eval(e, var_values, NULL); \
}
    VPPContext *vpp = ctx->priv;
    double  var_values[VAR_VARS_NB] = { NAN };
    AVExpr *w_expr  = NULL, *h_expr  = NULL;
    AVExpr *cw_expr = NULL, *ch_expr = NULL;
    AVExpr *cx_expr = NULL, *cy_expr = NULL;
    int     ret = 0;

    PASS_EXPR(cw_expr, vpp->cw);
    PASS_EXPR(ch_expr, vpp->ch);

    PASS_EXPR(w_expr, vpp->ow);
    PASS_EXPR(h_expr, vpp->oh);

    PASS_EXPR(cx_expr, vpp->cx);
    PASS_EXPR(cy_expr, vpp->cy);

    var_values[VAR_iW] =
    var_values[VAR_IN_W] = ctx->inputs[0]->w;

    var_values[VAR_iH] =
    var_values[VAR_IN_H] = ctx->inputs[0]->h;

    /* crop params */
    CALC_EXPR(cw_expr, var_values[CW], vpp->crop_w);
    CALC_EXPR(ch_expr, var_values[CH], vpp->crop_h);

    /* calc again in case cw is relative to ch */
    CALC_EXPR(cw_expr, var_values[CW], vpp->crop_w);

    CALC_EXPR(w_expr,
            var_values[VAR_OUT_W] = var_values[VAR_oW] = var_values[VAR_W],
            vpp->out_width);
    CALC_EXPR(h_expr,
            var_values[VAR_OUT_H] = var_values[VAR_oH] = var_values[VAR_H],
            vpp->out_height);

    /* calc again in case ow is relative to oh */
    CALC_EXPR(w_expr,
            var_values[VAR_OUT_W] = var_values[VAR_oW] = var_values[VAR_W],
            vpp->out_width);


    CALC_EXPR(cx_expr, var_values[CX], vpp->crop_x);
    CALC_EXPR(cy_expr, var_values[CY], vpp->crop_y);

    /* calc again in case cx is relative to cy */
    CALC_EXPR(cx_expr, var_values[CX], vpp->crop_x);

    if ((vpp->crop_w != var_values[VAR_iW]) || (vpp->crop_h != var_values[VAR_iH]))
        vpp->use_crop = 1;

release:
    av_expr_free(w_expr);
    av_expr_free(h_expr);
    av_expr_free(cw_expr);
    av_expr_free(ch_expr);
    av_expr_free(cx_expr);
    av_expr_free(cy_expr);
#undef PASS_EXPR
#undef CALC_EXPR

    return ret;
}

static int config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    VPPContext      *vpp = ctx->priv;
    int              ret;

    if (vpp->framerate.den == 0 || vpp->framerate.num == 0)
        vpp->framerate = inlink->frame_rate;

    if (av_cmp_q(vpp->framerate, inlink->frame_rate))
        vpp->use_frc = 1;

    ret = eval_expr(ctx);
    if (ret != 0) {
        av_log(ctx, AV_LOG_ERROR, "Fail to eval expr.\n");
        return ret;
    }

    if (vpp->out_height == 0 || vpp->out_width == 0) {
        vpp->out_width  = inlink->w;
        vpp->out_height = inlink->h;
    }

    if (vpp->use_crop) {
        vpp->crop_x = FFMAX(vpp->crop_x, 0);
        vpp->crop_y = FFMAX(vpp->crop_y, 0);

        if(vpp->crop_w + vpp->crop_x > inlink->w)
           vpp->crop_x = inlink->w - vpp->crop_w;
        if(vpp->crop_h + vpp->crop_y > inlink->h)
           vpp->crop_y = inlink->h - vpp->crop_h;
    }

    return 0;
}

static int config_output(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    VPPContext      *vpp = ctx->priv;
    QSVVPPParam     param = { NULL };
    QSVVPPCrop      crop  = { 0 };
    mfxExtBuffer    *ext_buf[ENH_FILTERS_COUNT];
    AVFilterLink    *inlink = ctx->inputs[0];

    outlink->w          = vpp->out_width;
    outlink->h          = vpp->out_height;
    outlink->frame_rate = vpp->framerate;
    outlink->time_base  = av_inv_q(vpp->framerate);

    param.filter_frame  = NULL;
    param.out_sw_format = AV_PIX_FMT_NV12;
    param.num_ext_buf   = 0;
    param.ext_buf       = ext_buf;

    if (vpp->use_crop) {
        crop.in_idx = 0;
        crop.x = vpp->crop_x;
        crop.y = vpp->crop_y;
        crop.w = vpp->crop_w;
        crop.h = vpp->crop_h;

        param.num_crop = 1;
        param.crop     = &crop;
    }

    if (vpp->deinterlace) {
        memset(&vpp->deinterlace_conf, 0, sizeof(mfxExtVPPDeinterlacing));
        vpp->deinterlace_conf.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
        vpp->deinterlace_conf.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);
        vpp->deinterlace_conf.Mode = vpp->deinterlace == 1 ?
                                     MFX_DEINTERLACING_BOB : MFX_DEINTERLACING_ADVANCED;

        param.ext_buf[param.num_ext_buf++] = (mfxExtBuffer*)&vpp->deinterlace_conf;
    }

    if (vpp->use_frc) {
        memset(&vpp->frc_conf, 0, sizeof(mfxExtVPPFrameRateConversion));
        vpp->frc_conf.Header.BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        vpp->frc_conf.Header.BufferSz = sizeof(mfxExtVPPFrameRateConversion);
        vpp->frc_conf.Algorithm = MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;

        param.ext_buf[param.num_ext_buf++] = (mfxExtBuffer*)&vpp->frc_conf;
    }

    if (vpp->denoise) {
        memset(&vpp->denoise_conf, 0, sizeof(mfxExtVPPDenoise));
        vpp->denoise_conf.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
        vpp->denoise_conf.Header.BufferSz = sizeof(mfxExtVPPDenoise);
        vpp->denoise_conf.DenoiseFactor   = vpp->denoise;

        param.ext_buf[param.num_ext_buf++] = (mfxExtBuffer*)&vpp->denoise_conf;
    }

    if (vpp->detail) {
        memset(&vpp->detail_conf, 0, sizeof(mfxExtVPPDetail));
        vpp->detail_conf.Header.BufferId  = MFX_EXTBUFF_VPP_DETAIL;
        vpp->detail_conf.Header.BufferSz  = sizeof(mfxExtVPPDetail);
        vpp->detail_conf.DetailFactor = vpp->detail;

        param.ext_buf[param.num_ext_buf++] = (mfxExtBuffer*)&vpp->detail_conf;
    }

    if (vpp->procamp) {
        memset(&vpp->procamp_conf, 0, sizeof(mfxExtVPPProcAmp));
        vpp->procamp_conf.Header.BufferId  = MFX_EXTBUFF_VPP_PROCAMP;
        vpp->procamp_conf.Header.BufferSz  = sizeof(mfxExtVPPProcAmp);
        vpp->procamp_conf.Hue              = vpp->hue;
        vpp->procamp_conf.Saturation       = vpp->saturation;
        vpp->procamp_conf.Contrast         = vpp->contrast;
        vpp->procamp_conf.Brightness       = vpp->brightness;

        param.ext_buf[param.num_ext_buf++] = (mfxExtBuffer*)&vpp->procamp_conf;
    }

    if (vpp->use_frc || vpp->use_crop || vpp->deinterlace || vpp->denoise ||
        vpp->detail || vpp->procamp || inlink->w != outlink->w || inlink->h != outlink->h)
        return ff_qsvvpp_create(ctx, &vpp->qsv, &param);
    else {
        av_log(ctx, AV_LOG_VERBOSE, "qsv vpp pass through mode.\n");
        if (inlink->hw_frames_ctx)
            outlink->hw_frames_ctx = av_buffer_ref(inlink->hw_frames_ctx);
    }

    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *picref)
{
    int              ret = 0;
    AVFilterContext  *ctx = inlink->dst;
    VPPContext       *vpp = inlink->dst->priv;
    AVFilterLink     *outlink = ctx->outputs[0];

    if (vpp->qsv)
        ret = ff_qsvvpp_filter_frame(vpp->qsv, inlink, picref);
    else {
        if (picref->pts != AV_NOPTS_VALUE)
            picref->pts = av_rescale_q(picref->pts, inlink->time_base, outlink->time_base);
        ret = ff_filter_frame(outlink, picref);
    }

    return ret;
}

static int query_formats(AVFilterContext *ctx)
{
    AVFilterFormats *in_fmts, *out_fmts;
    static const enum AVPixelFormat in_pix_fmts[] = {
        AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_NV12,
        AV_PIX_FMT_YUYV422,
        AV_PIX_FMT_RGB32,
        AV_PIX_FMT_QSV,
        AV_PIX_FMT_NONE
    };
    static const enum AVPixelFormat out_pix_fmts[] = {
        AV_PIX_FMT_NV12,
        AV_PIX_FMT_QSV,
        AV_PIX_FMT_NONE
    };

    in_fmts  = ff_make_format_list(in_pix_fmts);
    out_fmts = ff_make_format_list(out_pix_fmts);
    ff_formats_ref(in_fmts, &ctx->inputs[0]->out_formats);
    ff_formats_ref(out_fmts, &ctx->outputs[0]->in_formats);

    return 0;
}

static av_cold void vpp_uninit(AVFilterContext *ctx)
{
    VPPContext *vpp = ctx->priv;

    ff_qsvvpp_free(&vpp->qsv);
}

static const AVClass vpp_class = {
    .class_name = "vpp_qsv",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVFilterPad vpp_inputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_input,
        .filter_frame  = filter_frame,
    },
    { NULL }
};

static const AVFilterPad vpp_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_output,
    },
    { NULL }
};

AVFilter ff_vf_vpp_qsv = {
    .name          = "vpp_qsv",
    .description   = NULL_IF_CONFIG_SMALL("Quick Sync Video VPP."),
    .priv_size     = sizeof(VPPContext),
    .query_formats = query_formats,
    .uninit        = vpp_uninit,
    .inputs        = vpp_inputs,
    .outputs       = vpp_outputs,
    .priv_class    = &vpp_class,
    .flags_internal = FF_FILTER_FLAG_HWFRAME_AWARE,
};
