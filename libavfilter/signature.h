/*
 * Copyright (c) 2017 Gerion Entrup
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with FFmpeg; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file
 * MPEG-7 video signature calculation and lookup filter
 */

#ifndef AVFILTER_SIGNATURE_H
#define AVFILTER_SIGNATURE_H

#include <float.h>
#include "libavutil/common.h"
#include "libavutil/opt.h"
#include "libavutil/timestamp.h"
#include "avfilter.h"
#include "internal.h"

#define ELEMENT_COUNT 10
#define SIGELEM_SIZE 380
#define DIFFELEM_SIZE 348 /* SIGELEM_SIZE - elem_a1 - elem_a2 */
#define COARSE_SIZE 90

enum lookup_mode {
    MODE_OFF,
    MODE_FULL,
    MODE_FAST,
    NB_LOOKUP_MODE
};

enum formats {
    FORMAT_BINARY,
    FORMAT_XML,
    NB_FORMATS
};

typedef struct {
    uint8_t x;
    uint8_t y;
} Point;

typedef struct {
    Point up;
    Point to;
} Block;

typedef struct {
    int av_elem; /* average element category */
    short left_count; /* count of blocks that will be added together */
    short block_count; /* count of blocks per element */
    short elem_count;
    const Block* blocks;
} ElemCat;

typedef struct FineSignature {
    struct FineSignature* next;
    struct FineSignature* prev;
    uint64_t pts;
    uint32_t index; /* needed for xmlexport */
    uint8_t confidence;
    uint8_t words[5];
    uint8_t framesig[SIGELEM_SIZE/5];
} FineSignature;

typedef struct CoarseSignature {
    uint8_t data[5][31]; /* 5 words with min. 243 bit */
    struct FineSignature* first; /* associated Finesignatures */
    struct FineSignature* last;
    struct CoarseSignature* next;
} CoarseSignature;

/* lookup types */
typedef struct MatchingInfo {
    double meandist;
    double framerateratio; /* second/first */
    int score;
    int offset;
    int matchframes; /* number of matching frames */
    int whole;
    struct FineSignature* first;
    struct FineSignature* second;
    struct MatchingInfo* next;
} MatchingInfo;

typedef struct {
    AVRational time_base;
    /* needed for xml_export */
    int w; /* height */
    int h; /* width */

    /* overflow protection */
    int divide;

    FineSignature* finesiglist;
    FineSignature* curfinesig;

    CoarseSignature* coarsesiglist;
    CoarseSignature* coarseend; /* needed for xml export */
    /* helpers to store the alternating signatures */
    CoarseSignature* curcoarsesig1;
    CoarseSignature* curcoarsesig2;

    int coarsecount; /* counter from 0 to 89 */
    int midcoarse;   /* whether it is a coarsesignature beginning from 45 + i * 90 */
    uint32_t lastindex; /* helper to store amount of frames */

    int exported; /* boolean whether stream already exported */
} StreamContext;

typedef struct {
    const AVClass *class;
    /* input parameters */
    int mode;
    int nb_inputs;
    char *filename;
    int format;
    int thworddist;
    int thcomposdist;
    int thl1;
    int thdi;
    int thit;
    /* end input parameters */

    uint8_t l1distlut[243*242/2]; /* 243 + 242 + 241 ... */
    StreamContext* streamcontexts;
} SignatureContext;


static const Block elem_a1_data[] = {
    {{ 0, 0},{ 7, 7}},
    {{ 8, 0},{15, 7}},
    {{ 0, 8},{ 7,15}},
    {{ 8, 8},{15,15}},
    {{16, 0},{23, 7}},
    {{24, 0},{31, 7}},
    {{16, 8},{23,15}},
    {{24, 8},{31,15}},
    {{ 0,16},{ 7,23}},
    {{ 8,16},{15,23}},
    {{ 0,24},{ 7,31}},
    {{ 8,24},{15,31}},
    {{16,16},{23,23}},
    {{24,16},{31,23}},
    {{16,24},{23,31}},
    {{24,24},{31,31}},
    {{ 0, 0},{15,15}},
    {{16, 0},{31,15}},
    {{ 0,16},{15,31}},
    {{16,16},{31,31}}
};
static const ElemCat elem_a1 = { 1, 1, 1, 20, elem_a1_data };

static const Block elem_a2_data[] = {
    {{ 2, 2},{ 9, 9}},
    {{12, 2},{19, 9}},
    {{22, 2},{29, 9}},
    {{ 2,12},{ 9,19}},
    {{12,12},{19,19}},
    {{22,12},{29,19}},
    {{ 2,22},{ 9,29}},
    {{12,22},{19,29}},
    {{22,22},{29,29}},
    {{ 9, 9},{22,22}},
    {{ 6, 6},{25,25}},
    {{ 3, 3},{28,28}}
};
static const ElemCat elem_a2 = { 1, 1, 1, 12, elem_a2_data };

static const Block elem_d1_data[] = {
    {{ 0, 0},{ 1, 3}},{{ 2, 0},{ 3, 3}},
    {{ 4, 0},{ 7, 1}},{{ 4, 2},{ 7, 3}},
    {{ 0, 6},{ 3, 7}},{{ 0, 4},{ 3, 5}},
    {{ 6, 4},{ 7, 7}},{{ 4, 4},{ 5, 7}},
    {{ 8, 0},{ 9, 3}},{{10, 0},{11, 3}},
    {{12, 0},{15, 1}},{{12, 2},{15, 3}},
    {{ 8, 6},{11, 7}},{{ 8, 4},{11, 5}},
    {{14, 4},{15, 7}},{{12, 4},{13, 7}},
    {{ 0, 8},{ 1,11}},{{ 2, 8},{ 3,11}},
    {{ 4, 8},{ 7, 9}},{{ 4,10},{ 7,11}},
    {{ 0,14},{ 3,15}},{{ 0,12},{ 3,13}},
    {{ 6,12},{ 7,15}},{{ 4,12},{ 5,15}},
    {{ 8, 8},{ 9,11}},{{10, 8},{11,11}},
    {{12, 8},{15, 9}},{{12,10},{15,11}},
    {{ 8,14},{11,15}},{{ 8,12},{11,13}},
    {{14,12},{15,15}},{{12,12},{13,15}},
    {{16, 0},{19, 1}},{{16, 2},{19, 3}},
    {{22, 0},{23, 3}},{{20, 0},{21, 3}},
    {{16, 4},{17, 7}},{{18, 4},{19, 7}},
    {{20, 6},{23, 7}},{{20, 4},{23, 5}},
    {{24, 0},{27, 1}},{{24, 2},{27, 3}},
    {{30, 0},{31, 3}},{{28, 0},{29, 3}},
    {{24, 4},{25, 7}},{{26, 4},{27, 7}},
    {{28, 6},{31, 7}},{{28, 4},{31, 5}},
    {{16, 8},{19, 9}},{{16,10},{19,11}},
    {{22, 8},{23,11}},{{20, 8},{21,11}},
    {{16,12},{17,15}},{{18,12},{19,15}},
    {{20,14},{23,15}},{{20,12},{23,13}},
    {{24, 8},{27, 9}},{{24,10},{27,11}},
    {{30, 8},{31,11}},{{28, 8},{29,11}},
    {{24,12},{25,15}},{{26,12},{27,15}},
    {{28,14},{31,15}},{{28,12},{31,13}},
    {{ 0,16},{ 3,17}},{{ 0,18},{ 3,19}},
    {{ 6,16},{ 7,19}},{{ 4,16},{ 5,19}},
    {{ 0,20},{ 1,23}},{{ 2,20},{ 3,23}},
    {{ 4,22},{ 7,23}},{{ 4,20},{ 7,21}},
    {{ 8,16},{11,17}},{{ 8,18},{11,19}},
    {{14,16},{15,19}},{{12,16},{13,19}},
    {{ 8,20},{ 9,23}},{{10,20},{11,23}},
    {{12,22},{15,23}},{{12,20},{15,21}},
    {{ 0,24},{ 3,25}},{{ 0,26},{ 3,27}},
    {{ 6,24},{ 7,27}},{{ 4,24},{ 5,27}},
    {{ 0,28},{ 1,31}},{{ 2,28},{ 3,31}},
    {{ 4,30},{ 7,31}},{{ 4,28},{ 7,29}},
    {{ 8,24},{11,25}},{{ 8,26},{11,27}},
    {{14,24},{15,27}},{{12,24},{13,27}},
    {{ 8,28},{ 9,31}},{{10,28},{11,31}},
    {{12,30},{15,31}},{{12,28},{15,29}},
    {{16,16},{17,19}},{{18,16},{19,19}},
    {{20,16},{23,17}},{{20,18},{23,19}},
    {{16,22},{19,23}},{{16,20},{19,21}},
    {{22,20},{23,23}},{{20,20},{21,23}},
    {{24,16},{25,19}},{{26,16},{27,19}},
    {{28,16},{31,17}},{{28,18},{31,19}},
    {{24,22},{27,23}},{{24,20},{27,21}},
    {{30,20},{31,23}},{{28,20},{29,23}},
    {{16,24},{17,27}},{{18,24},{19,27}},
    {{20,24},{23,25}},{{20,26},{23,27}},
    {{16,30},{19,31}},{{16,28},{19,29}},
    {{22,28},{23,31}},{{20,28},{21,31}},
    {{24,24},{25,27}},{{26,24},{27,27}},
    {{28,24},{31,25}},{{28,26},{31,27}},
    {{24,30},{27,31}},{{24,28},{27,29}},
    {{30,28},{31,31}},{{28,28},{29,31}},
    {{ 2, 2},{ 3, 5}},{{ 4, 2},{ 5, 5}},
    {{ 6, 2},{ 9, 3}},{{ 6, 4},{ 9, 5}},
    {{ 2, 8},{ 5, 9}},{{ 2, 6},{ 5, 7}},
    {{ 8, 6},{ 9, 9}},{{ 6, 6},{ 7, 9}},
    {{12, 2},{13, 5}},{{14, 2},{15, 5}},
    {{16, 2},{19, 3}},{{16, 4},{19, 5}},
    {{12, 8},{15, 9}},{{12, 6},{15, 7}},
    {{18, 6},{19, 9}},{{16, 6},{17, 9}},
    {{22, 2},{23, 5}},{{24, 2},{25, 5}},
    {{26, 2},{29, 3}},{{26, 4},{29, 5}},
    {{22, 8},{25, 9}},{{22, 6},{25, 7}},
    {{28, 6},{29, 9}},{{26, 6},{27, 9}},
    {{ 2,12},{ 3,15}},{{ 4,12},{ 5,15}},
    {{ 6,12},{ 9,13}},{{ 6,14},{ 9,15}},
    {{ 2,18},{ 5,19}},{{ 2,16},{ 5,17}},
    {{ 8,16},{ 9,19}},{{ 6,16},{ 7,19}},
    {{12,12},{15,13}},{{12,14},{15,15}},
    {{16,12},{19,13}},{{16,14},{19,15}},
    {{12,18},{15,19}},{{12,16},{15,17}},
    {{16,18},{19,19}},{{16,16},{19,17}},
    {{22,12},{23,15}},{{24,12},{25,15}},
    {{26,12},{29,13}},{{26,14},{29,15}},
    {{22,18},{25,19}},{{22,16},{25,17}},
    {{28,16},{29,19}},{{26,16},{27,19}},
    {{ 2,22},{ 3,25}},{{ 4,22},{ 5,25}},
    {{ 6,22},{ 9,23}},{{ 6,24},{ 9,25}},
    {{ 2,28},{ 5,29}},{{ 2,26},{ 5,27}},
    {{ 8,26},{ 9,29}},{{ 6,26},{ 7,29}},
    {{12,22},{13,25}},{{14,22},{15,25}},
    {{16,22},{19,23}},{{16,24},{19,25}},
    {{12,28},{15,29}},{{12,26},{15,27}},
    {{18,26},{19,29}},{{16,26},{17,29}},
    {{22,22},{23,25}},{{24,22},{25,25}},
    {{26,22},{29,23}},{{26,24},{29,25}},
    {{22,28},{25,29}},{{22,26},{25,27}},
    {{28,26},{29,29}},{{26,26},{27,29}},
    {{ 7, 7},{10, 8}},{{ 7, 9},{10,10}},
    {{11, 7},{12,10}},{{13, 7},{14,10}},
    {{ 7,11},{ 8,14}},{{ 9,11},{10,14}},
    {{11,11},{14,12}},{{11,13},{14,14}},
    {{17, 7},{20, 8}},{{17, 9},{20,10}},
    {{21, 7},{22,10}},{{23, 7},{24,10}},
    {{17,11},{18,14}},{{19,11},{20,14}},
    {{21,11},{24,12}},{{21,13},{24,14}},
    {{ 7,17},{10,18}},{{ 7,19},{10,20}},
    {{11,17},{12,20}},{{13,17},{14,20}},
    {{ 7,21},{ 8,24}},{{ 9,21},{10,24}},
    {{11,21},{14,22}},{{11,23},{14,24}},
    {{17,17},{20,18}},{{17,19},{20,20}},
    {{21,17},{22,20}},{{23,17},{24,20}},
    {{17,21},{18,24}},{{19,21},{20,24}},
    {{21,21},{24,22}},{{21,23},{24,24}}
};
static const ElemCat elem_d1 = { 0, 1, 2, 116, elem_d1_data };

static const Block elem_d2_data[] = {
    {{ 0, 0},{ 3, 3}},{{ 4, 4},{ 7, 7}},{{ 4, 0},{ 7, 3}},{{ 0, 4},{ 3, 7}},
    {{ 8, 0},{11, 3}},{{12, 4},{15, 7}},{{12, 0},{15, 3}},{{ 8, 4},{11, 7}},
    {{16, 0},{19, 3}},{{20, 4},{23, 7}},{{20, 0},{23, 3}},{{16, 4},{19, 7}},
    {{24, 0},{27, 3}},{{28, 4},{31, 7}},{{28, 0},{31, 3}},{{24, 4},{27, 7}},
    {{ 0, 8},{ 3,11}},{{ 4,12},{ 7,15}},{{ 4, 8},{ 7,11}},{{ 0,12},{ 3,15}},
    {{ 8, 8},{11,11}},{{12,12},{15,15}},{{12, 8},{15,11}},{{ 8,12},{11,15}},
    {{16, 8},{19,11}},{{20,12},{23,15}},{{20, 8},{23,11}},{{16,12},{19,15}},
    {{24, 8},{27,11}},{{28,12},{31,15}},{{28, 8},{31,11}},{{24,12},{27,15}},
    {{ 0,16},{ 3,19}},{{ 4,20},{ 7,23}},{{ 4,16},{ 7,19}},{{ 0,20},{ 3,23}},
    {{ 8,16},{11,19}},{{12,20},{15,23}},{{12,16},{15,19}},{{ 8,20},{11,23}},
    {{16,16},{19,19}},{{20,20},{23,23}},{{20,16},{23,19}},{{16,20},{19,23}},
    {{24,16},{27,19}},{{28,20},{31,23}},{{28,16},{31,19}},{{24,20},{27,23}},
    {{ 0,24},{ 3,27}},{{ 4,28},{ 7,31}},{{ 4,24},{ 7,27}},{{ 0,28},{ 3,31}},
    {{ 8,24},{11,27}},{{12,28},{15,31}},{{12,24},{15,27}},{{ 8,28},{11,31}},
    {{16,24},{19,27}},{{20,28},{23,31}},{{20,24},{23,27}},{{16,28},{19,31}},
    {{24,24},{27,27}},{{28,28},{31,31}},{{28,24},{31,27}},{{24,28},{27,31}},
    {{ 4, 4},{ 7, 7}},{{ 8, 8},{11,11}},{{ 8, 4},{11, 7}},{{ 4, 8},{ 7,11}},
    {{12, 4},{15, 7}},{{16, 8},{19,11}},{{16, 4},{19, 7}},{{12, 8},{15,11}},
    {{20, 4},{23, 7}},{{24, 8},{27,11}},{{24, 4},{27, 7}},{{20, 8},{23,11}},
    {{ 4,12},{ 7,15}},{{ 8,16},{11,19}},{{ 8,12},{11,15}},{{ 4,16},{ 7,19}},
    {{12,12},{15,15}},{{16,16},{19,19}},{{16,12},{19,15}},{{12,16},{15,19}},
    {{20,12},{23,15}},{{24,16},{27,19}},{{24,12},{27,15}},{{20,16},{23,19}},
    {{ 4,20},{ 7,23}},{{ 8,24},{11,27}},{{ 8,20},{11,23}},{{ 4,24},{ 7,27}},
    {{12,20},{15,23}},{{16,24},{19,27}},{{16,20},{19,23}},{{12,24},{15,27}},
    {{20,20},{23,23}},{{24,24},{27,27}},{{24,20},{27,23}},{{20,24},{23,27}}
};
static const ElemCat elem_d2 = { 0, 2, 4, 25, elem_d2_data };

static const Block elem_d3_data[] = {
    {{ 1, 1},{10,10}},{{11, 1},{20,10}},
    {{ 1, 1},{10,10}},{{21, 1},{30,10}},
    {{ 1, 1},{10,10}},{{ 1,11},{10,20}},
    {{ 1, 1},{10,10}},{{11,11},{20,20}},
    {{ 1, 1},{10,10}},{{21,11},{30,20}},
    {{ 1, 1},{10,10}},{{ 1,21},{10,30}},
    {{ 1, 1},{10,10}},{{11,21},{20,30}},
    {{ 1, 1},{10,10}},{{21,21},{30,30}},
    {{11, 1},{20,10}},{{21, 1},{30,10}},
    {{11, 1},{20,10}},{{ 1,11},{10,20}},
    {{11, 1},{20,10}},{{11,11},{20,20}},
    {{11, 1},{20,10}},{{21,11},{30,20}},
    {{11, 1},{20,10}},{{ 1,21},{10,30}},
    {{11, 1},{20,10}},{{11,21},{20,30}},
    {{11, 1},{20,10}},{{21,21},{30,30}},
    {{21, 1},{30,10}},{{ 1,11},{10,20}},
    {{21, 1},{30,10}},{{11,11},{20,20}},
    {{21, 1},{30,10}},{{21,11},{30,20}},
    {{21, 1},{30,10}},{{ 1,21},{10,30}},
    {{21, 1},{30,10}},{{11,21},{20,30}},
    {{21, 1},{30,10}},{{21,21},{30,30}},
    {{ 1,11},{10,20}},{{11,11},{20,20}},
    {{ 1,11},{10,20}},{{21,11},{30,20}},
    {{ 1,11},{10,20}},{{ 1,21},{10,30}},
    {{ 1,11},{10,20}},{{11,21},{20,30}},
    {{ 1,11},{10,20}},{{21,21},{30,30}},
    {{11,11},{20,20}},{{21,11},{30,20}},
    {{11,11},{20,20}},{{ 1,21},{10,30}},
    {{11,11},{20,20}},{{11,21},{20,30}},
    {{11,11},{20,20}},{{21,21},{30,30}},
    {{21,11},{30,20}},{{ 1,21},{10,30}},
    {{21,11},{30,20}},{{11,21},{20,30}},
    {{21,11},{30,20}},{{21,21},{30,30}},
    {{ 1,21},{10,30}},{{11,21},{20,30}},
    {{ 1,21},{10,30}},{{21,21},{30,30}},
    {{11,21},{20,30}},{{21,21},{30,30}}
};
static const ElemCat elem_d3 = { 0, 1, 2, 36, elem_d3_data };

static const Block elem_d4_data[] = {
    {{ 7,13},{12,18}},{{19,13},{24,18}},
    {{13, 7},{18,12}},{{13,19},{18,24}},
    {{ 7, 7},{12,12}},{{19,19},{24,24}},
    {{19, 7},{24,12}},{{ 7,19},{12,24}},
    {{13, 7},{18,12}},{{19,13},{24,18}},
    {{19,13},{24,18}},{{13,19},{18,24}},
    {{13,19},{18,24}},{{ 7,13},{12,18}},
    {{ 7,13},{12,18}},{{13, 7},{18,12}},
    {{ 7, 7},{12,12}},{{19, 7},{24,12}},
    {{19, 7},{24,12}},{{19,19},{24,24}},
    {{19,19},{24,24}},{{ 7,19},{12,24}},
    {{ 7,19},{12,24}},{{ 7, 7},{12,12}},
    {{13,13},{18,18}},{{13, 1},{18, 6}},
    {{13,13},{18,18}},{{25,13},{30,18}},
    {{13,13},{18,18}},{{13,25},{18,30}},
    {{13,13},{18,18}},{{ 1,13},{ 6,18}},
    {{13, 1},{18, 6}},{{13,25},{18,30}},
    {{ 1,13},{ 6,18}},{{25,13},{30,18}},
    {{ 7, 1},{12, 6}},{{19, 1},{24, 6}},
    {{ 7,25},{12,30}},{{19,25},{24,30}},
    {{ 1, 7},{ 6,12}},{{ 1,19},{ 6,24}},
    {{25, 7},{30,12}},{{25,19},{30,24}},
    {{ 7, 1},{12, 6}},{{ 1, 7},{ 6,12}},
    {{19, 1},{24, 6}},{{25, 7},{30,12}},
    {{25,19},{30,24}},{{19,25},{24,30}},
    {{ 1,19},{ 6,24}},{{ 7,25},{12,30}},
    {{ 1, 1},{ 6, 6}},{{25, 1},{30, 6}},
    {{25, 1},{30, 6}},{{25,25},{30,30}},
    {{25,25},{30,30}},{{ 1,25},{ 6,30}},
    {{ 1,25},{ 6,30}},{{ 1, 1},{ 6, 6}}
};
static const ElemCat elem_d4 = { 0, 1, 2, 30, elem_d4_data };

static const Block elem_d5_data[] = {
    {{ 1, 1},{10, 3}},{{ 1, 4},{ 3, 7}},{{ 8, 4},{10, 7}},{{ 1, 8},{10,10}},{{ 4, 4},{ 7, 7}},
    {{11, 1},{20, 3}},{{11, 4},{13, 7}},{{18, 4},{20, 7}},{{11, 8},{20,10}},{{14, 4},{17, 7}},
    {{21, 1},{30, 3}},{{21, 4},{23, 7}},{{28, 4},{30, 7}},{{21, 8},{30,10}},{{24, 4},{27, 7}},
    {{ 1,11},{10,13}},{{ 1,14},{ 3,17}},{{ 8,14},{10,17}},{{ 1,18},{10,20}},{{ 4,14},{ 7,17}},
    {{11,11},{20,13}},{{11,14},{13,17}},{{18,14},{20,17}},{{11,18},{20,20}},{{14,14},{17,17}},
    {{21,11},{30,13}},{{21,14},{23,17}},{{28,14},{30,17}},{{21,18},{30,20}},{{24,14},{27,17}},
    {{ 1,21},{10,23}},{{ 1,24},{ 3,27}},{{ 8,24},{10,27}},{{ 1,28},{10,30}},{{ 4,24},{ 7,27}},
    {{11,21},{20,23}},{{11,24},{13,27}},{{18,24},{20,27}},{{11,28},{20,30}},{{14,24},{17,27}},
    {{21,21},{30,23}},{{21,24},{23,27}},{{28,24},{30,27}},{{21,28},{30,30}},{{24,24},{27,27}},
    {{ 6, 6},{15, 8}},{{ 6, 9},{ 8,12}},{{13, 9},{15,12}},{{ 6,13},{15,15}},{{ 9, 9},{12,12}},
    {{16, 6},{25, 8}},{{16, 9},{18,12}},{{23, 9},{25,12}},{{16,13},{25,15}},{{19, 9},{22,12}},
    {{ 6,16},{15,18}},{{ 6,19},{ 8,22}},{{13,19},{15,22}},{{ 6,23},{15,25}},{{ 9,19},{12,22}},
    {{16,16},{25,18}},{{16,19},{18,22}},{{23,19},{25,22}},{{16,23},{25,25}},{{19,19},{22,22}},
    {{ 6, 1},{15, 3}},{{ 6, 4},{ 8, 7}},{{13, 4},{15, 7}},{{ 6, 8},{15,10}},{{ 9, 4},{12, 7}},
    {{16, 1},{25, 3}},{{16, 4},{18, 7}},{{23, 4},{25, 7}},{{16, 8},{25,10}},{{19, 4},{22, 7}},
    {{ 1, 6},{10, 8}},{{ 1, 9},{ 3,12}},{{ 8, 9},{10,12}},{{ 1,13},{10,15}},{{ 4, 9},{ 7,12}},
    {{11, 6},{20, 8}},{{11, 9},{13,12}},{{18, 9},{20,12}},{{11,13},{20,15}},{{14, 9},{17,12}},
    {{21, 6},{30, 8}},{{21, 9},{23,12}},{{28, 9},{30,12}},{{21,13},{30,15}},{{24, 9},{27,12}},
    {{ 6,11},{15,13}},{{ 6,14},{ 8,17}},{{13,14},{15,17}},{{ 6,18},{15,20}},{{ 9,14},{12,17}},
    {{16,11},{25,13}},{{16,14},{18,17}},{{23,14},{25,17}},{{16,18},{25,20}},{{19,14},{22,17}},
    {{ 1,16},{10,18}},{{ 1,19},{ 3,22}},{{ 8,19},{10,22}},{{ 1,23},{10,25}},{{ 4,19},{ 7,22}},
    {{11,16},{20,18}},{{11,19},{13,22}},{{18,19},{20,22}},{{11,23},{20,25}},{{14,19},{17,22}},
    {{21,16},{30,18}},{{21,19},{23,22}},{{28,19},{30,22}},{{21,23},{30,25}},{{24,19},{27,22}},
    {{ 6,21},{15,23}},{{ 6,24},{ 8,27}},{{13,24},{15,27}},{{ 6,28},{15,30}},{{ 9,24},{12,27}},
    {{16,21},{25,23}},{{16,24},{18,27}},{{23,24},{25,27}},{{16,28},{25,30}},{{19,24},{22,27}},
    {{ 2, 2},{14, 6}},{{ 2, 7},{ 6, 9}},{{10, 7},{14, 9}},{{ 2,10},{14,14}},{{ 7, 7},{ 9, 9}},
    {{ 7, 2},{19, 6}},{{ 7, 7},{11, 9}},{{15, 7},{19, 9}},{{ 7,10},{19,14}},{{12, 7},{14, 9}},
    {{12, 2},{24, 6}},{{12, 7},{16, 9}},{{20, 7},{24, 9}},{{12,10},{24,14}},{{17, 7},{19, 9}},
    {{17, 2},{29, 6}},{{17, 7},{21, 9}},{{25, 7},{29, 9}},{{17,10},{29,14}},{{22, 7},{24, 9}},
    {{ 2, 7},{14,11}},{{ 2,12},{ 6,14}},{{10,12},{14,14}},{{ 2,15},{14,19}},{{ 7,12},{ 9,14}},
    {{ 7, 7},{19,11}},{{ 7,12},{11,14}},{{15,12},{19,14}},{{ 7,15},{19,19}},{{12,12},{14,14}},
    {{12, 7},{24,11}},{{12,12},{16,14}},{{20,12},{24,14}},{{12,15},{24,19}},{{17,12},{19,14}},
    {{17, 7},{29,11}},{{17,12},{21,14}},{{25,12},{29,14}},{{17,15},{29,19}},{{22,12},{24,14}},
    {{ 2,12},{14,16}},{{ 2,17},{ 6,19}},{{10,17},{14,19}},{{ 2,20},{14,24}},{{ 7,17},{ 9,19}},
    {{ 7,12},{19,16}},{{ 7,17},{11,19}},{{15,17},{19,19}},{{ 7,20},{19,24}},{{12,17},{14,19}},
    {{12,12},{24,16}},{{12,17},{16,19}},{{20,17},{24,19}},{{12,20},{24,24}},{{17,17},{19,19}},
    {{17,12},{29,16}},{{17,17},{21,19}},{{25,17},{29,19}},{{17,20},{29,24}},{{22,17},{24,19}},
    {{ 2,17},{14,21}},{{ 2,22},{ 6,24}},{{10,22},{14,24}},{{ 2,25},{14,29}},{{ 7,22},{ 9,24}},
    {{ 7,17},{19,21}},{{ 7,22},{11,24}},{{15,22},{19,24}},{{ 7,25},{19,29}},{{12,22},{14,24}},
    {{12,17},{24,21}},{{12,22},{16,24}},{{20,22},{24,24}},{{12,25},{24,29}},{{17,22},{19,24}},
    {{17,17},{29,21}},{{17,22},{21,24}},{{25,22},{29,24}},{{17,25},{29,29}},{{22,22},{24,24}},
    {{ 8, 3},{13, 4}},{{ 8, 5},{ 9, 6}},{{12, 5},{13, 6}},{{ 8, 7},{13, 8}},{{10, 5},{11, 6}},
    {{13, 3},{18, 4}},{{13, 5},{14, 6}},{{17, 5},{18, 6}},{{13, 7},{18, 8}},{{15, 5},{16, 6}},
    {{18, 3},{23, 4}},{{18, 5},{19, 6}},{{22, 5},{23, 6}},{{18, 7},{23, 8}},{{20, 5},{21, 6}},
    {{ 3, 8},{ 8, 9}},{{ 3,10},{ 4,11}},{{ 7,10},{ 8,11}},{{ 3,12},{ 8,13}},{{ 5,10},{ 6,11}},
    {{ 8, 8},{13, 9}},{{ 8,10},{ 9,11}},{{12,10},{13,11}},{{ 8,12},{13,13}},{{10,10},{11,11}},
    {{13, 8},{18, 9}},{{13,10},{14,11}},{{17,10},{18,11}},{{13,12},{18,13}},{{15,10},{16,11}},
    {{18, 8},{23, 9}},{{18,10},{19,11}},{{22,10},{23,11}},{{18,12},{23,13}},{{20,10},{21,11}},
    {{23, 8},{28, 9}},{{23,10},{24,11}},{{27,10},{28,11}},{{23,12},{28,13}},{{25,10},{26,11}},
    {{ 3,13},{ 8,14}},{{ 3,15},{ 4,16}},{{ 7,15},{ 8,16}},{{ 3,17},{ 8,18}},{{ 5,15},{ 6,16}},
    {{ 8,13},{13,14}},{{ 8,15},{ 9,16}},{{12,15},{13,16}},{{ 8,17},{13,18}},{{10,15},{11,16}},
    {{13,13},{18,14}},{{13,15},{14,16}},{{17,15},{18,16}},{{13,17},{18,18}},{{15,15},{16,16}},
    {{18,13},{23,14}},{{18,15},{19,16}},{{22,15},{23,16}},{{18,17},{23,18}},{{20,15},{21,16}},
    {{23,13},{28,14}},{{23,15},{24,16}},{{27,15},{28,16}},{{23,17},{28,18}},{{25,15},{26,16}},
    {{ 3,18},{ 8,19}},{{ 3,20},{ 4,21}},{{ 7,20},{ 8,21}},{{ 3,22},{ 8,23}},{{ 5,20},{ 6,21}},
    {{ 8,18},{13,19}},{{ 8,20},{ 9,21}},{{12,20},{13,21}},{{ 8,22},{13,23}},{{10,20},{11,21}},
    {{13,18},{18,19}},{{13,20},{14,21}},{{17,20},{18,21}},{{13,22},{18,23}},{{15,20},{16,21}},
    {{18,18},{23,19}},{{18,20},{19,21}},{{22,20},{23,21}},{{18,22},{23,23}},{{20,20},{21,21}},
    {{23,18},{28,19}},{{23,20},{24,21}},{{27,20},{28,21}},{{23,22},{28,23}},{{25,20},{26,21}},
    {{ 8,23},{13,24}},{{ 8,25},{ 9,26}},{{12,25},{13,26}},{{ 8,27},{13,28}},{{10,25},{11,26}},
    {{13,23},{18,24}},{{13,25},{14,26}},{{17,25},{18,26}},{{13,27},{18,28}},{{15,25},{16,26}},
    {{18,23},{23,24}},{{18,25},{19,26}},{{22,25},{23,26}},{{18,27},{23,28}},{{20,25},{21,26}}
};
static const ElemCat elem_d5 = { 0, 4, 5, 62, elem_d5_data };

static const Block elem_d6_data[] = {
    {{ 3, 5},{12,10}},{{ 5, 3},{10,12}},
    {{11, 5},{20,10}},{{13, 3},{18,12}},
    {{19, 5},{28,10}},{{21, 3},{26,12}},
    {{ 3,13},{12,18}},{{ 5,11},{10,20}},
    {{11,13},{20,18}},{{13,11},{18,20}},
    {{19,13},{28,18}},{{21,11},{26,20}},
    {{ 3,21},{12,26}},{{ 5,19},{10,28}},
    {{11,21},{20,26}},{{13,19},{18,28}},
    {{19,21},{28,26}},{{21,19},{26,28}}
};
static const ElemCat elem_d6 = { 0, 1, 2, 9, elem_d6_data };

static const Block elem_d7_data[] = {
    {{ 0, 4},{ 3, 7}},{{ 8, 4},{11, 7}},{{ 4, 4},{ 7, 7}},
    {{ 4, 0},{ 7, 3}},{{ 4, 8},{ 7,11}},{{ 4, 4},{ 7, 7}},
    {{ 5, 4},{ 8, 7}},{{13, 4},{16, 7}},{{ 9, 4},{12, 7}},
    {{ 9, 0},{12, 3}},{{ 9, 8},{12,11}},{{ 9, 4},{12, 7}},
    {{10, 4},{13, 7}},{{18, 4},{21, 7}},{{14, 4},{17, 7}},
    {{14, 0},{17, 3}},{{14, 8},{17,11}},{{14, 4},{17, 7}},
    {{15, 4},{18, 7}},{{23, 4},{26, 7}},{{19, 4},{22, 7}},
    {{19, 0},{22, 3}},{{19, 8},{22,11}},{{19, 4},{22, 7}},
    {{20, 4},{23, 7}},{{28, 4},{31, 7}},{{24, 4},{27, 7}},
    {{24, 0},{27, 3}},{{24, 8},{27,11}},{{24, 4},{27, 7}},
    {{ 0, 9},{ 3,12}},{{ 8, 9},{11,12}},{{ 4, 9},{ 7,12}},
    {{ 4, 5},{ 7, 8}},{{ 4,13},{ 7,16}},{{ 4, 9},{ 7,12}},
    {{ 5, 9},{ 8,12}},{{13, 9},{16,12}},{{ 9, 9},{12,12}},
    {{ 9, 5},{12, 8}},{{ 9,13},{12,16}},{{ 9, 9},{12,12}},
    {{10, 9},{13,12}},{{18, 9},{21,12}},{{14, 9},{17,12}},
    {{14, 5},{17, 8}},{{14,13},{17,16}},{{14, 9},{17,12}},
    {{15, 9},{18,12}},{{23, 9},{26,12}},{{19, 9},{22,12}},
    {{19, 5},{22, 8}},{{19,13},{22,16}},{{19, 9},{22,12}},
    {{20, 9},{23,12}},{{28, 9},{31,12}},{{24, 9},{27,12}},
    {{24, 5},{27, 8}},{{24,13},{27,16}},{{24, 9},{27,12}},
    {{ 0,14},{ 3,17}},{{ 8,14},{11,17}},{{ 4,14},{ 7,17}},
    {{ 4,10},{ 7,13}},{{ 4,18},{ 7,21}},{{ 4,14},{ 7,17}},
    {{ 5,14},{ 8,17}},{{13,14},{16,17}},{{ 9,14},{12,17}},
    {{ 9,10},{12,13}},{{ 9,18},{12,21}},{{ 9,14},{12,17}},
    {{10,14},{13,17}},{{18,14},{21,17}},{{14,14},{17,17}},
    {{14,10},{17,13}},{{14,18},{17,21}},{{14,14},{17,17}},
    {{15,14},{18,17}},{{23,14},{26,17}},{{19,14},{22,17}},
    {{19,10},{22,13}},{{19,18},{22,21}},{{19,14},{22,17}},
    {{20,14},{23,17}},{{28,14},{31,17}},{{24,14},{27,17}},
    {{24,10},{27,13}},{{24,18},{27,21}},{{24,14},{27,17}},
    {{ 0,19},{ 3,22}},{{ 8,19},{11,22}},{{ 4,19},{ 7,22}},
    {{ 4,15},{ 7,18}},{{ 4,23},{ 7,26}},{{ 4,19},{ 7,22}},
    {{ 5,19},{ 8,22}},{{13,19},{16,22}},{{ 9,19},{12,22}},
    {{ 9,15},{12,18}},{{ 9,23},{12,26}},{{ 9,19},{12,22}},
    {{10,19},{13,22}},{{18,19},{21,22}},{{14,19},{17,22}},
    {{14,15},{17,18}},{{14,23},{17,26}},{{14,19},{17,22}},
    {{15,19},{18,22}},{{23,19},{26,22}},{{19,19},{22,22}},
    {{19,15},{22,18}},{{19,23},{22,26}},{{19,19},{22,22}},
    {{20,19},{23,22}},{{28,19},{31,22}},{{24,19},{27,22}},
    {{24,15},{27,18}},{{24,23},{27,26}},{{24,19},{27,22}},
    {{ 0,24},{ 3,27}},{{ 8,24},{11,27}},{{ 4,24},{ 7,27}},
    {{ 4,20},{ 7,23}},{{ 4,28},{ 7,31}},{{ 4,24},{ 7,27}},
    {{ 5,24},{ 8,27}},{{13,24},{16,27}},{{ 9,24},{12,27}},
    {{ 9,20},{12,23}},{{ 9,28},{12,31}},{{ 9,24},{12,27}},
    {{10,24},{13,27}},{{18,24},{21,27}},{{14,24},{17,27}},
    {{14,20},{17,23}},{{14,28},{17,31}},{{14,24},{17,27}},
    {{15,24},{18,27}},{{23,24},{26,27}},{{19,24},{22,27}},
    {{19,20},{22,23}},{{19,28},{22,31}},{{19,24},{22,27}},
    {{20,24},{23,27}},{{28,24},{31,27}},{{24,24},{27,27}},
    {{24,20},{27,23}},{{24,28},{27,31}},{{24,24},{27,27}}
};
static const ElemCat elem_d7 = { 0, 2, 3, 50, elem_d7_data };

static const Block elem_d8_data[] = {
    {{ 0, 0},{ 7, 3}},{{ 0, 4},{ 7, 7}},
    {{ 8, 0},{11, 7}},{{12, 0},{15, 7}},
    {{ 0, 8},{ 3,15}},{{ 4, 8},{ 7,15}},
    {{ 8, 8},{15,11}},{{ 8,12},{15,15}},
    {{16, 0},{19, 7}},{{20, 0},{23, 7}},
    {{24, 0},{31, 3}},{{24, 4},{31, 7}},
    {{16, 8},{23,11}},{{16,12},{23,15}},
    {{24, 8},{27,15}},{{28, 8},{31,15}},
    {{ 0,16},{ 3,23}},{{ 4,16},{ 7,23}},
    {{ 8,16},{15,19}},{{ 8,20},{15,23}},
    {{ 0,24},{ 7,27}},{{ 0,28},{ 7,31}},
    {{ 8,24},{11,31}},{{12,24},{15,31}},
    {{16,16},{23,19}},{{16,20},{23,23}},
    {{24,16},{27,23}},{{28,16},{31,23}},
    {{16,24},{19,31}},{{20,24},{23,31}},
    {{24,24},{31,27}},{{24,28},{31,31}},
    {{ 0, 0},{ 7,15}},{{ 8, 0},{15,15}},
    {{16, 0},{31, 7}},{{16, 8},{31,15}},
    {{ 0,16},{15,23}},{{ 0,24},{15,31}},
    {{16,16},{23,31}},{{24,16},{31,31}}
};
static const ElemCat elem_d8 = { 0, 1, 2, 20, elem_d8_data };

static const ElemCat* elements[ELEMENT_COUNT] = { &elem_a1, &elem_a2,
                                                  &elem_d1, &elem_d2, &elem_d3, &elem_d4,
                                                  &elem_d5, &elem_d6, &elem_d7, &elem_d8 };
#endif /* AVFILTER_SIGNATURE_H */
