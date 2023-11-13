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

#include <stdint.h>

// cat 1 and 2 are defined in vp8data.h
static const uint8_t vp8_dct_cat3_prob[] = {
    173, 148, 140, 0
};
static const uint8_t vp8_dct_cat4_prob[] = {
    176, 155, 140, 135, 0
};
static const uint8_t vp8_dct_cat5_prob[] = {
    180, 157, 141, 134, 130, 0
};
static const uint8_t vp8_dct_cat6_prob[] = {
    254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0
};

// only used for cat3 and above; cat 1 and 2 are referenced directly.
const uint8_t *const ff_vp8_dct_cat_prob[] = {
    vp8_dct_cat3_prob,
    vp8_dct_cat4_prob,
    vp8_dct_cat5_prob,
    vp8_dct_cat6_prob,
};

const uint8_t ff_vp8_token_update_probs[4][8][3][11] = {
    {
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255 },
            { 250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
    },
    {
        {
            { 217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255 },
            { 234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255 },
        },
        {
            { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
    },
    {
        {
            { 186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255 },
            { 251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255 },
        },
        {
            { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
    },
    {
        {
            { 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255 },
            { 248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
        {
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
            { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        },
    },
};
