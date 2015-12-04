/*
 * Copyright (c) 2012 Anton Khirnov
 *
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

/*
 * generate texinfo manpages for avoptions
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#include "libavutil/attributes.h"
#include "libavutil/opt.h"

/* Forcibly turn off deprecation warnings, which just add noise here. */
#undef attribute_deprecated
#define attribute_deprecated

#include "libavcodec/options_table.h"

#include "libavformat/options_table.h"

static void print_usage(void)
{
    fprintf(stderr, "Usage: enum_options type\n"
            "type: format codec\n");
    exit(1);
}

static void print_option(const AVOption *opts, const AVOption *o, int per_stream)
{
    if (!(o->flags & (AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_ENCODING_PARAM)))
        return;

    printf("@item -%s%s @var{", o->name, per_stream ? "[:stream_specifier]" : "");
    switch (o->type) {
    case AV_OPT_TYPE_BINARY:   printf("hexadecimal string"); break;
    case AV_OPT_TYPE_STRING:   printf("string");             break;
    case AV_OPT_TYPE_INT:
    case AV_OPT_TYPE_INT64:    printf("integer");            break;
    case AV_OPT_TYPE_FLOAT:
    case AV_OPT_TYPE_DOUBLE:   printf("float");              break;
    case AV_OPT_TYPE_RATIONAL: printf("rational number");    break;
    case AV_OPT_TYPE_FLAGS:    printf("flags");              break;
    default:                   printf("value");              break;
    }
    printf("} (@emph{");

    if (o->flags & AV_OPT_FLAG_DECODING_PARAM) {
        printf("input");
        if (o->flags & AV_OPT_FLAG_ENCODING_PARAM)
            printf("/");
    }
    if (o->flags & AV_OPT_FLAG_ENCODING_PARAM) printf("output");
    if (o->flags & AV_OPT_FLAG_AUDIO_PARAM)    printf(",audio");
    if (o->flags & AV_OPT_FLAG_VIDEO_PARAM)    printf(",video");
    if (o->flags & AV_OPT_FLAG_SUBTITLE_PARAM) printf(",subtitles");

    printf("})\n");
    if (o->help)
        printf("%s\n", o->help);

    if (o->unit) {
        const AVOption *u;
        printf("\nPossible values:\n@table @samp\n");

        for (u = opts; u->name; u++) {
            if (u->type == AV_OPT_TYPE_CONST && u->unit && !strcmp(u->unit, o->unit))
                printf("@item %s\n%s\n", u->name, u->help ? u->help : "");
        }
        printf("@end table\n");
    }
}

static void show_opts(const AVOption *opts, int per_stream)
{
    const AVOption *o;

    printf("@table @option\n");
    for (o = opts; o->name; o++) {
        if (o->type != AV_OPT_TYPE_CONST)
            print_option(opts, o, per_stream);
    }
    printf("@end table\n");
}

static void show_format_opts(void)
{
    printf("@section Format AVOptions\n");
    show_opts(avformat_options, 0);
}

static void show_codec_opts(void)
{
    printf("@section Codec AVOptions\n");
    show_opts(avcodec_options, 1);
}

int main(int argc, char **argv)
{
    if (argc < 2)
        print_usage();

    if (!strcmp(argv[1], "format"))
        show_format_opts();
    else if (!strcmp(argv[1], "codec"))
        show_codec_opts();
    else
        print_usage();

    return 0;
}
