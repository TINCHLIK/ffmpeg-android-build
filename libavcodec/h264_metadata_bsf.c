/*
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
#include "libavutil/display.h"
#include "libavutil/common.h"
#include "libavutil/opt.h"

#include "bsf.h"
#include "cbs.h"
#include "cbs_h264.h"
#include "h264.h"
#include "h264_sei.h"

enum {
    PASS,
    INSERT,
    REMOVE,
    EXTRACT,
};

enum {
    FLIP_HORIZONTAL = 1,
    FLIP_VERTICAL   = 2,
};

typedef struct H264MetadataContext {
    const AVClass *class;

    CodedBitstreamContext *cbc;
    CodedBitstreamFragment access_unit;

    int done_first_au;

    int aud;

    AVRational sample_aspect_ratio;

    int video_format;
    int video_full_range_flag;
    int colour_primaries;
    int transfer_characteristics;
    int matrix_coefficients;

    int chroma_sample_loc_type;

    AVRational tick_rate;
    int fixed_frame_rate_flag;

    int crop_left;
    int crop_right;
    int crop_top;
    int crop_bottom;

    const char *sei_user_data;

    int delete_filler;

    int display_orientation;
    double rotate;
    int flip;
} H264MetadataContext;


static int h264_metadata_update_sps(AVBSFContext *bsf,
                                    H264RawSPS *sps)
{
    H264MetadataContext *ctx = bsf->priv_data;
    int need_vui = 0;
    int crop_unit_x, crop_unit_y;

    if (ctx->sample_aspect_ratio.num && ctx->sample_aspect_ratio.den) {
        // Table E-1.
        static const AVRational sar_idc[] = {
            {   0,  0 }, // Unspecified (never written here).
            {   1,  1 }, {  12, 11 }, {  10, 11 }, {  16, 11 },
            {  40, 33 }, {  24, 11 }, {  20, 11 }, {  32, 11 },
            {  80, 33 }, {  18, 11 }, {  15, 11 }, {  64, 33 },
            { 160, 99 }, {   4,  3 }, {   3,  2 }, {   2,  1 },
        };
        int num, den, i;

        av_reduce(&num, &den, ctx->sample_aspect_ratio.num,
                  ctx->sample_aspect_ratio.den, 65535);

        for (i = 1; i < FF_ARRAY_ELEMS(sar_idc); i++) {
            if (num == sar_idc[i].num &&
                den == sar_idc[i].den)
                break;
        }
        if (i == FF_ARRAY_ELEMS(sar_idc)) {
            sps->vui.aspect_ratio_idc = 255;
            sps->vui.sar_width  = num;
            sps->vui.sar_height = den;
        } else {
            sps->vui.aspect_ratio_idc = i;
        }
        sps->vui.aspect_ratio_info_present_flag = 1;
        need_vui = 1;
    }

#define SET_OR_INFER(field, value, present_flag, infer) do { \
        if (value >= 0) { \
            field = value; \
            need_vui = 1; \
        } else if (!present_flag) \
            field = infer; \
    } while (0)

    if (ctx->video_format             >= 0 ||
        ctx->video_full_range_flag    >= 0 ||
        ctx->colour_primaries         >= 0 ||
        ctx->transfer_characteristics >= 0 ||
        ctx->matrix_coefficients      >= 0) {

        SET_OR_INFER(sps->vui.video_format, ctx->video_format,
                     sps->vui.video_signal_type_present_flag, 5);

        SET_OR_INFER(sps->vui.video_full_range_flag,
                     ctx->video_full_range_flag,
                     sps->vui.video_signal_type_present_flag, 0);

        if (ctx->colour_primaries         >= 0 ||
            ctx->transfer_characteristics >= 0 ||
            ctx->matrix_coefficients      >= 0) {

            SET_OR_INFER(sps->vui.colour_primaries,
                         ctx->colour_primaries,
                         sps->vui.colour_description_present_flag, 2);

            SET_OR_INFER(sps->vui.transfer_characteristics,
                         ctx->transfer_characteristics,
                         sps->vui.colour_description_present_flag, 2);

            SET_OR_INFER(sps->vui.matrix_coefficients,
                         ctx->matrix_coefficients,
                         sps->vui.colour_description_present_flag, 2);

            sps->vui.colour_description_present_flag = 1;
        }
        sps->vui.video_signal_type_present_flag = 1;
        need_vui = 1;
    }

    if (ctx->chroma_sample_loc_type >= 0) {
        sps->vui.chroma_sample_loc_type_top_field =
            ctx->chroma_sample_loc_type;
        sps->vui.chroma_sample_loc_type_bottom_field =
            ctx->chroma_sample_loc_type;
        sps->vui.chroma_loc_info_present_flag = 1;
        need_vui = 1;
    }

    if (ctx->tick_rate.num && ctx->tick_rate.den) {
        int num, den;

        av_reduce(&num, &den, ctx->tick_rate.num, ctx->tick_rate.den,
                  UINT32_MAX > INT_MAX ? UINT32_MAX : INT_MAX);

        sps->vui.time_scale        = num;
        sps->vui.num_units_in_tick = den;

        sps->vui.timing_info_present_flag = 1;
        need_vui = 1;
    }
    SET_OR_INFER(sps->vui.fixed_frame_rate_flag,
                 ctx->fixed_frame_rate_flag,
                 sps->vui.timing_info_present_flag, 0);

    if (sps->separate_colour_plane_flag || sps->chroma_format_idc == 0) {
        crop_unit_x = 1;
        crop_unit_y = 2 - sps->frame_mbs_only_flag;
    } else {
        crop_unit_x = 1 + (sps->chroma_format_idc < 3);
        crop_unit_y = (1 + (sps->chroma_format_idc < 2)) *
                       (2 - sps->frame_mbs_only_flag);
    }
#define CROP(border, unit) do { \
        if (ctx->crop_ ## border >= 0) { \
            if (ctx->crop_ ## border % unit != 0) { \
                av_log(bsf, AV_LOG_ERROR, "Invalid value for crop_%s: " \
                       "must be a multiple of %d.\n", #border, unit); \
                return AVERROR(EINVAL); \
            } \
            sps->frame_crop_ ## border ## _offset = \
                  ctx->crop_ ## border / unit; \
            sps->frame_cropping_flag = 1; \
        } \
    } while (0)
    CROP(left,   crop_unit_x);
    CROP(right,  crop_unit_x);
    CROP(top,    crop_unit_y);
    CROP(bottom, crop_unit_y);
#undef CROP

    if (need_vui)
        sps->vui_parameters_present_flag = 1;

    return 0;
}

static int h264_metadata_filter(AVBSFContext *bsf, AVPacket *out)
{
    H264MetadataContext *ctx = bsf->priv_data;
    AVPacket *in = NULL;
    CodedBitstreamFragment *au = &ctx->access_unit;
    int err, i, j, has_sps;
    uint8_t *displaymatrix_side_data = NULL;
    size_t displaymatrix_side_data_size = 0;

    err = ff_bsf_get_packet(bsf, &in);
    if (err < 0)
        return err;

    err = ff_cbs_read_packet(ctx->cbc, au, in);
    if (err < 0) {
        av_log(bsf, AV_LOG_ERROR, "Failed to read packet.\n");
        goto fail;
    }

    if (au->nb_units == 0) {
        av_log(bsf, AV_LOG_ERROR, "No NAL units in packet.\n");
        err = AVERROR_INVALIDDATA;
        goto fail;
    }

    // If an AUD is present, it must be the first NAL unit.
    if (au->units[0].type == H264_NAL_AUD) {
        if (ctx->aud == REMOVE)
            ff_cbs_delete_unit(ctx->cbc, au, 0);
    } else {
        if (ctx->aud == INSERT) {
            static const int primary_pic_type_table[] = {
                0x084, // 2, 7
                0x0a5, // 0, 2, 5, 7
                0x0e7, // 0, 1, 2, 5, 6, 7
                0x210, // 4, 9
                0x318, // 3, 4, 8, 9
                0x294, // 2, 4, 7, 9
                0x3bd, // 0, 2, 3, 4, 5, 7, 8, 9
                0x3ff, // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
            };
            int primary_pic_type_mask = 0xff;
            H264RawAUD aud = {
                .nal_unit_header.nal_unit_type = H264_NAL_AUD,
            };

            for (i = 0; i < au->nb_units; i++) {
                if (au->units[i].type == H264_NAL_SLICE ||
                    au->units[i].type == H264_NAL_IDR_SLICE) {
                    H264RawSlice *slice = au->units[i].content;
                    for (j = 0; j < FF_ARRAY_ELEMS(primary_pic_type_table); j++) {
                         if (!(primary_pic_type_table[j] &
                               (1 << slice->header.slice_type)))
                             primary_pic_type_mask &= ~(1 << j);
                    }
                }
            }
            for (j = 0; j < FF_ARRAY_ELEMS(primary_pic_type_table); j++)
                if (primary_pic_type_mask & (1 << j))
                    break;
            if (j >= FF_ARRAY_ELEMS(primary_pic_type_table)) {
                av_log(bsf, AV_LOG_ERROR, "No usable primary_pic_type: "
                       "invalid slice types?\n");
                err = AVERROR_INVALIDDATA;
                goto fail;
            }

            aud.primary_pic_type = j;

            err = ff_cbs_insert_unit_content(ctx->cbc, au,
                                             0, H264_NAL_AUD, &aud, NULL);
            if (err < 0) {
                av_log(bsf, AV_LOG_ERROR, "Failed to insert AUD.\n");
                goto fail;
            }
        }
    }

    has_sps = 0;
    for (i = 0; i < au->nb_units; i++) {
        if (au->units[i].type == H264_NAL_SPS) {
            err = h264_metadata_update_sps(bsf, au->units[i].content);
            if (err < 0)
                goto fail;
            has_sps = 1;
        }
    }

    // Only insert the SEI in access units containing SPSs, and also
    // unconditionally in the first access unit we ever see.
    if (ctx->sei_user_data && (has_sps || !ctx->done_first_au)) {
        H264RawSEIPayload payload = {
            .payload_type = H264_SEI_TYPE_USER_DATA_UNREGISTERED,
        };
        H264RawSEIUserDataUnregistered *udu =
            &payload.payload.user_data_unregistered;

        for (i = j = 0; j < 32 && ctx->sei_user_data[i]; i++) {
            int c, v;
            c = ctx->sei_user_data[i];
            if (c == '-') {
                continue;
            } else if (av_isxdigit(c)) {
                c = av_tolower(c);
                v = (c <= '9' ? c - '0' : c - 'a' + 10);
            } else {
                goto invalid_user_data;
            }
            if (i & 1)
                udu->uuid_iso_iec_11578[j / 2] |= v;
            else
                udu->uuid_iso_iec_11578[j / 2] = v << 4;
            ++j;
        }
        if (j == 32 && ctx->sei_user_data[i] == '+') {
            size_t len = strlen(ctx->sei_user_data + i + 1);

            udu->data_ref = av_buffer_alloc(len + 1);
            if (!udu->data_ref) {
                err = AVERROR(ENOMEM);
                goto fail;
            }

            udu->data        = udu->data_ref->data;
            udu->data_length = len + 1;
            memcpy(udu->data, ctx->sei_user_data + i + 1, len + 1);

            err = ff_cbs_h264_add_sei_message(ctx->cbc, au, &payload);
            if (err < 0) {
                av_log(bsf, AV_LOG_ERROR, "Failed to add user data SEI "
                       "message to access unit.\n");
                goto fail;
            }

        } else {
        invalid_user_data:
            av_log(bsf, AV_LOG_ERROR, "Invalid user data: "
                   "must be \"UUID+string\".\n");
            err = AVERROR(EINVAL);
            goto fail;
        }
    }

    if (ctx->delete_filler) {
        for (i = 0; i < au->nb_units; i++) {
            if (au->units[i].type == H264_NAL_FILLER_DATA) {
                // Filler NAL units.
                err = ff_cbs_delete_unit(ctx->cbc, au, i);
                if (err < 0) {
                    av_log(bsf, AV_LOG_ERROR, "Failed to delete "
                           "filler NAL.\n");
                    goto fail;
                }
                --i;
                continue;
            }

            if (au->units[i].type == H264_NAL_SEI) {
                // Filler SEI messages.
                H264RawSEI *sei = au->units[i].content;

                for (j = 0; j < sei->payload_count; j++) {
                    if (sei->payload[j].payload_type ==
                        H264_SEI_TYPE_FILLER_PAYLOAD) {
                        err = ff_cbs_h264_delete_sei_message(ctx->cbc, au,
                                                             &au->units[i], j);
                        if (err < 0) {
                            av_log(bsf, AV_LOG_ERROR, "Failed to delete "
                                   "filler SEI message.\n");
                            goto fail;
                        }
                        // Renumbering might have happened, start again at
                        // the same NAL unit position.
                        --i;
                        break;
                    }
                }
            }
        }
    }

    if (ctx->display_orientation != PASS) {
        for (i = 0; i < au->nb_units; i++) {
            H264RawSEI *sei;
            if (au->units[i].type != H264_NAL_SEI)
                continue;
            sei = au->units[i].content;

            for (j = 0; j < sei->payload_count; j++) {
                H264RawSEIDisplayOrientation *disp;
                int32_t *matrix;

                if (sei->payload[j].payload_type !=
                    H264_SEI_TYPE_DISPLAY_ORIENTATION)
                    continue;
                disp = &sei->payload[j].payload.display_orientation;

                if (ctx->display_orientation == REMOVE ||
                    ctx->display_orientation == INSERT) {
                    err = ff_cbs_h264_delete_sei_message(ctx->cbc, au,
                                                         &au->units[i], j);
                    if (err < 0) {
                        av_log(bsf, AV_LOG_ERROR, "Failed to delete "
                               "display orientation SEI message.\n");
                        goto fail;
                    }
                    --i;
                    break;
                }

                matrix = av_mallocz(9 * sizeof(int32_t));
                if (!matrix) {
                    err = AVERROR(ENOMEM);
                    goto fail;
                }

                av_display_rotation_set(matrix,
                                        disp->anticlockwise_rotation *
                                        180.0 / 65536.0);
                av_display_matrix_flip(matrix, disp->hor_flip, disp->ver_flip);

                // If there are multiple display orientation messages in an
                // access unit then ignore all but the last one.
                av_freep(&displaymatrix_side_data);

                displaymatrix_side_data      = (uint8_t*)matrix;
                displaymatrix_side_data_size = 9 * sizeof(int32_t);
            }
        }
    }
    if (ctx->display_orientation == INSERT) {
        H264RawSEIPayload payload = {
            .payload_type = H264_SEI_TYPE_DISPLAY_ORIENTATION,
        };
        H264RawSEIDisplayOrientation *disp =
            &payload.payload.display_orientation;
        uint8_t *data;
        int size;
        int write = 0;

        data = av_packet_get_side_data(in, AV_PKT_DATA_DISPLAYMATRIX, &size);
        if (data && size >= 9 * sizeof(int32_t)) {
            int32_t matrix[9];
            int hflip, vflip;
            double angle;

            memcpy(matrix, data, sizeof(matrix));

            hflip = vflip = 0;
            if (matrix[0] < 0 && matrix[4] > 0)
                hflip = 1;
            else if (matrix[0] > 0 && matrix[4] < 0)
                vflip = 1;
            av_display_matrix_flip(matrix, hflip, vflip);

            angle = av_display_rotation_get(matrix);

            if (!(angle >= -180.0 && angle <= 180.0 /* also excludes NaN */) ||
                matrix[2] != 0 || matrix[5] != 0 ||
                matrix[6] != 0 || matrix[7] != 0) {
                av_log(bsf, AV_LOG_WARNING, "Input display matrix is not "
                       "representable in H.264 parameters.\n");
            } else {
                disp->hor_flip = hflip;
                disp->ver_flip = vflip;
                disp->anticlockwise_rotation =
                    (uint16_t)rint((angle >= 0.0 ? angle
                                                 : angle + 360.0) *
                                   65536.0 / 360.0);
                write = 1;
            }
        }

        if (has_sps || !ctx->done_first_au) {
            if (!isnan(ctx->rotate)) {
                disp->anticlockwise_rotation =
                    (uint16_t)rint((ctx->rotate >= 0.0 ? ctx->rotate
                                                       : ctx->rotate + 360.0) *
                                   65536.0 / 360.0);
                write = 1;
            }
            if (ctx->flip) {
                disp->hor_flip = !!(ctx->flip & FLIP_HORIZONTAL);
                disp->ver_flip = !!(ctx->flip & FLIP_VERTICAL);
                write = 1;
            }
        }

        if (write) {
            disp->display_orientation_repetition_period = 1;

            err = ff_cbs_h264_add_sei_message(ctx->cbc, au, &payload);
            if (err < 0) {
                av_log(bsf, AV_LOG_ERROR, "Failed to add display orientation "
                       "SEI message to access unit.\n");
                goto fail;
            }
        }
    }

    err = ff_cbs_write_packet(ctx->cbc, out, au);
    if (err < 0) {
        av_log(bsf, AV_LOG_ERROR, "Failed to write packet.\n");
        goto fail;
    }

    err = av_packet_copy_props(out, in);
    if (err < 0)
        goto fail;

    if (displaymatrix_side_data) {
        err = av_packet_add_side_data(out, AV_PKT_DATA_DISPLAYMATRIX,
                                      displaymatrix_side_data,
                                      displaymatrix_side_data_size);
        if (err) {
            av_log(bsf, AV_LOG_ERROR, "Failed to attach extracted "
                   "displaymatrix side data to packet.\n");
            goto fail;
        }
        displaymatrix_side_data = NULL;
    }

    ctx->done_first_au = 1;

    err = 0;
fail:
    ff_cbs_fragment_uninit(ctx->cbc, au);
    av_freep(&displaymatrix_side_data);

    if (err < 0)
        av_packet_unref(out);
    av_packet_free(&in);

    return err;
}

static int h264_metadata_init(AVBSFContext *bsf)
{
    H264MetadataContext *ctx = bsf->priv_data;
    CodedBitstreamFragment *au = &ctx->access_unit;
    int err, i;

    err = ff_cbs_init(&ctx->cbc, AV_CODEC_ID_H264, bsf);
    if (err < 0)
        return err;

    if (bsf->par_in->extradata) {
        err = ff_cbs_read_extradata(ctx->cbc, au, bsf->par_in);
        if (err < 0) {
            av_log(bsf, AV_LOG_ERROR, "Failed to read extradata.\n");
            goto fail;
        }

        for (i = 0; i < au->nb_units; i++) {
            if (au->units[i].type == H264_NAL_SPS) {
                err = h264_metadata_update_sps(bsf, au->units[i].content);
                if (err < 0)
                    goto fail;
            }
        }

        err = ff_cbs_write_extradata(ctx->cbc, bsf->par_out, au);
        if (err < 0) {
            av_log(bsf, AV_LOG_ERROR, "Failed to write extradata.\n");
            goto fail;
        }
    }

    err = 0;
fail:
    ff_cbs_fragment_uninit(ctx->cbc, au);
    return err;
}

static void h264_metadata_close(AVBSFContext *bsf)
{
    H264MetadataContext *ctx = bsf->priv_data;
    ff_cbs_close(&ctx->cbc);
}

#define OFFSET(x) offsetof(H264MetadataContext, x)
#define FLAGS (AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_BSF_PARAM)
static const AVOption h264_metadata_options[] = {
    { "aud", "Access Unit Delimiter NAL units",
        OFFSET(aud), AV_OPT_TYPE_INT,
        { .i64 = PASS }, PASS, REMOVE, FLAGS, "aud" },
    { "pass",   NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = PASS   }, .flags = FLAGS, .unit = "aud" },
    { "insert", NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = INSERT }, .flags = FLAGS, .unit = "aud" },
    { "remove", NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = REMOVE }, .flags = FLAGS, .unit = "aud" },

    { "sample_aspect_ratio", "Set sample aspect ratio (table E-1)",
        OFFSET(sample_aspect_ratio), AV_OPT_TYPE_RATIONAL,
        { .dbl = 0.0 }, 0, 65535, FLAGS },

    { "video_format", "Set video format (table E-2)",
        OFFSET(video_format), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 7, FLAGS},
    { "video_full_range_flag", "Set video full range flag",
        OFFSET(video_full_range_flag), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 1, FLAGS },
    { "colour_primaries", "Set colour primaries (table E-3)",
        OFFSET(colour_primaries), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 255, FLAGS },
    { "transfer_characteristics", "Set transfer characteristics (table E-4)",
        OFFSET(transfer_characteristics), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 255, FLAGS },
    { "matrix_coefficients", "Set matrix coefficients (table E-5)",
        OFFSET(matrix_coefficients), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 255, FLAGS },

    { "chroma_sample_loc_type", "Set chroma sample location type (figure E-1)",
        OFFSET(chroma_sample_loc_type), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 6, FLAGS },

    { "tick_rate", "Set VUI tick rate (num_units_in_tick / time_scale)",
        OFFSET(tick_rate), AV_OPT_TYPE_RATIONAL,
        { .dbl = 0.0 }, 0, UINT_MAX, FLAGS },
    { "fixed_frame_rate_flag", "Set VUI fixed frame rate flag",
        OFFSET(fixed_frame_rate_flag), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, 1, FLAGS },

    { "crop_left", "Set left border crop offset",
        OFFSET(crop_left), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, H264_MAX_WIDTH, FLAGS },
    { "crop_right", "Set right border crop offset",
        OFFSET(crop_right), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, H264_MAX_WIDTH, FLAGS },
    { "crop_top", "Set top border crop offset",
        OFFSET(crop_top), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, H264_MAX_HEIGHT, FLAGS },
    { "crop_bottom", "Set bottom border crop offset",
        OFFSET(crop_bottom), AV_OPT_TYPE_INT,
        { .i64 = -1 }, -1, H264_MAX_HEIGHT, FLAGS },

    { "sei_user_data", "Insert SEI user data (UUID+string)",
        OFFSET(sei_user_data), AV_OPT_TYPE_STRING, { .str = NULL }, .flags = FLAGS },

    { "delete_filler", "Delete all filler (both NAL and SEI)",
        OFFSET(delete_filler), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 1, FLAGS},

    { "display_orientation", "Display orientation SEI",
        OFFSET(display_orientation), AV_OPT_TYPE_INT,
        { .i64 = PASS }, PASS, EXTRACT, FLAGS, "disp_or" },
    { "pass",    NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = PASS    }, .flags = FLAGS, .unit = "disp_or" },
    { "insert",  NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = INSERT  }, .flags = FLAGS, .unit = "disp_or" },
    { "remove",  NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = REMOVE  }, .flags = FLAGS, .unit = "disp_or" },
    { "extract", NULL, 0, AV_OPT_TYPE_CONST,
        { .i64 = EXTRACT }, .flags = FLAGS, .unit = "disp_or" },

    { "rotate", "Set rotation in display orientation SEI (anticlockwise angle in degrees)",
        OFFSET(rotate), AV_OPT_TYPE_DOUBLE,
        { .dbl = NAN }, -360.0, +360.0, FLAGS },
    { "flip", "Set flip in display orientation SEI",
        OFFSET(flip), AV_OPT_TYPE_FLAGS,
        { .i64 = 0 }, 0, FLIP_HORIZONTAL | FLIP_VERTICAL, FLAGS, "flip" },
    { "horizontal", "Set hor_flip",
        0, AV_OPT_TYPE_CONST,
        { .i64 = FLIP_HORIZONTAL }, .flags = FLAGS, .unit = "flip" },
    { "vertical",   "Set ver_flip",
        0, AV_OPT_TYPE_CONST,
        { .i64 = FLIP_VERTICAL },   .flags = FLAGS, .unit = "flip" },

    { NULL }
};

static const AVClass h264_metadata_class = {
    .class_name = "h264_metadata_bsf",
    .item_name  = av_default_item_name,
    .option     = h264_metadata_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const enum AVCodecID h264_metadata_codec_ids[] = {
    AV_CODEC_ID_H264, AV_CODEC_ID_NONE,
};

const AVBitStreamFilter ff_h264_metadata_bsf = {
    .name           = "h264_metadata",
    .priv_data_size = sizeof(H264MetadataContext),
    .priv_class     = &h264_metadata_class,
    .init           = &h264_metadata_init,
    .close          = &h264_metadata_close,
    .filter         = &h264_metadata_filter,
    .codec_ids      = h264_metadata_codec_ids,
};
