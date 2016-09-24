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

#include "libavutil/internal.h"
#include "libavutil/opt.h"

#include "avcodec.h"
#include "internal.h"

#include "nvenc.h"

#define OFFSET(x) offsetof(NVENCContext, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "preset",   "Set the encoding preset",              OFFSET(preset),      AV_OPT_TYPE_INT,    { .i64 = PRESET_MEDIUM }, PRESET_DEFAULT, PRESET_LOSSLESS_HP, VE, "preset" },
    { "default",    "",                                   0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_DEFAULT }, 0, 0, VE, "preset" },
    { "slow",       "hq 2 passes",                        0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_SLOW }, 0, 0, VE, "preset" },
    { "medium",     "hq 1 pass",                          0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_MEDIUM }, 0, 0, VE, "preset" },
    { "fast",       "hp 1 pass",                          0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_FAST }, 0, 0, VE, "preset" },
    { "hp",         "",                                   0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_HP }, 0, 0, VE, "preset" },
    { "hq",         "",                                   0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_HQ }, 0, 0, VE, "preset" },
    { "bd",         "",                                   0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_BD }, 0, 0, VE, "preset" },
    { "ll",         "low latency",                        0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_LOW_LATENCY_DEFAULT }, 0, 0, VE, "preset" },
    { "llhq",       "low latency hq",                     0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_LOW_LATENCY_HQ }, 0, 0, VE, "preset" },
    { "llhp",       "low latency hp",                     0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_LOW_LATENCY_HP }, 0, 0, VE, "preset" },
    { "lossless",   NULL,                                 0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_LOSSLESS_DEFAULT }, 0, 0, VE, "preset" },
    { "losslesshp", NULL,                                 0,                   AV_OPT_TYPE_CONST,  { .i64 = PRESET_LOSSLESS_HP }, 0, 0, VE, "preset" },
    { "profile",  "Set the encoding profile",             OFFSET(profile),     AV_OPT_TYPE_INT,    { .i64 = NV_ENC_H264_PROFILE_HIGH }, NV_ENC_H264_PROFILE_BASELINE, NV_ENC_H264_PROFILE_CONSTRAINED_HIGH, VE, "profile" },
    { "baseline", "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_H264_PROFILE_BASELINE },            0, 0, VE, "profile" },
    { "main",     "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_H264_PROFILE_MAIN },                0, 0, VE, "profile" },
    { "high",     "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_H264_PROFILE_HIGH },                0, 0, VE, "profile" },
    { "high_444", "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_H264_PROFILE_HIGH_444 },            0, 0, VE, "profile" },
    { "constrained_high", "",                             0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_H264_PROFILE_CONSTRAINED_HIGH },    0, 0, VE, "profile" },
    { "level",    "Set the encoding level restriction",   OFFSET(level),       AV_OPT_TYPE_INT,    { .i64 = NV_ENC_LEVEL_AUTOSELECT }, NV_ENC_LEVEL_AUTOSELECT, NV_ENC_LEVEL_H264_51, VE, "level" },
    { "1.0",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_1 },  0, 0, VE,  "level" },
    { "1.b",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_1b }, 0, 0, VE,  "level" },
    { "1.1",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_11 }, 0, 0, VE,  "level" },
    { "1.2",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_12 }, 0, 0, VE,  "level" },
    { "1.3",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_13 }, 0, 0, VE,  "level" },
    { "2.0",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_2 },  0, 0, VE,  "level" },
    { "2.1",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_21 }, 0, 0, VE,  "level" },
    { "2.2",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_22 }, 0, 0, VE,  "level" },
    { "3.0",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_3 },  0, 0, VE,  "level" },
    { "3.1",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_31 }, 0, 0, VE,  "level" },
    { "3.2",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_32 }, 0, 0, VE,  "level" },
    { "4.0",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_4 },  0, 0, VE,  "level" },
    { "4.1",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_41 }, 0, 0, VE,  "level" },
    { "4.2",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_42 }, 0, 0, VE,  "level" },
    { "5.0",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_5 },  0, 0, VE,  "level" },
    { "5.1",      "",                                     0,                   AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_LEVEL_H264_51 }, 0, 0, VE,  "level" },
    { "rc",       "Override the preset rate-control",     OFFSET(rc),          AV_OPT_TYPE_INT,    { .i64 = -1 },                   -1, INT_MAX, VE, "rc" },
    { "constqp",          "Constant QP mode",                                                            0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_CONSTQP },              0, 0, VE, "rc" },
    { "vbr",              "Variable bitrate mode",                                                       0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_VBR },                  0, 0, VE, "rc" },
    { "cbr",              "Constant bitrate mode",                                                       0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_CBR },                  0, 0, VE, "rc" },
    { "vbr_minqp",        "Variable bitrate mode with MinQP",                                            0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_VBR_MINQP },            0, 0, VE, "rc" },
    { "ll_2pass_quality", "Multi-pass optimized for image quality (only for low-latency presets)",       0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_2_PASS_QUALITY },       0, 0, VE, "rc" },
    { "ll_2pass_size",    "Multi-pass optimized for constant frame size (only for low-latency presets)", 0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP }, 0, 0, VE, "rc" },
    { "vbr_2pass",        "Multi-pass variable bitrate mode",                                            0, AV_OPT_TYPE_CONST,  { .i64 = NV_ENC_PARAMS_RC_2_PASS_VBR },           0, 0, VE, "rc" },
    { "surfaces", "Number of concurrent surfaces",        OFFSET(nb_surfaces), AV_OPT_TYPE_INT,    { .i64 = 32 },                   0, INT_MAX, VE },
    { "device",   "Select a specific NVENC device",       OFFSET(device),      AV_OPT_TYPE_INT,    { .i64 = -1 },                   -2, INT_MAX, VE, "device" },
    { "any",      "Pick the first device available",      0,                   AV_OPT_TYPE_CONST,  { .i64 = ANY_DEVICE },           0, 0, VE, "device" },
    { "list",     "List the available devices",           0,                   AV_OPT_TYPE_CONST,  { .i64 = LIST_DEVICES },         0, 0, VE, "device" },
    { "async_depth", "Delay frame output by the given amount of frames", OFFSET(async_depth), AV_OPT_TYPE_INT, { .i64 = INT_MAX }, 0, INT_MAX, VE },
    { "delay",       "Delay frame output by the given amount of frames", OFFSET(async_depth), AV_OPT_TYPE_INT, { .i64 = INT_MAX }, 0, INT_MAX, VE },
#if NVENCAPI_MAJOR_VERSION >= 7
    { "rc-lookahead", "Number of frames to look ahead for rate-control", OFFSET(rc_lookahead), AV_OPT_TYPE_INT, { .i64 = -1 }, -1, INT_MAX, VE },
    { "no-scenecut", "When lookahead is enabled, set this to 1 to disable adaptive I-frame insertion at scene cuts", OFFSET(no_scenecut), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, VE },
    { "b_adapt", "When lookahead is enabled, set this to 0 to disable adaptive B-frame decision", OFFSET(b_adapt), AV_OPT_TYPE_INT, { .i64 = 1 }, 0, 1, VE },
    { "spatial-aq", "set to 1 to enable Spatial AQ", OFFSET(aq), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, VE },
    { "temporal-aq", "set to 1 to enable Temporal AQ",     OFFSET(temporal_aq),  AV_OPT_TYPE_INT,   { .i64 = 0                       }, 0, 1, VE        },
    { "zerolatency", "Set 1 to indicate zero latency operation (no reordering delay)", OFFSET(zerolatency), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, VE },
    { "nonref_p", "Set this to 1 to enable automatic insertion of non-reference P-frames", OFFSET(nonref_p), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, VE },
    { "strict_gop", "Set 1 to minimize GOP-to-GOP rate fluctuations", OFFSET(strict_gop), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, VE },
    { "aq-strength", "When Spatial AQ is enabled, this field is used to specify AQ strength. AQ strength scale is from 1 (low) - 15 (aggressive)", OFFSET(aq_strength), AV_OPT_TYPE_INT, { .i64 = 8 }, 1, 15, VE },
    { "cq", "Set target quality level (0 to 51, 0 means automatic) for constant quality mode in VBR rate control", OFFSET(quality), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 51, VE },
#endif /* NVENCAPI_MAJOR_VERSION >= 7 */
    { NULL }
};

static const AVClass nvenc_h264_class = {
    .class_name = "nvenc_h264",
    .item_name = av_default_item_name,
    .option = options,
    .version = LIBAVUTIL_VERSION_INT,
};

static const AVCodecDefault defaults[] = {
    { "b", "0" },
    { "qmin", "-1" },
    { "qmax", "-1" },
    { "qdiff", "-1" },
    { "qblur", "-1" },
    { "qcomp", "-1" },
    { NULL },
};

AVCodec ff_h264_nvenc_encoder = {
    .name           = "h264_nvenc",
    .long_name      = NULL_IF_CONFIG_SMALL("NVIDIA NVENC H.264 encoder"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_H264,
    .init           = ff_nvenc_encode_init,
    .encode2        = ff_nvenc_encode_frame,
    .close          = ff_nvenc_encode_close,
    .priv_data_size = sizeof(NVENCContext),
    .priv_class     = &nvenc_h264_class,
    .defaults       = defaults,
    .capabilities   = AV_CODEC_CAP_DELAY,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
    .pix_fmts       = ff_nvenc_pix_fmts,
};

#if FF_API_NVENC_OLD_NAME

static int nvenc_old_init(AVCodecContext *avctx)
{
    av_log(avctx, AV_LOG_WARNING, "This encoder is deprecated, use 'h264_nvenc' instead\n");
    return ff_nvenc_encode_init(avctx);
}

static const AVClass nvenc_h264_old_class = {
    .class_name = "nvenc_h264",
    .item_name = av_default_item_name,
    .option = options,
    .version = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_nvenc_h264_encoder = {
    .name           = "nvenc_h264",
    .long_name      = NULL_IF_CONFIG_SMALL("NVIDIA NVENC H.264 encoder"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_H264,
    .init           = nvenc_old_init,
    .encode2        = ff_nvenc_encode_frame,
    .close          = ff_nvenc_encode_close,
    .priv_data_size = sizeof(NVENCContext),
    .priv_class     = &nvenc_h264_old_class,
    .defaults       = defaults,
    .capabilities   = AV_CODEC_CAP_DELAY,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
    .pix_fmts       = ff_nvenc_pix_fmts,
};
#endif
