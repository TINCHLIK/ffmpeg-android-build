/*
 * Copyright (c) 2000-2003 Fabrice Bellard
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
 * multimedia converter based on the FFmpeg libraries
 */

#include "config.h"

#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_RESOURCE_H
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#elif HAVE_GETPROCESSTIMES
#include <windows.h>
#endif
#if HAVE_GETPROCESSMEMORYINFO
#include <windows.h>
#include <psapi.h>
#endif
#if HAVE_SETCONSOLECTRLHANDLER
#include <windows.h>
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if HAVE_TERMIOS_H
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#elif HAVE_KBHIT
#include <conio.h>
#endif

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/channel_layout.h"
#include "libavutil/dict.h"
#include "libavutil/display.h"
#include "libavutil/fifo.h"
#include "libavutil/hwcontext.h"
#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/libm.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/samplefmt.h"
#include "libavutil/thread.h"
#include "libavutil/threadmessage.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"

#include "libavcodec/version.h"

#include "libavformat/avformat.h"

#include "libavdevice/avdevice.h"

#include "libswresample/swresample.h"

#include "cmdutils.h"
#include "ffmpeg.h"
#include "ffmpeg_sched.h"
#include "ffmpeg_utils.h"
#include "sync_queue.h"

const char program_name[] = "ffmpeg";
const int program_birth_year = 2000;

FILE *vstats_file;

typedef struct BenchmarkTimeStamps {
    int64_t real_usec;
    int64_t user_usec;
    int64_t sys_usec;
} BenchmarkTimeStamps;

static BenchmarkTimeStamps get_benchmark_time_stamps(void);
static int64_t getmaxrss(void);

atomic_uint nb_output_dumped = 0;

static BenchmarkTimeStamps current_time;
AVIOContext *progress_avio = NULL;

InputFile   **input_files   = NULL;
int        nb_input_files   = 0;

OutputFile   **output_files   = NULL;
int         nb_output_files   = 0;

FilterGraph **filtergraphs;
int        nb_filtergraphs;

#if HAVE_TERMIOS_H

/* init terminal so that we can grab keys */
static struct termios oldtty;
static int restore_tty;
#endif

static void term_exit_sigsafe(void)
{
#if HAVE_TERMIOS_H
    if(restore_tty)
        tcsetattr (0, TCSANOW, &oldtty);
#endif
}

void term_exit(void)
{
    av_log(NULL, AV_LOG_QUIET, "%s", "");
    term_exit_sigsafe();
}

static volatile int received_sigterm = 0;
static volatile int received_nb_signals = 0;
static atomic_int transcode_init_done = ATOMIC_VAR_INIT(0);
static volatile int ffmpeg_exited = 0;
static int64_t copy_ts_first_pts = AV_NOPTS_VALUE;

static void
sigterm_handler(int sig)
{
    int ret;
    received_sigterm = sig;
    received_nb_signals++;
    term_exit_sigsafe();
    if(received_nb_signals > 3) {
        ret = write(2/*STDERR_FILENO*/, "Received > 3 system signals, hard exiting\n",
                    strlen("Received > 3 system signals, hard exiting\n"));
        if (ret < 0) { /* Do nothing */ };
        exit(123);
    }
}

#if HAVE_SETCONSOLECTRLHANDLER
static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    av_log(NULL, AV_LOG_DEBUG, "\nReceived windows signal %ld\n", fdwCtrlType);

    switch (fdwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        sigterm_handler(SIGINT);
        return TRUE;

    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        sigterm_handler(SIGTERM);
        /* Basically, with these 3 events, when we return from this method the
           process is hard terminated, so stall as long as we need to
           to try and let the main thread(s) clean up and gracefully terminate
           (we have at most 5 seconds, but should be done far before that). */
        while (!ffmpeg_exited) {
            Sleep(0);
        }
        return TRUE;

    default:
        av_log(NULL, AV_LOG_ERROR, "Received unknown windows signal %ld\n", fdwCtrlType);
        return FALSE;
    }
}
#endif

#ifdef __linux__
#define SIGNAL(sig, func)               \
    do {                                \
        action.sa_handler = func;       \
        sigaction(sig, &action, NULL);  \
    } while (0)
#else
#define SIGNAL(sig, func) \
    signal(sig, func)
#endif

void term_init(void)
{
#if defined __linux__
    struct sigaction action = {0};
    action.sa_handler = sigterm_handler;

    /* block other interrupts while processing this one */
    sigfillset(&action.sa_mask);

    /* restart interruptible functions (i.e. don't fail with EINTR)  */
    action.sa_flags = SA_RESTART;
#endif

#if HAVE_TERMIOS_H
    if (stdin_interaction) {
        struct termios tty;
        if (tcgetattr (0, &tty) == 0) {
            oldtty = tty;
            restore_tty = 1;

            tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                             |INLCR|IGNCR|ICRNL|IXON);
            tty.c_oflag |= OPOST;
            tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
            tty.c_cflag &= ~(CSIZE|PARENB);
            tty.c_cflag |= CS8;
            tty.c_cc[VMIN] = 1;
            tty.c_cc[VTIME] = 0;

            tcsetattr (0, TCSANOW, &tty);
        }
        SIGNAL(SIGQUIT, sigterm_handler); /* Quit (POSIX).  */
    }
#endif

    SIGNAL(SIGINT , sigterm_handler); /* Interrupt (ANSI).    */
    SIGNAL(SIGTERM, sigterm_handler); /* Termination (ANSI).  */
#ifdef SIGXCPU
    SIGNAL(SIGXCPU, sigterm_handler);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN); /* Broken pipe (POSIX). */
#endif
#if HAVE_SETCONSOLECTRLHANDLER
    SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, TRUE);
#endif
}

/* read a key without blocking */
static int read_key(void)
{
    unsigned char ch;
#if HAVE_TERMIOS_H
    int n = 1;
    struct timeval tv;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    n = select(1, &rfds, NULL, NULL, &tv);
    if (n > 0) {
        n = read(0, &ch, 1);
        if (n == 1)
            return ch;

        return n;
    }
#elif HAVE_KBHIT
#    if HAVE_PEEKNAMEDPIPE && HAVE_GETSTDHANDLE
    static int is_pipe;
    static HANDLE input_handle;
    DWORD dw, nchars;
    if(!input_handle){
        input_handle = GetStdHandle(STD_INPUT_HANDLE);
        is_pipe = !GetConsoleMode(input_handle, &dw);
    }

    if (is_pipe) {
        /* When running under a GUI, you will end here. */
        if (!PeekNamedPipe(input_handle, NULL, 0, NULL, &nchars, NULL)) {
            // input pipe may have been closed by the program that ran ffmpeg
            return -1;
        }
        //Read it
        if(nchars != 0) {
            read(0, &ch, 1);
            return ch;
        }else{
            return -1;
        }
    }
#    endif
    if(kbhit())
        return(getch());
#endif
    return -1;
}

static int decode_interrupt_cb(void *ctx)
{
    return received_nb_signals > atomic_load(&transcode_init_done);
}

const AVIOInterruptCB int_cb = { decode_interrupt_cb, NULL };

static void ffmpeg_cleanup(int ret)
{
    int i;

    if (do_benchmark) {
        int maxrss = getmaxrss() / 1024;
        av_log(NULL, AV_LOG_INFO, "bench: maxrss=%ikB\n", maxrss);
    }

    for (i = 0; i < nb_filtergraphs; i++)
        fg_free(&filtergraphs[i]);
    av_freep(&filtergraphs);

    for (i = 0; i < nb_output_files; i++)
        of_free(&output_files[i]);

    for (i = 0; i < nb_input_files; i++)
        ifile_close(&input_files[i]);

    if (vstats_file) {
        if (fclose(vstats_file))
            av_log(NULL, AV_LOG_ERROR,
                   "Error closing vstats file, loss of information possible: %s\n",
                   av_err2str(AVERROR(errno)));
    }
    av_freep(&vstats_filename);
    of_enc_stats_close();

    hw_device_free_all();

    av_freep(&filter_nbthreads);

    av_freep(&input_files);
    av_freep(&output_files);

    uninit_opts();

    avformat_network_deinit();

    if (received_sigterm) {
        av_log(NULL, AV_LOG_INFO, "Exiting normally, received signal %d.\n",
               (int) received_sigterm);
    } else if (ret && atomic_load(&transcode_init_done)) {
        av_log(NULL, AV_LOG_INFO, "Conversion failed!\n");
    }
    term_exit();
    ffmpeg_exited = 1;
}

OutputStream *ost_iter(OutputStream *prev)
{
    int of_idx  = prev ? prev->file->index : 0;
    int ost_idx = prev ? prev->index + 1  : 0;

    for (; of_idx < nb_output_files; of_idx++) {
        OutputFile *of = output_files[of_idx];
        if (ost_idx < of->nb_streams)
            return of->streams[ost_idx];

        ost_idx = 0;
    }

    return NULL;
}

InputStream *ist_iter(InputStream *prev)
{
    int if_idx  = prev ? prev->file->index : 0;
    int ist_idx = prev ? prev->index + 1  : 0;

    for (; if_idx < nb_input_files; if_idx++) {
        InputFile *f = input_files[if_idx];
        if (ist_idx < f->nb_streams)
            return f->streams[ist_idx];

        ist_idx = 0;
    }

    return NULL;
}

static int frame_data_ensure(AVBufferRef **dst, int writable)
{
    if (!*dst) {
        FrameData *fd;

        *dst = av_buffer_allocz(sizeof(*fd));
        if (!*dst)
            return AVERROR(ENOMEM);
        fd = (FrameData*)((*dst)->data);

        fd->dec.frame_num = UINT64_MAX;
        fd->dec.pts       = AV_NOPTS_VALUE;

        for (unsigned i = 0; i < FF_ARRAY_ELEMS(fd->wallclock); i++)
            fd->wallclock[i] = INT64_MIN;
    } else if (writable)
        return av_buffer_make_writable(dst);

    return 0;
}

FrameData *frame_data(AVFrame *frame)
{
    int ret = frame_data_ensure(&frame->opaque_ref, 1);
    return ret < 0 ? NULL : (FrameData*)frame->opaque_ref->data;
}

const FrameData *frame_data_c(AVFrame *frame)
{
    int ret = frame_data_ensure(&frame->opaque_ref, 0);
    return ret < 0 ? NULL : (const FrameData*)frame->opaque_ref->data;
}

FrameData *packet_data(AVPacket *pkt)
{
    int ret = frame_data_ensure(&pkt->opaque_ref, 1);
    return ret < 0 ? NULL : (FrameData*)pkt->opaque_ref->data;
}

const FrameData *packet_data_c(AVPacket *pkt)
{
    int ret = frame_data_ensure(&pkt->opaque_ref, 0);
    return ret < 0 ? NULL : (const FrameData*)pkt->opaque_ref->data;
}

void remove_avoptions(AVDictionary **a, AVDictionary *b)
{
    const AVDictionaryEntry *t = NULL;

    while ((t = av_dict_iterate(b, t))) {
        av_dict_set(a, t->key, NULL, AV_DICT_MATCH_CASE);
    }
}

int check_avoptions(AVDictionary *m)
{
    const AVDictionaryEntry *t;
    if ((t = av_dict_get(m, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_FATAL, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    return 0;
}

void update_benchmark(const char *fmt, ...)
{
    if (do_benchmark_all) {
        BenchmarkTimeStamps t = get_benchmark_time_stamps();
        va_list va;
        char buf[1024];

        if (fmt) {
            va_start(va, fmt);
            vsnprintf(buf, sizeof(buf), fmt, va);
            va_end(va);
            av_log(NULL, AV_LOG_INFO,
                   "bench: %8" PRIu64 " user %8" PRIu64 " sys %8" PRIu64 " real %s \n",
                   t.user_usec - current_time.user_usec,
                   t.sys_usec - current_time.sys_usec,
                   t.real_usec - current_time.real_usec, buf);
        }
        current_time = t;
    }
}

static void print_report(int is_last_report, int64_t timer_start, int64_t cur_time, int64_t pts)
{
    AVBPrint buf, buf_script;
    int64_t total_size = of_filesize(output_files[0]);
    int vid;
    double bitrate;
    double speed;
    static int64_t last_time = -1;
    static int first_report = 1;
    uint64_t nb_frames_dup = 0, nb_frames_drop = 0;
    int mins, secs, us;
    int64_t hours;
    const char *hours_sign;
    int ret;
    float t;

    if (!print_stats && !is_last_report && !progress_avio)
        return;

    if (!is_last_report) {
        if (last_time == -1) {
            last_time = cur_time;
        }
        if (((cur_time - last_time) < stats_period && !first_report) ||
            (first_report && atomic_load(&nb_output_dumped) < nb_output_files))
            return;
        last_time = cur_time;
    }

    t = (cur_time-timer_start) / 1000000.0;

    vid = 0;
    av_bprint_init(&buf, 0, AV_BPRINT_SIZE_AUTOMATIC);
    av_bprint_init(&buf_script, 0, AV_BPRINT_SIZE_AUTOMATIC);
    for (OutputStream *ost = ost_iter(NULL); ost; ost = ost_iter(ost)) {
        const float q = ost->enc ? atomic_load(&ost->quality) / (float) FF_QP2LAMBDA : -1;

        if (vid && ost->type == AVMEDIA_TYPE_VIDEO) {
            av_bprintf(&buf, "q=%2.1f ", q);
            av_bprintf(&buf_script, "stream_%d_%d_q=%.1f\n",
                       ost->file->index, ost->index, q);
        }
        if (!vid && ost->type == AVMEDIA_TYPE_VIDEO && ost->filter) {
            float fps;
            uint64_t frame_number = atomic_load(&ost->packets_written);

            fps = t > 1 ? frame_number / t : 0;
            av_bprintf(&buf, "frame=%5"PRId64" fps=%3.*f q=%3.1f ",
                     frame_number, fps < 9.95, fps, q);
            av_bprintf(&buf_script, "frame=%"PRId64"\n", frame_number);
            av_bprintf(&buf_script, "fps=%.2f\n", fps);
            av_bprintf(&buf_script, "stream_%d_%d_q=%.1f\n",
                       ost->file->index, ost->index, q);
            if (is_last_report)
                av_bprintf(&buf, "L");

            nb_frames_dup  = atomic_load(&ost->filter->nb_frames_dup);
            nb_frames_drop = atomic_load(&ost->filter->nb_frames_drop);

            vid = 1;
        }
    }

    if (copy_ts) {
        if (copy_ts_first_pts == AV_NOPTS_VALUE && pts > 1)
            copy_ts_first_pts = pts;
        if (copy_ts_first_pts != AV_NOPTS_VALUE)
            pts -= copy_ts_first_pts;
    }

    us    = FFABS64U(pts) % AV_TIME_BASE;
    secs  = FFABS64U(pts) / AV_TIME_BASE % 60;
    mins  = FFABS64U(pts) / AV_TIME_BASE / 60 % 60;
    hours = FFABS64U(pts) / AV_TIME_BASE / 3600;
    hours_sign = (pts < 0) ? "-" : "";

    bitrate = pts != AV_NOPTS_VALUE && pts && total_size >= 0 ? total_size * 8 / (pts / 1000.0) : -1;
    speed   = pts != AV_NOPTS_VALUE && t != 0.0 ? (double)pts / AV_TIME_BASE / t : -1;

    if (total_size < 0) av_bprintf(&buf, "size=N/A time=");
    else                av_bprintf(&buf, "size=%8.0fkB time=", total_size / 1024.0);
    if (pts == AV_NOPTS_VALUE) {
        av_bprintf(&buf, "N/A ");
    } else {
        av_bprintf(&buf, "%s%02"PRId64":%02d:%02d.%02d ",
                   hours_sign, hours, mins, secs, (100 * us) / AV_TIME_BASE);
    }

    if (bitrate < 0) {
        av_bprintf(&buf, "bitrate=N/A");
        av_bprintf(&buf_script, "bitrate=N/A\n");
    }else{
        av_bprintf(&buf, "bitrate=%6.1fkbits/s", bitrate);
        av_bprintf(&buf_script, "bitrate=%6.1fkbits/s\n", bitrate);
    }

    if (total_size < 0) av_bprintf(&buf_script, "total_size=N/A\n");
    else                av_bprintf(&buf_script, "total_size=%"PRId64"\n", total_size);
    if (pts == AV_NOPTS_VALUE) {
        av_bprintf(&buf_script, "out_time_us=N/A\n");
        av_bprintf(&buf_script, "out_time_ms=N/A\n");
        av_bprintf(&buf_script, "out_time=N/A\n");
    } else {
        av_bprintf(&buf_script, "out_time_us=%"PRId64"\n", pts);
        av_bprintf(&buf_script, "out_time_ms=%"PRId64"\n", pts);
        av_bprintf(&buf_script, "out_time=%s%02"PRId64":%02d:%02d.%06d\n",
                   hours_sign, hours, mins, secs, us);
    }

    if (nb_frames_dup || nb_frames_drop)
        av_bprintf(&buf, " dup=%"PRId64" drop=%"PRId64, nb_frames_dup, nb_frames_drop);
    av_bprintf(&buf_script, "dup_frames=%"PRId64"\n", nb_frames_dup);
    av_bprintf(&buf_script, "drop_frames=%"PRId64"\n", nb_frames_drop);

    if (speed < 0) {
        av_bprintf(&buf, " speed=N/A");
        av_bprintf(&buf_script, "speed=N/A\n");
    } else {
        av_bprintf(&buf, " speed=%4.3gx", speed);
        av_bprintf(&buf_script, "speed=%4.3gx\n", speed);
    }

    if (print_stats || is_last_report) {
        const char end = is_last_report ? '\n' : '\r';
        if (print_stats==1 && AV_LOG_INFO > av_log_get_level()) {
            fprintf(stderr, "%s    %c", buf.str, end);
        } else
            av_log(NULL, AV_LOG_INFO, "%s    %c", buf.str, end);

        fflush(stderr);
    }
    av_bprint_finalize(&buf, NULL);

    if (progress_avio) {
        av_bprintf(&buf_script, "progress=%s\n",
                   is_last_report ? "end" : "continue");
        avio_write(progress_avio, buf_script.str,
                   FFMIN(buf_script.len, buf_script.size - 1));
        avio_flush(progress_avio);
        av_bprint_finalize(&buf_script, NULL);
        if (is_last_report) {
            if ((ret = avio_closep(&progress_avio)) < 0)
                av_log(NULL, AV_LOG_ERROR,
                       "Error closing progress log, loss of information possible: %s\n", av_err2str(ret));
        }
    }

    first_report = 0;
}

int copy_av_subtitle(AVSubtitle *dst, const AVSubtitle *src)
{
    int ret = AVERROR_BUG;
    AVSubtitle tmp = {
        .format = src->format,
        .start_display_time = src->start_display_time,
        .end_display_time = src->end_display_time,
        .num_rects = 0,
        .rects = NULL,
        .pts = src->pts
    };

    if (!src->num_rects)
        goto success;

    if (!(tmp.rects = av_calloc(src->num_rects, sizeof(*tmp.rects))))
        return AVERROR(ENOMEM);

    for (int i = 0; i < src->num_rects; i++) {
        AVSubtitleRect *src_rect = src->rects[i];
        AVSubtitleRect *dst_rect;

        if (!(dst_rect = tmp.rects[i] = av_mallocz(sizeof(*tmp.rects[0])))) {
            ret = AVERROR(ENOMEM);
            goto cleanup;
        }

        tmp.num_rects++;

        dst_rect->type      = src_rect->type;
        dst_rect->flags     = src_rect->flags;

        dst_rect->x         = src_rect->x;
        dst_rect->y         = src_rect->y;
        dst_rect->w         = src_rect->w;
        dst_rect->h         = src_rect->h;
        dst_rect->nb_colors = src_rect->nb_colors;

        if (src_rect->text)
            if (!(dst_rect->text = av_strdup(src_rect->text))) {
                ret = AVERROR(ENOMEM);
                goto cleanup;
            }

        if (src_rect->ass)
            if (!(dst_rect->ass = av_strdup(src_rect->ass))) {
                ret = AVERROR(ENOMEM);
                goto cleanup;
            }

        for (int j = 0; j < 4; j++) {
            // SUBTITLE_BITMAP images are special in the sense that they
            // are like PAL8 images. first pointer to data, second to
            // palette. This makes the size calculation match this.
            size_t buf_size = src_rect->type == SUBTITLE_BITMAP && j == 1 ?
                              AVPALETTE_SIZE :
                              src_rect->h * src_rect->linesize[j];

            if (!src_rect->data[j])
                continue;

            if (!(dst_rect->data[j] = av_memdup(src_rect->data[j], buf_size))) {
                ret = AVERROR(ENOMEM);
                goto cleanup;
            }
            dst_rect->linesize[j] = src_rect->linesize[j];
        }
    }

success:
    *dst = tmp;

    return 0;

cleanup:
    avsubtitle_free(&tmp);

    return ret;
}

static void subtitle_free(void *opaque, uint8_t *data)
{
    AVSubtitle *sub = (AVSubtitle*)data;
    avsubtitle_free(sub);
    av_free(sub);
}

int subtitle_wrap_frame(AVFrame *frame, AVSubtitle *subtitle, int copy)
{
    AVBufferRef *buf;
    AVSubtitle *sub;
    int ret;

    if (copy) {
        sub = av_mallocz(sizeof(*sub));
        ret = sub ? copy_av_subtitle(sub, subtitle) : AVERROR(ENOMEM);
        if (ret < 0) {
            av_freep(&sub);
            return ret;
        }
    } else {
        sub = av_memdup(subtitle, sizeof(*subtitle));
        if (!sub)
            return AVERROR(ENOMEM);
        memset(subtitle, 0, sizeof(*subtitle));
    }

    buf = av_buffer_create((uint8_t*)sub, sizeof(*sub),
                           subtitle_free, NULL, 0);
    if (!buf) {
        avsubtitle_free(sub);
        av_freep(&sub);
        return AVERROR(ENOMEM);
    }

    frame->buf[0] = buf;

    return 0;
}

static void print_stream_maps(void)
{
    av_log(NULL, AV_LOG_INFO, "Stream mapping:\n");
    for (InputStream *ist = ist_iter(NULL); ist; ist = ist_iter(ist)) {
        for (int j = 0; j < ist->nb_filters; j++) {
            if (!filtergraph_is_simple(ist->filters[j]->graph)) {
                av_log(NULL, AV_LOG_INFO, "  Stream #%d:%d (%s) -> %s",
                       ist->file->index, ist->index, ist->dec ? ist->dec->name : "?",
                       ist->filters[j]->name);
                if (nb_filtergraphs > 1)
                    av_log(NULL, AV_LOG_INFO, " (graph %d)", ist->filters[j]->graph->index);
                av_log(NULL, AV_LOG_INFO, "\n");
            }
        }
    }

    for (OutputStream *ost = ost_iter(NULL); ost; ost = ost_iter(ost)) {
        if (ost->attachment_filename) {
            /* an attached file */
            av_log(NULL, AV_LOG_INFO, "  File %s -> Stream #%d:%d\n",
                   ost->attachment_filename, ost->file->index, ost->index);
            continue;
        }

        if (ost->filter && !filtergraph_is_simple(ost->filter->graph)) {
            /* output from a complex graph */
            av_log(NULL, AV_LOG_INFO, "  %s", ost->filter->name);
            if (nb_filtergraphs > 1)
                av_log(NULL, AV_LOG_INFO, " (graph %d)", ost->filter->graph->index);

            av_log(NULL, AV_LOG_INFO, " -> Stream #%d:%d (%s)\n", ost->file->index,
                   ost->index, ost->enc_ctx->codec->name);
            continue;
        }

        av_log(NULL, AV_LOG_INFO, "  Stream #%d:%d -> #%d:%d",
               ost->ist->file->index,
               ost->ist->index,
               ost->file->index,
               ost->index);
        if (ost->enc_ctx) {
            const AVCodec *in_codec    = ost->ist->dec;
            const AVCodec *out_codec   = ost->enc_ctx->codec;
            const char *decoder_name   = "?";
            const char *in_codec_name  = "?";
            const char *encoder_name   = "?";
            const char *out_codec_name = "?";
            const AVCodecDescriptor *desc;

            if (in_codec) {
                decoder_name  = in_codec->name;
                desc = avcodec_descriptor_get(in_codec->id);
                if (desc)
                    in_codec_name = desc->name;
                if (!strcmp(decoder_name, in_codec_name))
                    decoder_name = "native";
            }

            if (out_codec) {
                encoder_name   = out_codec->name;
                desc = avcodec_descriptor_get(out_codec->id);
                if (desc)
                    out_codec_name = desc->name;
                if (!strcmp(encoder_name, out_codec_name))
                    encoder_name = "native";
            }

            av_log(NULL, AV_LOG_INFO, " (%s (%s) -> %s (%s))",
                   in_codec_name, decoder_name,
                   out_codec_name, encoder_name);
        } else
            av_log(NULL, AV_LOG_INFO, " (copy)");
        av_log(NULL, AV_LOG_INFO, "\n");
    }
}

static void set_tty_echo(int on)
{
#if HAVE_TERMIOS_H
    struct termios tty;
    if (tcgetattr(0, &tty) == 0) {
        if (on) tty.c_lflag |= ECHO;
        else    tty.c_lflag &= ~ECHO;
        tcsetattr(0, TCSANOW, &tty);
    }
#endif
}

static int check_keyboard_interaction(int64_t cur_time)
{
    int i, key;
    static int64_t last_time;
    if (received_nb_signals)
        return AVERROR_EXIT;
    /* read_key() returns 0 on EOF */
    if (cur_time - last_time >= 100000) {
        key =  read_key();
        last_time = cur_time;
    }else
        key = -1;
    if (key == 'q') {
        av_log(NULL, AV_LOG_INFO, "\n\n[q] command received. Exiting.\n\n");
        return AVERROR_EXIT;
    }
    if (key == '+') av_log_set_level(av_log_get_level()+10);
    if (key == '-') av_log_set_level(av_log_get_level()-10);
    if (key == 'c' || key == 'C'){
        char buf[4096], target[64], command[256], arg[256] = {0};
        double time;
        int k, n = 0;
        fprintf(stderr, "\nEnter command: <target>|all <time>|-1 <command>[ <argument>]\n");
        i = 0;
        set_tty_echo(1);
        while ((k = read_key()) != '\n' && k != '\r' && i < sizeof(buf)-1)
            if (k > 0)
                buf[i++] = k;
        buf[i] = 0;
        set_tty_echo(0);
        fprintf(stderr, "\n");
        if (k > 0 &&
            (n = sscanf(buf, "%63[^ ] %lf %255[^ ] %255[^\n]", target, &time, command, arg)) >= 3) {
            av_log(NULL, AV_LOG_DEBUG, "Processing command target:%s time:%f command:%s arg:%s",
                   target, time, command, arg);
            for (i = 0; i < nb_filtergraphs; i++)
                fg_send_command(filtergraphs[i], time, target, command, arg,
                                key == 'C');
        } else {
            av_log(NULL, AV_LOG_ERROR,
                   "Parse error, at least 3 arguments were expected, "
                   "only %d given in string '%s'\n", n, buf);
        }
    }
    if (key == '?'){
        fprintf(stderr, "key    function\n"
                        "?      show this help\n"
                        "+      increase verbosity\n"
                        "-      decrease verbosity\n"
                        "c      Send command to first matching filter supporting it\n"
                        "C      Send/Queue command to all matching filters\n"
                        "h      dump packets/hex press to cycle through the 3 states\n"
                        "q      quit\n"
                        "s      Show QP histogram\n"
        );
    }
    return 0;
}

/*
 * The following code is the main loop of the file converter
 */
static int transcode(Scheduler *sch)
{
    int ret = 0, i;
    int64_t timer_start, transcode_ts = 0;

    print_stream_maps();

    atomic_store(&transcode_init_done, 1);

    ret = sch_start(sch);
    if (ret < 0)
        return ret;

    if (stdin_interaction) {
        av_log(NULL, AV_LOG_INFO, "Press [q] to stop, [?] for help\n");
    }

    timer_start = av_gettime_relative();

    while (!sch_wait(sch, stats_period, &transcode_ts)) {
        int64_t cur_time= av_gettime_relative();

        /* if 'q' pressed, exits */
        if (stdin_interaction)
            if (check_keyboard_interaction(cur_time) < 0)
                break;

        /* dump report by using the output first video and audio streams */
        print_report(0, timer_start, cur_time, transcode_ts);
    }

    ret = sch_stop(sch);

    /* write the trailer if needed */
    for (i = 0; i < nb_output_files; i++) {
        int err = of_write_trailer(output_files[i]);
        ret = err_merge(ret, err);
    }

    term_exit();

    /* dump report by using the first video and audio streams */
    print_report(1, timer_start, av_gettime_relative(), transcode_ts);

    return ret;
}

static BenchmarkTimeStamps get_benchmark_time_stamps(void)
{
    BenchmarkTimeStamps time_stamps = { av_gettime_relative() };
#if HAVE_GETRUSAGE
    struct rusage rusage;

    getrusage(RUSAGE_SELF, &rusage);
    time_stamps.user_usec =
        (rusage.ru_utime.tv_sec * 1000000LL) + rusage.ru_utime.tv_usec;
    time_stamps.sys_usec =
        (rusage.ru_stime.tv_sec * 1000000LL) + rusage.ru_stime.tv_usec;
#elif HAVE_GETPROCESSTIMES
    HANDLE proc;
    FILETIME c, e, k, u;
    proc = GetCurrentProcess();
    GetProcessTimes(proc, &c, &e, &k, &u);
    time_stamps.user_usec =
        ((int64_t)u.dwHighDateTime << 32 | u.dwLowDateTime) / 10;
    time_stamps.sys_usec =
        ((int64_t)k.dwHighDateTime << 32 | k.dwLowDateTime) / 10;
#else
    time_stamps.user_usec = time_stamps.sys_usec = 0;
#endif
    return time_stamps;
}

static int64_t getmaxrss(void)
{
#if HAVE_GETRUSAGE && HAVE_STRUCT_RUSAGE_RU_MAXRSS
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    return (int64_t)rusage.ru_maxrss * 1024;
#elif HAVE_GETPROCESSMEMORYINFO
    HANDLE proc;
    PROCESS_MEMORY_COUNTERS memcounters;
    proc = GetCurrentProcess();
    memcounters.cb = sizeof(memcounters);
    GetProcessMemoryInfo(proc, &memcounters, sizeof(memcounters));
    return memcounters.PeakPagefileUsage;
#else
    return 0;
#endif
}

int main(int argc, char **argv)
{
    Scheduler *sch = NULL;

    int ret;
    BenchmarkTimeStamps ti;

    init_dynload();

    setvbuf(stderr,NULL,_IONBF,0); /* win32 runtime needs this */

    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
    avformat_network_init();

    show_banner(argc, argv, options);

    sch = sch_alloc();
    if (!sch) {
        ret = AVERROR(ENOMEM);
        goto finish;
    }

    /* parse options and open all input/output files */
    ret = ffmpeg_parse_options(argc, argv, sch);
    if (ret < 0)
        goto finish;

    if (nb_output_files <= 0 && nb_input_files == 0) {
        show_usage();
        av_log(NULL, AV_LOG_WARNING, "Use -h to get full help or, even better, run 'man %s'\n", program_name);
        ret = 1;
        goto finish;
    }

    if (nb_output_files <= 0) {
        av_log(NULL, AV_LOG_FATAL, "At least one output file must be specified\n");
        ret = 1;
        goto finish;
    }

    current_time = ti = get_benchmark_time_stamps();
    ret = transcode(sch);
    if (ret >= 0 && do_benchmark) {
        int64_t utime, stime, rtime;
        current_time = get_benchmark_time_stamps();
        utime = current_time.user_usec - ti.user_usec;
        stime = current_time.sys_usec  - ti.sys_usec;
        rtime = current_time.real_usec - ti.real_usec;
        av_log(NULL, AV_LOG_INFO,
               "bench: utime=%0.3fs stime=%0.3fs rtime=%0.3fs\n",
               utime / 1000000.0, stime / 1000000.0, rtime / 1000000.0);
    }

    ret = received_nb_signals                 ? 255 :
          (ret == FFMPEG_ERROR_RATE_EXCEEDED) ?  69 : ret;

finish:
    if (ret == AVERROR_EXIT)
        ret = 0;

    ffmpeg_cleanup(ret);

    sch_free(&sch);

    return ret;
}
