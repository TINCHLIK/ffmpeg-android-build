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

/**
 * @file
 * DNN common functions different backends.
 */

#ifndef AVFILTER_DNN_DNN_BACKEND_COMMON_H
#define AVFILTER_DNN_DNN_BACKEND_COMMON_H

#include "../dnn_interface.h"

// one task for one function call from dnn interface
typedef struct TaskItem {
    void *model; // model for the backend
    AVFrame *in_frame;
    AVFrame *out_frame;
    const char *input_name;
    const char **output_names;
    uint8_t async;
    uint8_t do_ioproc;
    uint32_t nb_output;
    uint32_t inference_todo;
    uint32_t inference_done;
} TaskItem;

// one task might have multiple inferences
typedef struct InferenceItem {
    TaskItem *task;
    uint32_t bbox_index;
} InferenceItem;

int ff_check_exec_params(void *ctx, DNNBackendType backend, DNNFunctionType func_type, DNNExecBaseParams *exec_params);

#endif
