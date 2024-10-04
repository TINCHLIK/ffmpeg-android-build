/*
 * MXF
 * Copyright (c) 2006 SmartJog S.A., Baptiste Coudurier <baptiste dot coudurier at smartjog dot com>
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

#include "libavutil/common.h"
#include "libavcodec/codec_id.h"
#include "mxf.h"

const uint8_t ff_mxf_random_index_pack_key[16] = { 0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x11,0x01,0x00 };

/**
 * SMPTE RP224 http://www.smpte-ra.org/mdd/index.html
 */
const MXFCodecUL ff_mxf_data_definition_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x01,0x00,0x00,0x00 }, 13, AVMEDIA_TYPE_VIDEO },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x02,0x00,0x00,0x00 }, 13, AVMEDIA_TYPE_AUDIO },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x03,0x00,0x00,0x00 }, 13, AVMEDIA_TYPE_DATA },
    { { 0x80,0x7D,0x00,0x60,0x08,0x14,0x3E,0x6F,0x6F,0x3C,0x8C,0xE1,0x6C,0xEF,0x11,0xD2 }, 16, AVMEDIA_TYPE_VIDEO }, /* LegacyPicture Avid Media Composer MXF */
    { { 0x80,0x7D,0x00,0x60,0x08,0x14,0x3E,0x6F,0x78,0xE1,0xEB,0xE1,0x6C,0xEF,0x11,0xD2 }, 16, AVMEDIA_TYPE_AUDIO }, /* LegacySound Avid Media Composer MXF */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0,  AVMEDIA_TYPE_DATA },
};

const MXFCodecUL ff_mxf_codec_uls[] = {
    /* PictureEssenceCoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x01,0x11,0x00 }, 14, AV_CODEC_ID_MPEG2VIDEO }, /* MP@ML Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x01 }, 14, AV_CODEC_ID_MPEG2VIDEO }, /* D-10 50Mbps PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x03,0x03,0x00 }, 14, AV_CODEC_ID_MPEG2VIDEO }, /* MP@HL Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x04,0x02,0x00 }, 14, AV_CODEC_ID_MPEG2VIDEO }, /* 422P@HL I-Frame */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x03 }, 14,      AV_CODEC_ID_MPEG4 }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x02,0x01,0x02,0x00 }, 13,    AV_CODEC_ID_DVVIDEO }, /* DV25 IEC PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x01,0x02,0x02,0x03,0x01,0x01,0x00 }, 14,   AV_CODEC_ID_JPEG2000 }, /* JPEG 2000 code stream */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x01,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 SP@LL */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x02,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 SP@ML */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x03,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 MP@LL */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x04,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 MP@ML */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x05,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 MP@HL */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x06,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 AP@L0 */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x07,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 AP@L1 */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x08,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 AP@L2 */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x09,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 AP@L3 */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x04,0x0A,0x00,0x00 }, 14,        AV_CODEC_ID_VC1 }, /* VC1 AP@L4 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x01,0x7F,0x00,0x00,0x00 }, 13,   AV_CODEC_ID_RAWVIDEO }, /* uncompressed */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x01,0x01,0x02,0x01,0x00 }, 15,   AV_CODEC_ID_RAWVIDEO }, /* uncompressed 422 8-bit */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x71,0x00,0x00,0x00 }, 13,      AV_CODEC_ID_DNXHD }, /* SMPTE VC-3/DNxHD */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x03,0x02,0x00,0x00 }, 14,      AV_CODEC_ID_DNXHD }, /* SMPTE VC-3/DNxHD */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0E,0x04,0x02,0x01,0x02,0x04,0x01,0x00 }, 16,      AV_CODEC_ID_DNXHD }, /* SMPTE VC-3/DNxHD Legacy Avid Media Composer MXF */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x02,0x02,0x03,0x07,0x01,0x00 }, 14,      AV_CODEC_ID_DNXUC }, /* DNxUncompressed/SMPTE RDD 50 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x01,0x32,0x00,0x00 }, 14,       AV_CODEC_ID_H264 }, /* H.264/MPEG-4 AVC Intra */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x02,0x01,0x31,0x11,0x01 }, 14,       AV_CODEC_ID_H264 }, /* H.264/MPEG-4 AVC SPS/PPS in-band */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x01,0x01,0x02,0x02,0x01 }, 16,       AV_CODEC_ID_V210 }, /* V210 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0E,0x04,0x02,0x01,0x02,0x11,0x00,0x00 }, 14,     AV_CODEC_ID_PRORES }, /* Avid MC7 ProRes */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x02,0x02,0x03,0x06,0x00,0x00 }, 14,     AV_CODEC_ID_PRORES }, /* Apple ProRes */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x02,0x02,0x03,0x09,0x01,0x00 }, 15,       AV_CODEC_ID_FFV1 }, /*FFV1 V0 */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x02,0x02,0x03,0x09,0x02,0x00 }, 15,       AV_CODEC_ID_FFV1 }, /*FFV1 V1 */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x02,0x02,0x03,0x09,0x04,0x00 }, 15,       AV_CODEC_ID_FFV1 }, /*FFV1 V3 */
    /* SoundEssenceCompression */
    { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x03,0x04,0x02,0x02,0x02,0x03,0x03,0x01,0x00 }, 14,        AV_CODEC_ID_AAC }, /* MPEG-2 AAC ADTS (legacy) */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x00,0x00,0x00,0x00 }, 13,  AV_CODEC_ID_PCM_S16LE }, /* uncompressed */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x7F,0x00,0x00,0x00 }, 13,  AV_CODEC_ID_PCM_S16LE },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x02,0x02,0x01,0x7E,0x00,0x00,0x00 }, 13,  AV_CODEC_ID_PCM_S16BE }, /* From Omneon MXF file */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x04,0x04,0x02,0x02,0x02,0x03,0x01,0x01,0x00 }, 15,   AV_CODEC_ID_PCM_ALAW }, /* XDCAM Proxy C0023S01.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x01,0x00 }, 15,        AV_CODEC_ID_AC3 },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x05,0x00 }, 15,        AV_CODEC_ID_MP2 }, /* MP2 or MP3 */
  //{ { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x1C,0x00 }, 15,    AV_CODEC_ID_DOLBY_E }, /* Dolby-E */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x02,0x02,0x02,0x04,0x03,0x00,0x00 }, 14,        AV_CODEC_ID_AAC }, /* MPEG-2 AAC SMPTE 381-4 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x02,0x02,0x02,0x04,0x04,0x00,0x00 }, 14,        AV_CODEC_ID_AAC }, /* MPEG-4 AAC SMPTE 381-4 */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0,       AV_CODEC_ID_NONE },
};

const MXFCodecUL ff_mxf_pixel_format_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x01,0x01,0x02,0x01,0x01 }, 16, AV_PIX_FMT_UYVY422 },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0A,0x04,0x01,0x02,0x01,0x01,0x02,0x01,0x02 }, 16, AV_PIX_FMT_YUYV422 },
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0,    AV_PIX_FMT_NONE },
};

const MXFCodecUL ff_mxf_codec_tag_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0E,0x04,0x03,0x01,0x01,0x03,0x01,0x00 }, 15, MKTAG('A', 'V', 'u', 'p') }, /* Avid 1:1 */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0,                         0 },
};

const MXFCodecUL ff_mxf_color_primaries_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x03,0x01,0x00,0x00 }, 14, AVCOL_PRI_SMPTE170M }, /* SMPTE 170M */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x03,0x02,0x00,0x00 }, 14, AVCOL_PRI_BT470BG }, /* ITU-R BT.470 PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x03,0x03,0x00,0x00 }, 14, AVCOL_PRI_BT709 }, /* ITU-R BT.709 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x03,0x04,0x00,0x00 }, 14, AVCOL_PRI_BT2020 }, /* ITU-R BT.2020 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x03,0x05,0x00,0x00 }, 14, AVCOL_PRI_SMPTE428 }, /* SMPTE-DC28 DCDM */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x03,0x06,0x00,0x00 }, 14, AVCOL_PRI_SMPTE432 }, /* P3D65 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x03,0x08,0x00,0x00 }, 14, AVCOL_PRI_SMPTE428 }, /* Cinema Mezzanine */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x03,0x0a,0x00,0x00 }, 14, AVCOL_PRI_SMPTE431 }, /* P3DCI */
    /* alternate mappings for encoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x03,0x01,0x00,0x00 }, 14, AVCOL_PRI_SMPTE240M }, /* = AVCOL_PRI_SMPTE170M */

    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0, AVCOL_PRI_UNSPECIFIED },
};

const MXFCodecUL ff_mxf_color_trc_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,0x01,0x00,0x00 }, 14, AVCOL_TRC_GAMMA22 }, /* ITU-R BT.470 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,0x02,0x00,0x00 }, 14, AVCOL_TRC_BT709 }, /* ITU-R BT.709 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,0x03,0x00,0x00 }, 14, AVCOL_TRC_SMPTE240M }, /* SMPTE 240M */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,0x04,0x00,0x00 }, 14, AVCOL_TRC_BT709 }, /* SMPTE 274/296M (must appear after ITU-R BT.709) */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x01,0x05,0x00,0x00 }, 14, AVCOL_TRC_BT1361_ECG }, /* ITU-R BT.1361 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x01,0x06,0x00,0x00 }, 14, AVCOL_TRC_LINEAR }, /* Linear */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x08,0x04,0x01,0x01,0x01,0x01,0x07,0x00,0x00 }, 14, AVCOL_TRC_SMPTE428 }, /* SMPTE-DC28 DCDM */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x01,0x08,0x00,0x00 }, 14, AVCOL_TRC_IEC61966_2_4 }, /* IEC 61966-2-4 xvYCC */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0E,0x04,0x01,0x01,0x01,0x01,0x09,0x00,0x00 }, 14, AVCOL_TRC_BT2020_10 }, /* ITU-R BT.2020 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x01,0x0A,0x00,0x00 }, 14, AVCOL_TRC_SMPTE2084 }, /* SMPTE ST 2084 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x01,0x0B,0x00,0x00 }, 14, AVCOL_TRC_ARIB_STD_B67 }, /* Hybrid Log-Gamma OETF */
    /* alternate mappings for encoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,0x01,0x00,0x00 }, 14, AVCOL_TRC_GAMMA28 }, /* = AVCOL_TRC_GAMMA22 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,0x02,0x00,0x00 }, 14, AVCOL_TRC_SMPTE170M }, /* = AVCOL_TRC_BT709 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0E,0x04,0x01,0x01,0x01,0x01,0x09,0x00,0x00 }, 14, AVCOL_TRC_BT2020_12 }, /* = AVCOL_TRC_BT2020_10 */

    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0, AVCOL_TRC_UNSPECIFIED },
};

/* aka Coding Equations */
const MXFCodecUL ff_mxf_color_space_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x02,0x01,0x00,0x00 }, 14, AVCOL_SPC_BT470BG }, /* ITU-R BT.601 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x02,0x02,0x00,0x00 }, 14, AVCOL_SPC_BT709 }, /* ITU-R BT.709 */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x06,0x04,0x01,0x01,0x01,0x02,0x03,0x00,0x00 }, 14, AVCOL_SPC_SMPTE240M }, /* SMPTE 240M */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x02,0x04,0x00,0x00 }, 14, AVCOL_SPC_YCGCO }, /* YCgCo */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x02,0x05,0x00,0x00 }, 14, AVCOL_SPC_RGB }, /* GBR */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0D,0x04,0x01,0x01,0x01,0x02,0x06,0x00,0x00 }, 14, AVCOL_SPC_BT2020_NCL }, /* ITU-R BT.2020 Non-Constant Luminance */
    /* alternate mappings for encoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x02,0x01,0x00,0x00 }, 14, AVCOL_SPC_SMPTE170M }, /* = AVCOL_SPC_BT470BG */

    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0, AVCOL_SPC_UNSPECIFIED },
};

static const struct {
    enum AVPixelFormat pix_fmt;
    const char data[16];
} ff_mxf_pixel_layouts[] = {
    /**
     * See SMPTE 377M E.2.46
     *
     * Note: Only RGB, palette based and "abnormal" YUV pixel formats like 4:2:2:4 go here.
     *       For regular YUV, use CDCIPictureEssenceDescriptor.
     *
     * Note: Do not use these for encoding descriptors for little-endian formats until we
     *       get samples or official word from SMPTE on how/if those can be encoded.
     */
    {AV_PIX_FMT_ABGR,    {'A', 8,  'B', 8,  'G', 8, 'R', 8                 }},
    {AV_PIX_FMT_ARGB,    {'A', 8,  'R', 8,  'G', 8, 'B', 8                 }},
    {AV_PIX_FMT_BGR24,   {'B', 8,  'G', 8,  'R', 8                         }},
    {AV_PIX_FMT_BGRA,    {'B', 8,  'G', 8,  'R', 8, 'A', 8                 }},
    {AV_PIX_FMT_RGB24,   {'R', 8,  'G', 8,  'B', 8                         }},
    {AV_PIX_FMT_RGB444BE,{'F', 4,  'R', 4,  'G', 4, 'B', 4                 }},
    {AV_PIX_FMT_RGB48BE, {'R', 8,  'r', 8,  'G', 8, 'g', 8, 'B', 8, 'b', 8 }},
    {AV_PIX_FMT_RGB48BE, {'R', 16, 'G', 16, 'B', 16                        }},
    {AV_PIX_FMT_RGB48LE, {'r', 8,  'R', 8,  'g', 8, 'G', 8, 'b', 8, 'B', 8 }},
    {AV_PIX_FMT_RGB555BE,{'F', 1,  'R', 5,  'G', 5, 'B', 5                 }},
    {AV_PIX_FMT_RGB565BE,{'R', 5,  'G', 6,  'B', 5                         }},
    {AV_PIX_FMT_RGBA,    {'R', 8,  'G', 8,  'B', 8, 'A', 8                 }},
    {AV_PIX_FMT_PAL8,    {'P', 8                                           }},
    {AV_PIX_FMT_GRAY8,   {'A', 8                                           }},
};

static const int num_pixel_layouts = FF_ARRAY_ELEMS(ff_mxf_pixel_layouts);

int ff_mxf_decode_pixel_layout(const char pixel_layout[16], enum AVPixelFormat *pix_fmt)
{
    int x;

    for(x = 0; x < num_pixel_layouts; x++) {
        if (!memcmp(pixel_layout, ff_mxf_pixel_layouts[x].data, 16)) {
            *pix_fmt = ff_mxf_pixel_layouts[x].pix_fmt;
            return 0;
        }
    }

    return -1;
}

/**
 * See SMPTE 326M-2000 Section 7.2 Content package rate
 * MXFContentPackageRate->rate is bits b5..b0.
 */
static const MXFContentPackageRate mxf_content_package_rates[] = {
    {  2, { 1,    24    } },
    {  3, { 1001, 24000 } },
    {  4, { 1,    25    } },
    {  6, { 1,    30    } },
    {  7, { 1001, 30000 } },
    {  8, { 1   , 48    } },
    {  9, { 1001, 48000 } },
    { 10, { 1,    50    } },
    { 12, { 1,    60    } },
    { 13, { 1001, 60000 } },
    { 14, { 1,    72    } },
    { 15, { 1001, 72000 } },
    { 16, { 1,    75    } },
    { 18, { 1,    90    } },
    { 19, { 1001, 90000 } },
    { 20, { 1,    96    } },
    { 21, { 1001, 96000 } },
    { 22, { 1,    100   } },
    { 24, { 1,    120   } },
    { 25, { 1001, 120000} },
    {0}
};

int ff_mxf_get_content_package_rate(AVRational time_base)
{
    for (int i = 0; mxf_content_package_rates[i].rate; i++)
        if (!av_cmp_q(time_base, mxf_content_package_rates[i].tb))
            return mxf_content_package_rates[i].rate;
    return 0;
}
