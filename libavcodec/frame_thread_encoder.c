/*
 * Copyright (c) 2012 Michael Niedermayer <michaelni@gmx.at>
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

#include <stdatomic.h>

#include "frame_thread_encoder.h"

#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/thread.h"
#include "avcodec.h"
#include "internal.h"
#include "thread.h"

#define MAX_THREADS 64
/* There can be as many as MAX_THREADS + 1 outstanding tasks.
 * An additional + 1 is needed so that one can distinguish
 * the case of zero and MAX_THREADS + 1 outstanding tasks modulo
 * the number of buffers. */
#define BUFFER_SIZE (MAX_THREADS + 2)

typedef struct{
    AVFrame  *indata;
    AVPacket *outdata;
    int64_t return_code;
    int       finished;
} Task;

typedef struct{
    AVCodecContext *parent_avctx;
    pthread_mutex_t buffer_mutex;

    pthread_mutex_t task_fifo_mutex; /* Used to guard (next_)task_index */
    pthread_cond_t task_fifo_cond;

    unsigned max_tasks;
    Task tasks[BUFFER_SIZE];
    pthread_mutex_t finished_task_mutex; /* Guards tasks[i].finished */
    pthread_cond_t finished_task_cond;

    unsigned next_task_index;
    unsigned task_index;
    unsigned finished_task_index;

    pthread_t worker[MAX_THREADS];
    atomic_int exit;
} ThreadContext;

static void * attribute_align_arg worker(void *v){
    AVCodecContext *avctx = v;
    ThreadContext *c = avctx->internal->frame_thread_encoder;

    while (!atomic_load(&c->exit)) {
        int got_packet = 0, ret;
        AVPacket *pkt;
        AVFrame *frame;
        Task *task;
        unsigned task_index;

        pthread_mutex_lock(&c->task_fifo_mutex);
        while (c->next_task_index == c->task_index || atomic_load(&c->exit)) {
            if (atomic_load(&c->exit)) {
                pthread_mutex_unlock(&c->task_fifo_mutex);
                goto end;
            }
            pthread_cond_wait(&c->task_fifo_cond, &c->task_fifo_mutex);
        }
        task_index         = c->next_task_index;
        c->next_task_index = (c->next_task_index + 1) % c->max_tasks;
        pthread_mutex_unlock(&c->task_fifo_mutex);
        /* The main thread ensures that any two outstanding tasks have
         * different indices, ergo each worker thread owns its element
         * of c->tasks with the exception of finished, which is shared
         * with the main thread and guarded by finished_task_mutex. */
        task  = &c->tasks[task_index];
        frame = task->indata;
        pkt   = task->outdata;

        ret = avctx->codec->encode2(avctx, pkt, frame, &got_packet);
        if(got_packet) {
            int ret2 = av_packet_make_refcounted(pkt);
            if (ret >= 0 && ret2 < 0)
                ret = ret2;
            pkt->pts = pkt->dts = frame->pts;
        } else {
            pkt->data = NULL;
            pkt->size = 0;
        }
        pthread_mutex_lock(&c->buffer_mutex);
        av_frame_unref(frame);
        pthread_mutex_unlock(&c->buffer_mutex);
        pthread_mutex_lock(&c->finished_task_mutex);
        task->return_code = ret;
        task->finished    = 1;
        pthread_cond_signal(&c->finished_task_cond);
        pthread_mutex_unlock(&c->finished_task_mutex);
    }
end:
    pthread_mutex_lock(&c->buffer_mutex);
    avcodec_close(avctx);
    pthread_mutex_unlock(&c->buffer_mutex);
    av_freep(&avctx);
    return NULL;
}

int ff_frame_thread_encoder_init(AVCodecContext *avctx, AVDictionary *options){
    int i=0;
    ThreadContext *c;


    if(   !(avctx->thread_type & FF_THREAD_FRAME)
       || !(avctx->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS))
        return 0;

    if(   !avctx->thread_count
       && avctx->codec_id == AV_CODEC_ID_MJPEG
       && !(avctx->flags & AV_CODEC_FLAG_QSCALE)) {
        av_log(avctx, AV_LOG_DEBUG,
               "Forcing thread count to 1 for MJPEG encoding, use -thread_type slice "
               "or a constant quantizer if you want to use multiple cpu cores\n");
        avctx->thread_count = 1;
    }
    if(   avctx->thread_count > 1
       && avctx->codec_id == AV_CODEC_ID_MJPEG
       && !(avctx->flags & AV_CODEC_FLAG_QSCALE))
        av_log(avctx, AV_LOG_WARNING,
               "MJPEG CBR encoding works badly with frame multi-threading, consider "
               "using -threads 1, -thread_type slice or a constant quantizer.\n");

    if (avctx->codec_id == AV_CODEC_ID_HUFFYUV ||
        avctx->codec_id == AV_CODEC_ID_FFVHUFF) {
        int warn = 0;
        int context_model = 0;
        AVDictionaryEntry *con = av_dict_get(options, "context", NULL, AV_DICT_MATCH_CASE);

        if (con && con->value)
            context_model = atoi(con->value);

        if (avctx->flags & AV_CODEC_FLAG_PASS1)
            warn = 1;
        else if(context_model > 0) {
            AVDictionaryEntry *t = av_dict_get(options, "non_deterministic",
                                               NULL, AV_DICT_MATCH_CASE);
            warn = !t || !t->value || !atoi(t->value) ? 1 : 0;
        }
        // huffyuv does not support these with multiple frame threads currently
        if (warn) {
            av_log(avctx, AV_LOG_WARNING,
               "Forcing thread count to 1 for huffyuv encoding with first pass or context 1\n");
            avctx->thread_count = 1;
        }
    }

    if(!avctx->thread_count) {
        avctx->thread_count = av_cpu_count();
        avctx->thread_count = FFMIN(avctx->thread_count, MAX_THREADS);
    }

    if(avctx->thread_count <= 1)
        return 0;

    if(avctx->thread_count > MAX_THREADS)
        return AVERROR(EINVAL);

    av_assert0(!avctx->internal->frame_thread_encoder);
    c = avctx->internal->frame_thread_encoder = av_mallocz(sizeof(ThreadContext));
    if(!c)
        return AVERROR(ENOMEM);

    c->parent_avctx = avctx;

    pthread_mutex_init(&c->task_fifo_mutex, NULL);
    pthread_mutex_init(&c->finished_task_mutex, NULL);
    pthread_mutex_init(&c->buffer_mutex, NULL);
    pthread_cond_init(&c->task_fifo_cond, NULL);
    pthread_cond_init(&c->finished_task_cond, NULL);
    atomic_init(&c->exit, 0);

    c->max_tasks = avctx->thread_count + 2;
    for (unsigned i = 0; i < c->max_tasks; i++) {
        if (!(c->tasks[i].indata  = av_frame_alloc()) ||
            !(c->tasks[i].outdata = av_packet_alloc()))
            goto fail;
    }

    for(i=0; i<avctx->thread_count ; i++){
        AVDictionary *tmp = NULL;
        int ret;
        void *tmpv;
        AVCodecContext *thread_avctx = avcodec_alloc_context3(avctx->codec);
        if(!thread_avctx)
            goto fail;
        tmpv = thread_avctx->priv_data;
        *thread_avctx = *avctx;
        ret = av_opt_copy(thread_avctx, avctx);
        if (ret < 0)
            goto fail;
        thread_avctx->priv_data = tmpv;
        thread_avctx->internal = NULL;
        if (avctx->codec->priv_class) {
            int ret = av_opt_copy(thread_avctx->priv_data, avctx->priv_data);
            if (ret < 0)
                goto fail;
        } else if (avctx->codec->priv_data_size) {
            memcpy(thread_avctx->priv_data, avctx->priv_data, avctx->codec->priv_data_size);
        }
        thread_avctx->thread_count = 1;
        thread_avctx->active_thread_type &= ~FF_THREAD_FRAME;

        av_dict_copy(&tmp, options, 0);
        av_dict_set(&tmp, "threads", "1", 0);
        if(avcodec_open2(thread_avctx, avctx->codec, &tmp) < 0) {
            av_dict_free(&tmp);
            goto fail;
        }
        av_dict_free(&tmp);
        av_assert0(!thread_avctx->internal->frame_thread_encoder);
        thread_avctx->internal->frame_thread_encoder = c;
        if(pthread_create(&c->worker[i], NULL, worker, thread_avctx)) {
            goto fail;
        }
    }

    avctx->active_thread_type = FF_THREAD_FRAME;

    return 0;
fail:
    avctx->thread_count = i;
    av_log(avctx, AV_LOG_ERROR, "ff_frame_thread_encoder_init failed\n");
    ff_frame_thread_encoder_free(avctx);
    return -1;
}

void ff_frame_thread_encoder_free(AVCodecContext *avctx){
    int i;
    ThreadContext *c= avctx->internal->frame_thread_encoder;

    pthread_mutex_lock(&c->task_fifo_mutex);
    atomic_store(&c->exit, 1);
    pthread_cond_broadcast(&c->task_fifo_cond);
    pthread_mutex_unlock(&c->task_fifo_mutex);

    for (i=0; i<avctx->thread_count; i++) {
         pthread_join(c->worker[i], NULL);
    }

    for (unsigned i = 0; i < c->max_tasks; i++) {
        av_frame_free(&c->tasks[i].indata);
        av_packet_free(&c->tasks[i].outdata);
    }

    pthread_mutex_destroy(&c->task_fifo_mutex);
    pthread_mutex_destroy(&c->finished_task_mutex);
    pthread_mutex_destroy(&c->buffer_mutex);
    pthread_cond_destroy(&c->task_fifo_cond);
    pthread_cond_destroy(&c->finished_task_cond);
    av_freep(&avctx->internal->frame_thread_encoder);
}

int ff_thread_video_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                                 AVFrame *frame, int *got_packet_ptr)
{
    ThreadContext *c = avctx->internal->frame_thread_encoder;
    Task *outtask;

    av_assert1(!*got_packet_ptr);

    if(frame){
        av_frame_move_ref(c->tasks[c->task_index].indata, frame);

        pthread_mutex_lock(&c->task_fifo_mutex);
        c->task_index = (c->task_index + 1) % c->max_tasks;
        pthread_cond_signal(&c->task_fifo_cond);
        pthread_mutex_unlock(&c->task_fifo_mutex);
    }

    outtask = &c->tasks[c->finished_task_index];
    pthread_mutex_lock(&c->finished_task_mutex);
    /* The access to task_index in the following code is ok,
     * because it is only ever changed by the main thread. */
    if (c->task_index == c->finished_task_index ||
        (frame && !outtask->finished &&
         (c->task_index - c->finished_task_index + c->max_tasks) % c->max_tasks <= avctx->thread_count)) {
            pthread_mutex_unlock(&c->finished_task_mutex);
            return 0;
        }
    while (!outtask->finished) {
        pthread_cond_wait(&c->finished_task_cond, &c->finished_task_mutex);
    }
    /* We now own outtask completely: No worker thread touches it any more,
     * because there is no outstanding task with this index. */
    outtask->finished = 0;
    av_packet_move_ref(pkt, outtask->outdata);
    if(pkt->data)
        *got_packet_ptr = 1;
    c->finished_task_index = (c->finished_task_index + 1) % c->max_tasks;
    pthread_mutex_unlock(&c->finished_task_mutex);

    return outtask->return_code;
}
