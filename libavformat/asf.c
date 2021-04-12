/*
 * Copyright (c) 2000, 2001 Fabrice Bellard
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

#include "asf.h"
#include "id3v2.h"
#include "internal.h"

const ff_asf_guid ff_asf_header = {
    0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C
};

const ff_asf_guid ff_asf_file_header = {
    0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

const ff_asf_guid ff_asf_stream_header = {
    0x91, 0x07, 0xDC, 0xB7, 0xB7, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

const ff_asf_guid ff_asf_ext_stream_header = {
    0xCB, 0xA5, 0xE6, 0x14, 0x72, 0xC6, 0x32, 0x43, 0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A
};

const ff_asf_guid ff_asf_audio_stream = {
    0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

const ff_asf_guid ff_asf_audio_conceal_spread = {
    0x50, 0xCD, 0xC3, 0xBF, 0x8F, 0x61, 0xCF, 0x11, 0x8B, 0xB2, 0x00, 0xAA, 0x00, 0xB4, 0xE2, 0x20
};

const ff_asf_guid ff_asf_video_stream = {
    0xC0, 0xEF, 0x19, 0xBC, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

const ff_asf_guid ff_asf_jfif_media = {
    0x00, 0xE1, 0x1B, 0xB6, 0x4E, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

const ff_asf_guid ff_asf_video_conceal_none = {
    0x00, 0x57, 0xFB, 0x20, 0x55, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

const ff_asf_guid ff_asf_command_stream = {
    0xC0, 0xCF, 0xDA, 0x59, 0xE6, 0x59, 0xD0, 0x11, 0xA3, 0xAC, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6
};

const ff_asf_guid ff_asf_comment_header = {
    0x33, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};

const ff_asf_guid ff_asf_codec_comment_header = {
    0x40, 0x52, 0xD1, 0x86, 0x1D, 0x31, 0xD0, 0x11, 0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6
};
const ff_asf_guid ff_asf_codec_comment1_header = {
    0x41, 0x52, 0xd1, 0x86, 0x1D, 0x31, 0xD0, 0x11, 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6
};

const ff_asf_guid ff_asf_data_header = {
    0x36, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};

const ff_asf_guid ff_asf_head1_guid = {
    0xb5, 0x03, 0xbf, 0x5f, 0x2E, 0xA9, 0xCF, 0x11, 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65
};

const ff_asf_guid ff_asf_head2_guid = {
    0x11, 0xd2, 0xd3, 0xab, 0xBA, 0xA9, 0xCF, 0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65
};

const ff_asf_guid ff_asf_extended_content_header = {
    0x40, 0xA4, 0xD0, 0xD2, 0x07, 0xE3, 0xD2, 0x11, 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50
};

const ff_asf_guid ff_asf_simple_index_header = {
    0x90, 0x08, 0x00, 0x33, 0xB1, 0xE5, 0xCF, 0x11, 0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB
};

const ff_asf_guid ff_asf_ext_stream_embed_stream_header = {
    0xe2, 0x65, 0xfb, 0x3a, 0xEF, 0x47, 0xF2, 0x40, 0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43
};

const ff_asf_guid ff_asf_ext_stream_audio_stream = {
    0x9d, 0x8c, 0x17, 0x31, 0xE1, 0x03, 0x28, 0x45, 0xb5, 0x82, 0x3d, 0xf9, 0xdb, 0x22, 0xf5, 0x03
};

const ff_asf_guid ff_asf_metadata_header = {
    0xea, 0xcb, 0xf8, 0xc5, 0xaf, 0x5b, 0x77, 0x48, 0x84, 0x67, 0xaa, 0x8c, 0x44, 0xfa, 0x4c, 0xca
};

const ff_asf_guid ff_asf_metadata_library_header = {
    0x94, 0x1c, 0x23, 0x44, 0x98, 0x94, 0xd1, 0x49, 0xa1, 0x41, 0x1d, 0x13, 0x4e, 0x45, 0x70, 0x54
};

const ff_asf_guid ff_asf_marker_header = {
    0x01, 0xCD, 0x87, 0xF4, 0x51, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

const ff_asf_guid ff_asf_reserved_4 = {
        0x20, 0xdb, 0xfe, 0x4c, 0xf6, 0x75, 0xCF, 0x11, 0x9c, 0x0f, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb
};

/* I am not a number !!! This GUID is the one found on the PC used to
 * generate the stream */
const ff_asf_guid ff_asf_my_guid = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const ff_asf_guid ff_asf_language_guid = {
    0xa9, 0x46, 0x43, 0x7c, 0xe0, 0xef, 0xfc, 0x4b, 0xb2, 0x29, 0x39, 0x3e, 0xde, 0x41, 0x5c, 0x85
};

const ff_asf_guid ff_asf_content_encryption = {
    0xfb, 0xb3, 0x11, 0x22, 0x23, 0xbd, 0xd2, 0x11, 0xb4, 0xb7, 0x00, 0xa0, 0xc9, 0x55, 0xfc, 0x6e
};

const ff_asf_guid ff_asf_ext_content_encryption = {
    0x14, 0xe6, 0x8a, 0x29, 0x22, 0x26, 0x17, 0x4c, 0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c
};

const ff_asf_guid ff_asf_digital_signature = {
    0xfc, 0xb3, 0x11, 0x22, 0x23, 0xbd, 0xd2, 0x11, 0xb4, 0xb7, 0x00, 0xa0, 0xc9, 0x55, 0xfc, 0x6e
};

const ff_asf_guid ff_asf_extended_stream_properties_object = {
    0xcb, 0xa5, 0xe6, 0x14, 0x72, 0xc6, 0x32, 0x43, 0x83, 0x99, 0xa9, 0x69, 0x52, 0x06, 0x5b, 0x5a
};

const ff_asf_guid ff_asf_group_mutual_exclusion_object = {
    0x40, 0x5a, 0x46, 0xd1, 0x79, 0x5a, 0x38, 0x43, 0xb7, 0x1b, 0xe3, 0x6b, 0x8f, 0xd6, 0xc2, 0x49
};

const ff_asf_guid ff_asf_mutex_language = {
    0x00, 0x2a, 0xe2, 0xd6, 0xda, 0x35, 0xd1, 0x11, 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe
};

/* List of official tags at http://msdn.microsoft.com/en-us/library/dd743066(VS.85).aspx */
const AVMetadataConv ff_asf_metadata_conv[] = {
    { "WM/AlbumArtist",          "album_artist"     },
    { "WM/AlbumTitle",           "album"            },
    { "Author",                  "artist"           },
    { "Description",             "comment"          },
    { "WM/Composer",             "composer"         },
    { "WM/EncodedBy",            "encoded_by"       },
    { "WM/EncodingSettings",     "encoder"          },
    { "WM/Genre",                "genre"            },
    { "WM/Language",             "language"         },
    { "WM/OriginalFilename",     "filename"         },
    { "WM/PartOfSet",            "disc"             },
    { "WM/Publisher",            "publisher"        },
    { "WM/Tool",                 "encoder"          },
    { "WM/TrackNumber",          "track"            },
    { "WM/MediaStationCallSign", "service_provider" },
    { "WM/MediaStationName",     "service_name"     },
//  { "Year"               , "date"        }, TODO: conversion year<->date
    { 0 }
};

/* MSDN claims that this should be "compatible with the ID3 frame, APIC",
 * but in reality this is only loosely similar */
static int asf_read_picture(AVFormatContext *s, int len)
{
    const CodecMime *mime = ff_id3v2_mime_tags;
    enum  AVCodecID id    = AV_CODEC_ID_NONE;
    char mimetype[64];
    uint8_t  *desc = NULL;
    AVStream   *st = NULL;
    int ret, type, picsize, desc_len;

    /* type + picsize + mime + desc */
    if (len < 1 + 4 + 2 + 2) {
        av_log(s, AV_LOG_ERROR, "Invalid attached picture size: %d.\n", len);
        return AVERROR_INVALIDDATA;
    }

    /* picture type */
    type = avio_r8(s->pb);
    len--;
    if (type >= FF_ARRAY_ELEMS(ff_id3v2_picture_types) || type < 0) {
        av_log(s, AV_LOG_WARNING, "Unknown attached picture type: %d.\n", type);
        type = 0;
    }

    /* picture data size */
    picsize = avio_rl32(s->pb);
    len    -= 4;

    /* picture MIME type */
    len -= avio_get_str16le(s->pb, len, mimetype, sizeof(mimetype));
    while (mime->id != AV_CODEC_ID_NONE) {
        if (!strncmp(mime->str, mimetype, sizeof(mimetype))) {
            id = mime->id;
            break;
        }
        mime++;
    }
    if (id == AV_CODEC_ID_NONE) {
        av_log(s, AV_LOG_ERROR, "Unknown attached picture mimetype: %s.\n",
               mimetype);
        return 0;
    }

    if (picsize >= len) {
        av_log(s, AV_LOG_ERROR, "Invalid attached picture data size: %d >= %d.\n",
               picsize, len);
        return AVERROR_INVALIDDATA;
    }

    /* picture description */
    desc_len = (len - picsize) * 2 + 1;
    desc     = av_malloc(desc_len);
    if (!desc)
        return AVERROR(ENOMEM);
    len -= avio_get_str16le(s->pb, len - picsize, desc, desc_len);

    ret = ff_add_attached_pic(s, NULL, s->pb, NULL, picsize);
    if (ret < 0)
        goto fail;
    st = s->streams[s->nb_streams - 1];

    st->codecpar->codec_id        = id;

    if (*desc) {
        if (av_dict_set(&st->metadata, "title", desc, AV_DICT_DONT_STRDUP_VAL) < 0)
            av_log(s, AV_LOG_WARNING, "av_dict_set failed.\n");
    } else
        av_freep(&desc);

    if (av_dict_set(&st->metadata, "comment", ff_id3v2_picture_types[type], 0) < 0)
        av_log(s, AV_LOG_WARNING, "av_dict_set failed.\n");

    return 0;

fail:
    av_freep(&desc);
    return ret;
}

static int get_id3_tag(AVFormatContext *s, int len)
{
    ID3v2ExtraMeta *id3v2_extra_meta;

    ff_id3v2_read(s, ID3v2_DEFAULT_MAGIC, &id3v2_extra_meta, len);
    if (id3v2_extra_meta) {
        ff_id3v2_parse_apic(s, id3v2_extra_meta);
        ff_id3v2_parse_chapters(s, id3v2_extra_meta);
        ff_id3v2_free_extra_meta(&id3v2_extra_meta);
    }
    return 0;
}

int ff_asf_handle_byte_array(AVFormatContext *s, const char *name,
                             int val_len)
{
    if (!strcmp(name, "WM/Picture")) // handle cover art
        return asf_read_picture(s, val_len);
    else if (!strcmp(name, "ID3")) // handle ID3 tag
        return get_id3_tag(s, val_len);

    return 1;
}
