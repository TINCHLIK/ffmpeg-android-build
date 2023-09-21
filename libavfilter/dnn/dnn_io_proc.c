/*
 * Copyright (c) 2020
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

#include "dnn_io_proc.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libavutil/avassert.h"
#include "libavutil/detection_bbox.h"

int ff_proc_from_dnn_to_frame(AVFrame *frame, DNNData *output, void *log_ctx)
{
    struct SwsContext *sws_ctx;
    int ret = 0;
    int linesize[4] = { 0 };
    void **dst_data = NULL;
    void *middle_data = NULL;
    uint8_t *planar_data[4] = { 0 };
    int plane_size = frame->width * frame->height * sizeof(uint8_t);
    int bytewidth = av_image_get_linesize(frame->format, frame->width, 0);
    if (bytewidth < 0) {
        return AVERROR(EINVAL);
    }
    if (output->dt != DNN_FLOAT) {
        avpriv_report_missing_feature(log_ctx, "data type rather than DNN_FLOAT");
        return AVERROR(ENOSYS);
    }
    dst_data = (void **)frame->data;
    linesize[0] = frame->linesize[0];
    if (output->layout == DL_NCHW) {
        middle_data = av_malloc(plane_size * output->channels);
        if (!middle_data) {
            ret = AVERROR(ENOMEM);
            goto err;
        }
        dst_data = &middle_data;
        linesize[0] = frame->width * 3;
    }

    switch (frame->format) {
    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_BGR24:
        sws_ctx = sws_getContext(frame->width * 3,
                                 frame->height,
                                 AV_PIX_FMT_GRAYF32,
                                 frame->width * 3,
                                 frame->height,
                                 AV_PIX_FMT_GRAY8,
                                 0, NULL, NULL, NULL);
        if (!sws_ctx) {
            av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(AV_PIX_FMT_GRAYF32), frame->width * 3, frame->height,
                av_get_pix_fmt_name(AV_PIX_FMT_GRAY8),   frame->width * 3, frame->height);
            ret = AVERROR(EINVAL);
            goto err;
        }
        sws_scale(sws_ctx, (const uint8_t *[4]){(const uint8_t *)output->data, 0, 0, 0},
                           (const int[4]){frame->width * 3 * sizeof(float), 0, 0, 0}, 0, frame->height,
                           (uint8_t * const*)dst_data, linesize);
        sws_freeContext(sws_ctx);
        // convert data from planar to packed
        if (output->layout == DL_NCHW) {
            sws_ctx = sws_getContext(frame->width,
                                     frame->height,
                                     AV_PIX_FMT_GBRP,
                                     frame->width,
                                     frame->height,
                                     frame->format,
                                     0, NULL, NULL, NULL);
            if (!sws_ctx) {
                av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
                       "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                       av_get_pix_fmt_name(AV_PIX_FMT_GBRP), frame->width, frame->height,
                       av_get_pix_fmt_name(frame->format),frame->width, frame->height);
                ret = AVERROR(EINVAL);
                goto err;
            }
            if (frame->format == AV_PIX_FMT_RGB24) {
                planar_data[0] = (uint8_t *)middle_data + plane_size;
                planar_data[1] = (uint8_t *)middle_data + plane_size * 2;
                planar_data[2] = (uint8_t *)middle_data;
            } else if (frame->format == AV_PIX_FMT_BGR24) {
                planar_data[0] = (uint8_t *)middle_data + plane_size;
                planar_data[1] = (uint8_t *)middle_data;
                planar_data[2] = (uint8_t *)middle_data + plane_size * 2;
            }
            sws_scale(sws_ctx, (const uint8_t * const *)planar_data,
                      (const int [4]){frame->width * sizeof(uint8_t),
                                      frame->width * sizeof(uint8_t),
                                      frame->width * sizeof(uint8_t), 0},
                      0, frame->height, frame->data, frame->linesize);
            sws_freeContext(sws_ctx);
        }
        break;
    case AV_PIX_FMT_GRAYF32:
        av_image_copy_plane(frame->data[0], frame->linesize[0],
                            output->data, bytewidth,
                            bytewidth, frame->height);
        break;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUV410P:
    case AV_PIX_FMT_YUV411P:
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_NV12:
        sws_ctx = sws_getContext(frame->width,
                                 frame->height,
                                 AV_PIX_FMT_GRAYF32,
                                 frame->width,
                                 frame->height,
                                 AV_PIX_FMT_GRAY8,
                                 0, NULL, NULL, NULL);
        if (!sws_ctx) {
            av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(AV_PIX_FMT_GRAYF32), frame->width, frame->height,
                av_get_pix_fmt_name(AV_PIX_FMT_GRAY8),   frame->width, frame->height);
            ret = AVERROR(EINVAL);
            goto err;
        }
        sws_scale(sws_ctx, (const uint8_t *[4]){(const uint8_t *)output->data, 0, 0, 0},
                           (const int[4]){frame->width * sizeof(float), 0, 0, 0}, 0, frame->height,
                           (uint8_t * const*)frame->data, frame->linesize);
        sws_freeContext(sws_ctx);
        break;
    default:
        avpriv_report_missing_feature(log_ctx, "%s", av_get_pix_fmt_name(frame->format));
        ret = AVERROR(ENOSYS);
        goto err;
    }

err:
    av_free(middle_data);
    return ret;
}

int ff_proc_from_frame_to_dnn(AVFrame *frame, DNNData *input, void *log_ctx)
{
    struct SwsContext *sws_ctx;
    int ret = 0;
    int linesize[4] = { 0 };
    void **src_data = NULL;
    void *middle_data = NULL;
    uint8_t *planar_data[4] = { 0 };
    int plane_size = frame->width * frame->height * sizeof(uint8_t);
    int bytewidth = av_image_get_linesize(frame->format, frame->width, 0);
    if (bytewidth < 0) {
        return AVERROR(EINVAL);
    }
    if (input->dt != DNN_FLOAT) {
        avpriv_report_missing_feature(log_ctx, "data type rather than DNN_FLOAT");
        return AVERROR(ENOSYS);
    }

    src_data = (void **)frame->data;
    linesize[0] = frame->linesize[0];
    if (input->layout == DL_NCHW) {
        middle_data = av_malloc(plane_size * input->channels);
        if (!middle_data) {
            ret = AVERROR(ENOMEM);
            goto err;
        }
        src_data = &middle_data;
        linesize[0] = frame->width * 3;
    }

    switch (frame->format) {
    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_BGR24:
        // convert data from planar to packed
        if (input->layout == DL_NCHW) {
            sws_ctx = sws_getContext(frame->width,
                                     frame->height,
                                     frame->format,
                                     frame->width,
                                     frame->height,
                                     AV_PIX_FMT_GBRP,
                                     0, NULL, NULL, NULL);
            if (!sws_ctx) {
                av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
                       "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                       av_get_pix_fmt_name(frame->format), frame->width, frame->height,
                       av_get_pix_fmt_name(AV_PIX_FMT_GBRP),frame->width, frame->height);
                ret = AVERROR(EINVAL);
                goto err;
            }
            if (frame->format == AV_PIX_FMT_RGB24) {
                planar_data[0] = (uint8_t *)middle_data + plane_size;
                planar_data[1] = (uint8_t *)middle_data + plane_size * 2;
                planar_data[2] = (uint8_t *)middle_data;
            } else if (frame->format == AV_PIX_FMT_BGR24) {
                planar_data[0] = (uint8_t *)middle_data + plane_size;
                planar_data[1] = (uint8_t *)middle_data;
                planar_data[2] = (uint8_t *)middle_data + plane_size * 2;
            }
            sws_scale(sws_ctx, (const uint8_t * const *)frame->data,
                      frame->linesize, 0, frame->height, planar_data,
                      (const int [4]){frame->width * sizeof(uint8_t),
                                      frame->width * sizeof(uint8_t),
                                      frame->width * sizeof(uint8_t), 0});
            sws_freeContext(sws_ctx);
        }
        sws_ctx = sws_getContext(frame->width * 3,
                                 frame->height,
                                 AV_PIX_FMT_GRAY8,
                                 frame->width * 3,
                                 frame->height,
                                 AV_PIX_FMT_GRAYF32,
                                 0, NULL, NULL, NULL);
        if (!sws_ctx) {
            av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(AV_PIX_FMT_GRAY8),  frame->width * 3, frame->height,
                av_get_pix_fmt_name(AV_PIX_FMT_GRAYF32),frame->width * 3, frame->height);
            ret = AVERROR(EINVAL);
            goto err;
        }
        sws_scale(sws_ctx, (const uint8_t **)src_data,
                           linesize, 0, frame->height,
                           (uint8_t * const [4]){input->data, 0, 0, 0},
                           (const int [4]){frame->width * 3 * sizeof(float), 0, 0, 0});
        sws_freeContext(sws_ctx);
        break;
    case AV_PIX_FMT_GRAYF32:
        av_image_copy_plane(input->data, bytewidth,
                            frame->data[0], frame->linesize[0],
                            bytewidth, frame->height);
        break;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUV410P:
    case AV_PIX_FMT_YUV411P:
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_NV12:
        sws_ctx = sws_getContext(frame->width,
                                 frame->height,
                                 AV_PIX_FMT_GRAY8,
                                 frame->width,
                                 frame->height,
                                 AV_PIX_FMT_GRAYF32,
                                 0, NULL, NULL, NULL);
        if (!sws_ctx) {
            av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(AV_PIX_FMT_GRAY8),  frame->width, frame->height,
                av_get_pix_fmt_name(AV_PIX_FMT_GRAYF32),frame->width, frame->height);
            ret = AVERROR(EINVAL);
            goto err;
        }
        sws_scale(sws_ctx, (const uint8_t **)frame->data,
                           frame->linesize, 0, frame->height,
                           (uint8_t * const [4]){input->data, 0, 0, 0},
                           (const int [4]){frame->width * sizeof(float), 0, 0, 0});
        sws_freeContext(sws_ctx);
        break;
    default:
        avpriv_report_missing_feature(log_ctx, "%s", av_get_pix_fmt_name(frame->format));
        ret = AVERROR(ENOSYS);
        goto err;
    }
err:
    av_free(middle_data);
    return ret;
}

static enum AVPixelFormat get_pixel_format(DNNData *data)
{
    if (data->dt == DNN_UINT8) {
        switch (data->order) {
        case DCO_BGR:
            return AV_PIX_FMT_BGR24;
        case DCO_RGB:
            return AV_PIX_FMT_RGB24;
        default:
            av_assert0(!"unsupported data pixel format.\n");
            return AV_PIX_FMT_BGR24;
        }
    }

    av_assert0(!"unsupported data type.\n");
    return AV_PIX_FMT_BGR24;
}

int ff_frame_to_dnn_classify(AVFrame *frame, DNNData *input, uint32_t bbox_index, void *log_ctx)
{
    const AVPixFmtDescriptor *desc;
    int offsetx[4], offsety[4];
    uint8_t *bbox_data[4];
    struct SwsContext *sws_ctx;
    int linesizes[4];
    int ret = 0;
    enum AVPixelFormat fmt;
    int left, top, width, height;
    const AVDetectionBBoxHeader *header;
    const AVDetectionBBox *bbox;
    AVFrameSideData *sd = av_frame_get_side_data(frame, AV_FRAME_DATA_DETECTION_BBOXES);
    av_assert0(sd);

    if (input->layout == DL_NCHW) {
        av_log(log_ctx, AV_LOG_ERROR, "dnn_classify input data doesn't support layout: NCHW\n");
        return AVERROR(ENOSYS);
    }

    header = (const AVDetectionBBoxHeader *)sd->data;
    bbox = av_get_detection_bbox(header, bbox_index);

    left = bbox->x;
    width = bbox->w;
    top = bbox->y;
    height = bbox->h;

    fmt = get_pixel_format(input);
    sws_ctx = sws_getContext(width, height, frame->format,
                             input->width, input->height, fmt,
                             SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        av_log(log_ctx, AV_LOG_ERROR, "Failed to create scale context for the conversion "
               "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
               av_get_pix_fmt_name(frame->format), width, height,
               av_get_pix_fmt_name(fmt), input->width, input->height);
        return AVERROR(EINVAL);
    }

    ret = av_image_fill_linesizes(linesizes, fmt, input->width);
    if (ret < 0) {
        av_log(log_ctx, AV_LOG_ERROR, "unable to get linesizes with av_image_fill_linesizes");
        sws_freeContext(sws_ctx);
        return ret;
    }

    desc = av_pix_fmt_desc_get(frame->format);
    offsetx[1] = offsetx[2] = AV_CEIL_RSHIFT(left, desc->log2_chroma_w);
    offsetx[0] = offsetx[3] = left;

    offsety[1] = offsety[2] = AV_CEIL_RSHIFT(top, desc->log2_chroma_h);
    offsety[0] = offsety[3] = top;

    for (int k = 0; frame->data[k]; k++)
        bbox_data[k] = frame->data[k] + offsety[k] * frame->linesize[k] + offsetx[k];

    sws_scale(sws_ctx, (const uint8_t *const *)&bbox_data, frame->linesize,
                       0, height,
                       (uint8_t *const [4]){input->data, 0, 0, 0}, linesizes);

    sws_freeContext(sws_ctx);

    return ret;
}

int ff_frame_to_dnn_detect(AVFrame *frame, DNNData *input, void *log_ctx)
{
    struct SwsContext *sws_ctx;
    int linesizes[4];
    int ret = 0;
    enum AVPixelFormat fmt = get_pixel_format(input);

    if (input->layout == DL_NCHW) {
        av_log(log_ctx, AV_LOG_ERROR, "dnn_detect input data doesn't support layout: NCHW\n");
        return AVERROR(ENOSYS);
    }

    sws_ctx = sws_getContext(frame->width, frame->height, frame->format,
                             input->width, input->height, fmt,
                             SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        av_log(log_ctx, AV_LOG_ERROR, "Impossible to create scale context for the conversion "
            "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
            av_get_pix_fmt_name(frame->format), frame->width, frame->height,
            av_get_pix_fmt_name(fmt), input->width, input->height);
        return AVERROR(EINVAL);
    }

    ret = av_image_fill_linesizes(linesizes, fmt, input->width);
    if (ret < 0) {
        av_log(log_ctx, AV_LOG_ERROR, "unable to get linesizes with av_image_fill_linesizes");
        sws_freeContext(sws_ctx);
        return ret;
    }

    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height,
                       (uint8_t *const [4]){input->data, 0, 0, 0}, linesizes);

    sws_freeContext(sws_ctx);
    return ret;
}
