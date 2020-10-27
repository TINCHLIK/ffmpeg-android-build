/*
 * COOK compatible decoder data
 * Copyright (c) 2003 Sascha Sommer
 * Copyright (c) 2005 Benjamin Larsson
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
 * Cook AKA RealAudio G2 compatible decoder data
 */

#ifndef AVCODEC_COOKDATA_H
#define AVCODEC_COOKDATA_H

#include <stdint.h>

/* various data tables */

static const int expbits_tab[8] = {
    52,47,43,37,29,22,16,0,
};

static const float dither_tab[9] = {
  0.0, 0.0, 0.0, 0.0, 0.0, 0.176777, 0.25, 0.707107, 1.0
};

static const float quant_centroid_tab[7][14] = {
  { 0.000, 0.392, 0.761, 1.120, 1.477, 1.832, 2.183, 2.541, 2.893, 3.245, 3.598, 3.942, 4.288, 4.724 },
  { 0.000, 0.544, 1.060, 1.563, 2.068, 2.571, 3.072, 3.562, 4.070, 4.620, 0.000, 0.000, 0.000, 0.000 },
  { 0.000, 0.746, 1.464, 2.180, 2.882, 3.584, 4.316, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000 },
  { 0.000, 1.006, 2.000, 2.993, 3.985, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000 },
  { 0.000, 1.321, 2.703, 3.983, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000 },
  { 0.000, 1.657, 3.491, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000 },
  { 0.000, 1.964, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000 }
};

static const int invradix_tab[7] = {
    74899, 104858, 149797, 209716, 262144, 349526, 524288,
};

static const int kmax_tab[7] = {
    13, 9, 6, 4, 3, 2, 1,
};

static const int vd_tab[7] = {
    2, 2, 2, 4, 4, 5, 5,
};

static const int vpr_tab[7] = {
    10, 10, 10, 5, 5, 4, 4,
};



/* VLC data */

static const int vhsize_tab[7] = {
    181, 94, 48, 520, 209, 192, 32,
};

static const int vhvlcsize_tab[7] = {
    8, 7, 7, 10, 9, 9, 6,
};

static const uint8_t envelope_quant_index_huffbits[13][24] = {
    { 3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  5,
      5,  5,  6,  7,  8,  9, 11, 11, 12, 12, 12, 12 },
    { 3,  3,  3,  3,  3,  3,  4,  4,  5,  5,  5,  6,
      7,  8,  9, 10, 11, 12, 13, 15, 15, 15, 16, 16 },
    { 3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  5,
      5,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 14 },
    { 3,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,
      7,  7,  7,  9,  9,  9, 10, 11, 13, 13, 13, 13 },
    { 3,  3,  3,  3,  3,  4,  4,  4,  5,  5,  5,  5,
      6,  6,  6,  7,  8,  9, 10, 11, 12, 13, 14, 14 },
    { 3,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,
      7,  7,  8,  8,  8,  9, 10, 11, 12, 13, 14, 14 },
    { 2,  3,  3,  3,  3,  4,  4,  5,  5,  5,  6,  7,
      8,  9, 10, 11, 12, 13, 15, 15, 16, 16, 16, 16 },
    { 2,  3,  3,  3,  3,  4,  4,  5,  5,  5,  7,  7,
      7,  9,  9,  9, 10, 11, 12, 14, 14, 14, 15, 15 },
    { 3,  3,  3,  3,  3,  3,  4,  4,  5,  5,  6,  6,
      7,  7,  8,  8,  9,  9,  9, 10, 11, 12, 13, 13 },
    { 3,  3,  3,  3,  3,  3,  4,  4,  5,  5,  6,  6,
      6,  8,  8,  8,  9, 10, 11, 12, 14, 14, 14, 14 },
    { 2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  6,  6,
      6,  8,  8,  9,  9,  9, 10, 11, 12, 13, 14, 14 },
    { 2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,
      7,  8,  9, 10, 11, 12, 13, 14, 16, 16, 16, 16 },
    { 2,  3,  3,  3,  3,  4,  4,  5,  5,  5,  7,  7,
      7,  8,  9, 10, 11, 13, 14, 14, 14, 14, 14, 14 },
};

static const uint8_t envelope_quant_index_huffsyms[13][24] = {
    { 10, 11, 12,  0,  4,  5,  6,  7,  8,  9, 13,  2,
       3, 14,  1, 15, 16, 17, 18, 19, 20, 21, 22, 23 },
    {  6,  7,  8,  9, 10, 11,  5, 12,  3,  4, 13,  2,
      14,  1, 15,  0, 16, 17, 18, 19, 20, 21, 22, 23 },
    { 11, 12, 13,  5,  6,  7,  8,  9, 10, 14, 15,  4,
      16, 17,  3, 18,  2, 19,  1, 20,  0, 21, 22, 23 },
    {  9, 10, 11, 12, 13,  8, 14, 15, 16,  6,  7, 17,
       4,  5, 18,  2,  3, 19,  1, 20,  0, 21, 22, 23 },
    { 10, 11, 12, 13, 14,  8,  9, 15,  6,  7, 16, 17,
       4,  5, 18, 19,  3, 20,  2, 21,  0,  1, 22, 23 },
    {  9, 10, 11, 12, 13,  7,  8, 14, 15,  6, 16, 17,
       5, 18,  3,  4, 19,  2, 20,  1,  0, 21, 22, 23 },
    { 12,  9, 10, 11, 13,  8, 14,  7, 15, 16,  6, 17,
       5, 18,  4, 19,  3, 20,  0,  2,  1, 21, 22, 23 },
    { 12, 10, 11, 13, 14,  9, 15,  7,  8, 16,  5,  6,
      17,  4, 18, 19,  3,  2, 20,  0,  1, 21, 22, 23 },
    {  8,  9, 10, 11, 12, 13,  7, 14,  6, 15,  5, 16,
       4, 17,  3, 18,  0,  1,  2, 19, 20, 21, 22, 23 },
    {  8,  9, 10, 11, 12, 13,  7, 14,  6, 15,  4,  5,
      16,  3, 17, 18, 19,  2, 20,  1,  0, 21, 22, 23 },
    { 12, 10, 11, 13,  7,  8,  9, 14,  6, 15,  4,  5,
      16,  3, 17,  2, 18, 19,  1, 20, 21,  0, 22, 23 },
    { 12, 11, 13, 14,  8,  9, 10, 15,  6,  7, 16,  5,
      17, 18,  4, 19,  3,  2,  1, 20,  0, 21, 22, 23 },
    { 12, 10, 11, 13, 14,  9, 15,  8, 16, 17,  6,  7,
      18,  5, 19,  4, 20,  0,  1,  2,  3, 21, 22, 23 },
};


static const uint8_t cvh_huffbits0[181] = {
     1,  4,  4,  5,  5,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14,
    14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16,
    16,
};

static const uint8_t cvh_huffsyms0[181] = {
      0,   1,  14,  15,  28,   2,   3,  16,  29,  42,   4,   5,  17,  18,  30,
     43,  56,  57,   6,   7,   8,  19,  20,  31,  32,  44,  58,  70,  71,  84,
     85,  98,  99,   9,  10,  21,  22,  23,  24,  33,  34,  35,  36,  45,  46,
     47,  48,  59,  60,  61,  72,  73,  74,  86,  87, 100, 101, 112, 113, 114,
    126, 127, 140, 141,  11,  25,  37,  38,  39,  49,  50,  51,  52,  62,  63,
     64,  65,  75,  76,  77,  78,  88,  89, 102, 103, 115, 116, 117, 128, 129,
    130, 131, 142, 143, 154, 155, 156,  12,  13,  26,  27,  40,  53,  66,  67,
     79,  80,  90,  91,  92, 104, 105, 106, 118, 119, 132, 144, 145, 157, 158,
    168, 169, 170, 182, 183,  41,  54,  68,  81,  93,  94, 107, 108, 120, 122,
    133, 134, 146, 159, 160, 171, 184,  55,  69,  82,  95,  96, 109, 121, 147,
    148, 161, 172, 173, 174, 185, 186,  83, 110, 123, 135, 136, 149, 150, 187,
     97, 111, 124, 151, 162, 163, 175, 188, 125, 137, 138, 164, 176, 177, 189,
    190,
};

static const uint8_t cvh_huffbits1[94] = {
     1,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,
     7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 14,
    14, 15, 16, 16,
};


static const uint8_t cvh_huffsyms1[94] = {
     0,  1, 10,  2, 11, 20, 21,  3, 12, 22, 30, 31,  4, 13, 14, 23, 32, 40, 41,
     5,  6, 15, 16, 24, 25, 33, 34, 42, 43, 50, 51, 52, 60, 61, 62,  7, 17, 18,
    26, 27, 35, 36, 44, 45, 53, 54, 63, 70, 71, 72, 80, 81, 82,  8,  9, 28, 37,
    46, 55, 56, 64, 73, 83, 90, 91, 19, 29, 38, 47, 48, 57, 65, 66, 74, 84, 92,
    39, 58, 67, 75, 76, 85, 93, 49, 68, 94, 59, 77, 78, 86, 95, 69, 87, 96,
};

static const uint8_t cvh_huffbits2[48] = {
     1,  3,  4,  4,  5,  5,  5,  5,  6,  6,  7,  7,  7,  7,  7,  8,  8,  8,
     8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10,
    10, 10, 11, 11, 12, 12, 12, 13, 14, 15, 16, 16,
};

static const uint8_t cvh_huffsyms2[48] = {
     0,  7,  1,  8,  2,  9, 14, 15, 16, 22,  3, 10, 17, 21, 23,  4, 11, 18, 24,
    28, 29, 30, 35,  5, 12, 25, 31, 36, 37, 42,  6, 13, 19, 20, 26, 32, 38, 43,
    39, 44, 27, 33, 45, 46, 34, 40, 41, 47,
};

static const uint8_t cvh_huffbits3[520] = {
     2,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
};

static const uint16_t cvh_huffsyms3[520] = {
      0,   1, 125,   5,   6,  25,  30, 150,   2,   7,  26,  31, 126, 130, 131,
    151, 155, 156, 250, 275,  10,  35,  36,  50,  55, 175, 180,   3,   8,  11,
     12,  27,  32,  37,  56, 127, 132, 136, 152, 157, 160, 161, 176, 181, 251,
    255, 256, 276, 280, 281, 300, 305, 375, 400,  15,  16,  40,  41,  51,  60,
     61,  75,  80, 135, 162, 177, 185, 186, 200, 205, 301, 306, 405, 425, 500,
    525,   4,   9,  13,  17,  20,  28,  33,  38,  42,  52,  57,  81,  85, 128,
    133, 137, 140, 141, 158, 165, 166, 182, 187, 191, 206, 210, 257, 261, 277,
    282, 285, 286, 310, 311, 325, 330, 376, 380, 401, 406, 430,  21,  29,  46,
     62,  65,  66,  76,  86, 100, 105, 142, 153, 163, 190, 201, 211, 225, 230,
    252, 260, 262, 287, 302, 307, 381, 402, 426, 431, 450, 455, 505, 550,  14,
     18,  34,  43,  45,  53,  58,  67,  70,  71,  77,  87, 138, 146, 167, 168,
    171, 178, 183, 192, 207, 216, 235, 258, 265, 283, 291, 312, 315, 316, 326,
    331, 332, 335, 336, 350, 407, 410, 411, 530, 555,  22,  39,  47,  59,  63,
     82,  90,  91, 101, 106, 110, 111, 129, 134, 145, 154, 159, 170, 172, 188,
    195, 196, 202, 212, 215, 226, 231, 236, 253, 263, 266, 267, 278, 288, 290,
    292, 303, 317, 337, 355, 356, 377, 382, 385, 386, 432, 436, 451, 456, 460,
    501, 506, 526, 531, 551,  68,  72, 115, 147, 164, 184, 272, 295, 296, 297,
    309, 333, 340, 360, 387, 416, 427, 435, 437, 480, 510, 532, 556,  19,  44,
     54,  83,  97, 104, 107, 143, 173, 193, 208, 237, 268, 313, 320, 327, 341,
    351, 352, 378, 403, 412, 441, 442, 457, 475, 511, 515, 527, 528, 536, 552,
     23,  24,  48,  49,  64,  69,  73,  78,  79,  84,  88,  89,  92,  93,  94,
     95,  96,  98, 102, 103, 108, 109, 112, 113, 116, 117, 118, 120, 121, 139,
    144, 148, 149, 169, 174, 179, 189, 194, 197, 198, 203, 204, 209, 213, 214,
    217, 218, 219, 220, 221, 222, 223, 227, 228, 229, 232, 233, 234, 238, 240,
    241, 242, 243, 245, 246, 254, 259, 264, 269, 270, 271, 273, 279, 284, 289,
    293, 294, 298, 304, 308, 314, 318, 319, 321, 322, 323, 328, 329, 334, 338,
    339, 342, 343, 345, 346, 347, 353, 357, 358, 361, 362, 363, 365, 366, 367,
    379, 383, 384, 388, 389, 390, 391, 392, 393, 394, 395, 396, 397, 398, 404,
    408, 409, 413, 414, 415, 417, 418, 419, 420, 421, 422, 423, 428, 429, 433,
    434, 438, 439, 440, 443, 445, 446, 447, 452, 453, 454, 458, 459, 461, 462,
    463, 465, 466, 467, 468, 470, 471, 476, 477, 478, 481, 482, 483, 485, 486,
    487, 490, 491, 502, 503, 504, 507, 508, 509, 512, 513, 516, 517, 518, 520,
    521, 529, 533, 534, 535, 537, 538, 540, 541, 542, 543, 545, 546, 553, 557,
    558, 560, 561, 562, 563, 565, 566, 567, 575, 576, 577, 578, 580, 581, 582,
    583, 585, 586, 587, 590, 591, 600, 601, 605, 606,
};

static const uint8_t cvh_huffbits4[209] = {
     2,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,
     7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
};

static const uint8_t cvh_huffsyms4[209] = {
      0,   1,   4,  16,  64,  80,   5,  17,  20,  21,  65,  68,  84,  69,  81,
     85, 128,   2,   6,   8,  25,  32,  96, 100, 144,   9,  22,  24,  36,  37,
     89, 101, 132, 148,  18,  33,  66,  70,  72,  73,  82,  86,  88,  97, 129,
    133, 145, 149, 160, 164, 192,   3,   7,  10,  26,  40,  41, 104, 105, 112,
    208,  12,  13,  28,  29,  48,  52,  74,  90, 102, 116, 152, 161, 165,  19,
     23,  34,  38,  83,  93,  98, 113, 134, 136, 137, 150, 153, 193, 196, 209,
    212,  42,  49,  53,  67,  71,  77,  87,  92, 117, 130, 146, 197,  11,  44,
     45,  56,  76, 106, 108, 131, 168, 169, 176, 180, 213, 224,  14,  15,  27,
     30,  31,  35,  39,  43,  46,  50,  51,  54,  55,  57,  58,  60,  61,  75,
     78,  79,  91,  94,  95,  99, 103, 107, 109, 110, 114, 115, 118, 119, 120,
    121, 122, 124, 125, 135, 138, 139, 140, 141, 142, 147, 151, 154, 155, 156,
    157, 158, 162, 163, 166, 167, 170, 172, 173, 177, 178, 181, 182, 184, 185,
    194, 195, 198, 199, 200, 201, 202, 204, 205, 210, 211, 214, 215, 216, 217,
    218, 220, 221, 225, 226, 228, 229, 230, 232, 233, 240, 241, 244, 245,
};

static const uint8_t cvh_huffbits5[192] = {
     2,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,
     6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,
     9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
};

static const uint8_t cvh_huffsyms5[192] = {
      0,   1,   3,   9,  27,  81,   4,  12,  36,  82,  84, 108,  10,  13,  28,
     30,  39,  90, 109, 117,  31,  37,  40,  85,  91,  93, 111, 120,   2,  54,
     94, 112, 118, 121, 162, 189,   5,   6,  18, 135,   7,  15,  21,  45,  63,
    163, 171,  11,  16,  19,  48,  57,  83,  87,  99, 144, 165, 198,  14,  29,
     32,  33,  34,  42,  46,  58,  66,  86,  88,  96, 102, 114, 126, 127, 129,
    138, 166, 172, 174, 190, 192,  22,  38,  41,  43,  49,  55,  64,  92, 100,
    103, 110, 130, 136, 139, 145, 147, 148, 175, 193, 199, 201,   8,  24,  95,
     97, 115, 119, 123, 153, 180, 216,  17,  20,  23,  25,  35,  44,  47,  50,
     51,  52,  56,  59,  60,  61,  65,  67,  68,  69,  70,  72,  73,  75,  76,
     89,  98, 101, 104, 105, 106, 113, 116, 122, 124, 125, 128, 131, 132, 133,
    137, 140, 141, 142, 146, 149, 150, 151, 154, 156, 157, 164, 167, 168, 169,
    173, 176, 177, 178, 181, 183, 184, 191, 194, 195, 196, 200, 202, 203, 204,
    205, 207, 208, 210, 211, 217, 219, 220, 225, 226, 228, 229,
};

static const uint8_t cvh_huffbits6[32] = {
     1,  4,  4,  4,  4,  4,  6,  6,  6,  6,  6,  6,  6,  6,  6,  7,  8,  8,
     8,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 11, 11,
};

static const uint8_t cvh_huffsyms6[32] = {
     0,  1,  2,  4,  8, 16,  3,  5,  6,  9, 10, 12, 17, 20, 24, 18,  7, 11, 14,
    19, 22, 26, 28, 13, 21, 25, 30, 15, 27, 29, 23, 31,
};

static const void* const cvh_huffsyms[7] = {
    cvh_huffsyms0, cvh_huffsyms1, cvh_huffsyms2, cvh_huffsyms3,
    cvh_huffsyms4, cvh_huffsyms5, cvh_huffsyms6,
};

static const uint8_t* const cvh_huffbits[7] = {
    cvh_huffbits0, cvh_huffbits1, cvh_huffbits2, cvh_huffbits3,
    cvh_huffbits4, cvh_huffbits5, cvh_huffbits6,
};


static const uint8_t ccpl_huffsyms2[3] = {
     1,  0,  2,
};

static const uint8_t ccpl_huffsyms3[7] = {
     3,  2,  4,  5,  1,  0,  6,
};

static const uint8_t ccpl_huffsyms4[15] = {
     7,  6,  8,  5,  9,  4, 10,  3, 11,  2, 12,  0,  1, 13, 14,
};

static const uint8_t ccpl_huffsyms5[31] = {
    15, 14, 16, 12, 13, 17, 18, 10, 11, 19, 20,  8,  9, 21, 22,  6,  7, 23, 24,
     4,  5, 25, 26,  0,  1,  2,  3, 27, 28, 29, 30,
};

static const uint8_t ccpl_huffsyms6[63] = {
    31, 30, 32, 28, 29, 33, 34, 26, 27, 35, 36, 22, 23, 24, 25, 37, 38, 39, 40,
    18, 19, 20, 21, 41, 42, 43, 44, 13, 14, 15, 16, 17, 45, 46, 47, 48,  9, 10,
    11, 12, 49, 50, 51, 52, 53,  5,  6,  7,  8, 54, 55, 56, 57,  4, 58,  3, 59,
     2, 60, 61,  1,  0, 62,
};

static const uint8_t ccpl_huffbits2[3] = {
     1,  2,  2,
};

static const uint8_t ccpl_huffbits3[7] = {
     1,  2,  3,  4,  5,  6,  6,
};

static const uint8_t ccpl_huffbits4[15] = {
     1,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  8,  8,
};

static const uint8_t ccpl_huffbits5[31] = {
     1,  3,  3,  5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,
     8,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10,
};

static const uint8_t ccpl_huffbits6[63] = {
     1,  3,  4,  5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7,
     7,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12,
    12, 13, 13, 14, 14, 14, 15, 16, 16,
};

static const uint8_t *const ccpl_huffsyms[5] = {
    ccpl_huffsyms2, ccpl_huffsyms3,
    ccpl_huffsyms4, ccpl_huffsyms5, ccpl_huffsyms6
};

static const uint8_t* const ccpl_huffbits[5] = {
    ccpl_huffbits2,ccpl_huffbits3,
    ccpl_huffbits4,ccpl_huffbits5,ccpl_huffbits6
};


//Coupling tables

static const int cplband[51] = {
    0,1,2,3,4,5,6,7,8,9,
    10,11,11,12,12,13,13,14,14,14,
    15,15,15,15,16,16,16,16,16,17,
    17,17,17,17,17,18,18,18,18,18,
    18,18,19,19,19,19,19,19,19,19,
    19,
};

// The 1 and 0 at the beginning/end are to prevent overflows with
// bitstream-read indexes. E.g. if n_bits=5, we can access any
// index from [1, (1<<n_bits)] for the first decoupling coeff,
// and (1<<n_bits)-coeff1 as index for coeff2, i.e.:
// coeff1_idx = [1, 32], and coeff2_idx = [0, 31].
// These values aren't part of the tables in the original binary.

static const float cplscale2[5] = {
1,
0.953020632266998,0.70710676908493,0.302905440330505,
0,
};

static const float cplscale3[9] = {
1,
0.981279790401459,0.936997592449188,0.875934481620789,0.70710676908493,
0.482430040836334,0.349335819482803,0.192587479948997,
0,
};

static const float cplscale4[17] = {
1,
0.991486728191376,0.973249018192291,0.953020632266998,0.930133521556854,
0.903453230857849,0.870746195316315,0.826180458068848,0.70710676908493,
0.563405573368073,0.491732746362686,0.428686618804932,0.367221474647522,
0.302905440330505,0.229752898216248,0.130207896232605,
0,
};

static const float cplscale5[33] = {
1,
0.995926380157471,0.987517595291138,0.978726446628571,0.969505727291107,
0.95979779958725,0.949531257152557,0.938616216182709,0.926936149597168,
0.914336204528809,0.900602877140045,0.885426938533783,0.868331849575043,
0.84851086139679,0.824381768703461,0.791833400726318,0.70710676908493,
0.610737144947052,0.566034197807312,0.529177963733673,0.495983630418777,
0.464778542518616,0.434642940759659,0.404955863952637,0.375219136476517,
0.344963222742081,0.313672333955765,0.280692428350449,0.245068684220314,
0.205169528722763,0.157508864998817,0.0901700109243393,
0,
};

static const float cplscale6[65] = {
1,
0.998005926609039,0.993956744670868,0.989822506904602,0.985598564147949,
0.981279790401459,0.976860702037811,0.972335040569305,0.967696130275726,
0.962936460971832,0.958047747612000,0.953020632266998,0.947844684123993,
0.942508161067963,0.936997592449188,0.931297719478607,0.925390899181366,
0.919256627559662,0.912870943546295,0.906205296516418,0.899225592613220,
0.891890347003937,0.884148240089417,0.875934481620789,0.867165684700012,
0.857730865478516,0.847477376461029,0.836184680461884,0.823513329029083,
0.808890223503113,0.791194140911102,0.767520070075989,0.707106769084930,
0.641024887561798,0.611565053462982,0.587959706783295,0.567296981811523,
0.548448026180267,0.530831515789032,0.514098942279816,0.498019754886627,
0.482430040836334,0.467206478118896,0.452251672744751,0.437485188245773,
0.422837972640991,0.408248275518417,0.393658757209778,0.379014074802399,
0.364258885383606,0.349335819482803,0.334183186292648,0.318732559680939,
0.302905440330505,0.286608695983887,0.269728302955627,0.252119421958923,
0.233590632677078,0.213876649737358,0.192587479948997,0.169101938605309,
0.142307326197624,0.109772264957428,0.0631198287010193,
0,
};

static const float* const cplscales[5] = {
    cplscale2, cplscale3, cplscale4, cplscale5, cplscale6,
};

#endif /* AVCODEC_COOKDATA_H */
