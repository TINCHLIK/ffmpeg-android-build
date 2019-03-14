#!/bin/sh
#
# automatic regression test for libavformat
#
#
#set -x

set -e

. $(dirname $0)/regression-funcs.sh

eval do_$test=y

ENC_OPTS="$ENC_OPTS -metadata title=lavftest"

do_lavf_fate()
{
    file=${outfile}lavf.$1
    input="${target_samples}/$2"
    do_avconv $file $DEC_OPTS -i "$input" $ENC_OPTS -vcodec copy -acodec copy
    do_avconv_crc $file $DEC_OPTS -i $target_path/$file $3
}

do_lavf()
{
    file=${outfile}lavf.$1
    do_avconv $file $DEC_OPTS -f image2 -c:v pgmyuv -i $raw_src $DEC_OPTS -ar 44100 -f s16le $2 -i $pcm_src $ENC_OPTS -b:a 64k -t 1 -qscale:v 10 $3
    test $5 = "disable_crc" ||
        do_avconv_crc $file $DEC_OPTS -i $target_path/$file $4
}

do_lavf_timecode_nodrop() { do_lavf $1 "" "$2 -timecode 02:56:14:13"; }
do_lavf_timecode_drop()   { do_lavf $1 "" "$2 -timecode 02:56:14.13 -r 30000/1001"; }

do_lavf_timecode()
{
    do_lavf_timecode_nodrop "$@"
    do_lavf_timecode_drop "$@"
    do_lavf $1 "" "$2"
}

if [ -n "$do_avi" ] ; then
do_lavf avi "" "-acodec mp2 -ar 44100 -ab 64k -threads 1"
fi

if [ -n "$do_asf" ] ; then
do_lavf asf "" "-acodec mp2 -ar 44100 -ab 64k" "-r 25"
fi

if [ -n "$do_rm" ] ; then
file=${outfile}lavf.rm
# The RealMedia muxer is broken.
do_lavf rm "" "-c:a ac3_fixed" "" disable_crc
fi

if [ -n "$do_mpg" ] ; then
do_lavf_timecode mpg "-ab 64k -ar 44100 -threads 1"
fi

if [ -n "$do_mxf" ] ; then
do_lavf_timecode mxf "-ar 48000 -bf 2 -threads 1"
fi

if [ -n "$do_mxf_d10" ]; then
do_lavf mxf_d10 "-ar 48000 -ac 2" "-r 25 -vf scale=720:576,pad=720:608:0:32 -vcodec mpeg2video -g 0 -flags +ildct+low_delay -dc 10 -non_linear_quant 1 -intra_vlc 1 -qscale 1 -ps 1 -qmin 1 -rc_max_vbv_use 1 -rc_min_vbv_use 1 -pix_fmt yuv422p -minrate 30000k -maxrate 30000k -b 30000k -bufsize 1200000 -top 1 -rc_init_occupancy 1200000 -qmax 12 -f mxf_d10"
fi

if [ -n "$do_mxf_dv25" ]; then
do_lavf mxf_dv25 "-ar 48000 -ac 2" "-r 25 -vf scale=720:576,setdar=4/3 -vcodec dvvideo -pix_fmt yuv420p -b 25000k -top 0 -f mxf"
fi

if [ -n "$do_mxf_dvcpro50" ]; then
do_lavf mxf_dvcpro50 "-ar 48000 -ac 2" "-r 25 -vf scale=720:576,setdar=16/9 -vcodec dvvideo -pix_fmt yuv422p -b 50000k -top 0 -f mxf"
fi

if [ -n "$do_mxf_opatom" ]; then
do_lavf mxf_opatom "" "-s 1920x1080 -vcodec dnxhd -pix_fmt yuv422p -vb 36M -f mxf_opatom -map 0"
fi

if [ -n "$do_mxf_opatom_audio" ]; then
do_lavf mxf_opatom_audio "-ar 48000 -ac 1" "-f mxf_opatom -mxf_audio_edit_rate 25 -map 1"
fi

if [ -n "$do_ts" ] ; then
do_lavf ts "" "-ab 64k -mpegts_transport_stream_id 42 -ar 44100 -threads 1"
fi

if [ -n "$do_swf" ] ; then
do_lavf swf "" "-an"
fi

if [ -n "$do_ffm" ] ; then
do_lavf ffm "" "-ar 44100 -threads 1"
fi

if [ -n "$do_flm" ] ; then
do_lavf flm "" "-pix_fmt rgba"
fi

if [ -n "$do_flv_fmt" ] ; then
do_lavf flv "" "-an"
fi

if [ -n "$do_mov" ] ; then
mov_common_opt="-acodec pcm_alaw -vcodec mpeg4 -threads 1"
do_lavf mov "" "-movflags +rtphint $mov_common_opt"
do_lavf_timecode mov "-movflags +faststart $mov_common_opt"
do_lavf_timecode mp4 "-vcodec mpeg4 -an -threads 1"
fi

if [ -n "$do_ismv" ] ; then
do_lavf_timecode ismv "-an -vcodec mpeg4 -threads 1"
fi

if [ -n "$do_dv_fmt" ] ; then
do_lavf_timecode_nodrop dv "-ar 48000 -r 25 -s pal -ac 2"
do_lavf_timecode_drop   dv "-ar 48000 -pix_fmt yuv411p -s ntsc -ac 2"
do_lavf dv "-ar 48000 -channel_layout stereo" "-r 25 -s pal"
fi

if [ -n "$do_gxf" ] ; then
do_lavf_timecode_nodrop gxf "-ar 48000 -r 25 -s pal -ac 1 -threads 1"
do_lavf_timecode_drop   gxf "-ar 48000 -s ntsc -ac 1 -threads 1"
do_lavf gxf "-ar 48000" "-r 25 -s pal -ac 1 -threads 1"
fi

if [ -n "$do_nut" ] ; then
do_lavf nut "" "-acodec mp2 -ab 64k -ar 44100 -threads 1"
fi

if [ -n "$do_mkv" ] ; then
do_lavf mkv "" "-acodec mp2 -ab 64k -vcodec mpeg4 \
 -attach ${raw_src%/*}/00.pgm -metadata:s:t mimetype=image/x-portable-greymap -threads 1"
do_lavf mkv "" "-acodec mp2 -ab 64k -vcodec mpeg4 -ar 44100 -threads 1"
fi

if [ -n "$do_mp3" ] ; then
do_lavf_fate mp3 "mp3-conformance/he_32khz.bit" "-acodec copy"
fi

if [ -n "$do_latm" ] ; then
do_lavf_fate latm "aac/al04_44.mp4" "-acodec copy"
fi

if [ -n "$do_ogg_vp3" ] ; then
# -idct simple causes different results on different systems
DEC_OPTS="$DEC_OPTS -idct auto"
do_lavf_fate ogg "vp3/coeff_level64.mkv"
fi

if [ -n "$do_ogg_vp8" ] ; then
do_lavf_fate ogv "vp8/RRSF49-short.webm" "-acodec copy"
fi

if [ -n "$do_mov_qtrle_mace6" ] ; then
DEC_OPTS="$DEC_OPTS -idct auto"
do_lavf_fate mov "qtrle/Animation-16Greys.mov"
fi

if [ -n "$do_avi_cram" ] ; then
DEC_OPTS="$DEC_OPTS -idct auto"
do_lavf_fate avi "cram/toon.avi"
fi

if [ -n "$do_wtv" ] ; then
do_lavf wtv "" "-acodec mp2 -threads 1"
fi


# streamed images
# mjpeg
#file=${outfile}lavf.mjpeg
#do_avconv $file -t 1 -qscale 10 -f image2 -vcodec pgmyuv -i $raw_src
#do_avconv_crc $file -i $target_path/$file

if [ -n "$do_gif" ] ; then
file=${outfile}lavf.gif
do_avconv $file $DEC_OPTS -f image2 -vcodec pgmyuv -i $raw_src $ENC_OPTS -t 1 -qscale 10 -pix_fmt rgb24
do_avconv_crc $file $DEC_OPTS -i $target_path/$file -pix_fmt rgb24
fi

if [ -n "$do_apng" ] ; then
file=${outfile}lavf.apng
do_avconv $file $DEC_OPTS -f image2 -vcodec pgmyuv -i $raw_src $ENC_OPTS -t 1 -pix_fmt rgb24
do_avconv_crc $file $DEC_OPTS -i $target_path/$file -pix_fmt rgb24
file_copy=${outfile}lavf.copy.apng
do_avconv $file_copy $DEC_OPTS -i $file $ENC_OPTS -c copy
do_avconv_crc $file_copy $DEC_OPTS -i $target_path/$file_copy
file=${outfile}lavf.png
do_avconv $file $DEC_OPTS -f image2 -vcodec pgmyuv -i $raw_src $ENC_OPTS -pix_fmt rgb24 -frames:v 1 -f apng
do_avconv_crc $file $DEC_OPTS -i $target_path/$file -pix_fmt rgb24
fi

if [ -n "$do_yuv4mpeg" ] ; then
file=${outfile}lavf.y4m
do_avconv $file $DEC_OPTS -f image2 -vcodec pgmyuv -i $raw_src $ENC_OPTS -t 1 -qscale 10
do_avconv_crc $file -i $target_path/$file
fi

if [ -n "$do_fits" ] ; then
pix_fmts="gray gray16be gbrp gbrap gbrp16be gbrap16be"
for pix_fmt in $pix_fmts ; do
    file=${outfile}${pix_fmt}lavf.fits
    do_avconv $file $DEC_OPTS -f image2 -vcodec pgmyuv -i $raw_src $ENC_OPTS -pix_fmt $pix_fmt
    do_avconv_crc $file $DEC_OPTS -i $target_path/$file -pix_fmt $pix_fmt
done
fi

if [ -n "$do_smjpeg" ] ; then
do_lavf smjpeg "" "-f smjpeg"
fi

# pix_fmt conversions

if [ -n "$do_pixfmt" ] ; then
outfile="$datadir/pixfmt/"
conversions="yuv420p yuv422p yuv444p yuyv422 yuv410p yuv411p yuvj420p \
             yuvj422p yuvj444p rgb24 bgr24 rgb32 rgb565 rgb555 gray monow \
             monob yuv440p yuvj440p"
for pix_fmt in $conversions ; do
    file=${outfile}${pix_fmt}.yuv
    run_avconv $DEC_OPTS -r 1 -f image2 -vcodec pgmyuv -i $raw_src \
               $ENC_OPTS -f rawvideo -t 1 -s 352x288 -pix_fmt $pix_fmt $target_path/$raw_dst
    do_avconv $file $DEC_OPTS -f rawvideo -s 352x288 -pix_fmt $pix_fmt -i $target_path/$raw_dst \
                    $ENC_OPTS -f rawvideo -s 352x288 -pix_fmt yuv444p
done
fi
