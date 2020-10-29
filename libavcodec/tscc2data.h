/*
 * TechSmith Screen Codec 2 (aka Dora) decoder
 * Copyright (c) 2012 Konstantin Shishkov
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

#ifndef AVCODEC_TSCC2DATA_H
#define AVCODEC_TSCC2DATA_H

#include <stdint.h>

#define NUM_VLC_SETS 13

static const uint16_t tscc2_quants[NUM_VLC_SETS][3] = {
    {  655,  861, 1130 }, {  983, 1291, 1695 }, { 1311, 1721, 2260 },
    { 1638, 2151, 2825 }, { 1966, 2582, 3390 }, { 2294, 3012, 3955 },
    { 2621, 3442, 4520 }, { 2949, 3872, 5085 }, { 3277, 4303, 5650 },
    { 3604, 4733, 6215 }, { 3932, 5163, 6780 }, { 4260, 5593, 7345 },
    { 4588, 6024, 7910 },
};

#define DC_VLC_COUNT 47

static const uint16_t tscc2_dc_vlc_syms[DC_VLC_COUNT] = {
    0x0FF, 0x001, 0x0FC, 0x0F1, 0x0EA, 0x017, 0x00E, 0x016, 0x0EB, 0x009,
    0x0F6, 0x004, 0x006, 0x0F2, 0x015, 0x014, 0x00D, 0x0EC, 0x0ED, 0x100,
    0x0FA, 0x0F7, 0x008, 0x00C, 0x013, 0x0EE, 0x0F3, 0x0F4, 0x005, 0x0FE,
    0x002, 0x0FB, 0x0F8, 0x012, 0x011, 0x00B, 0x0FD, 0x003, 0x007, 0x0EF,
    0x0F0, 0x0F5, 0x0F9, 0x00F, 0x010, 0x00A, 0x000,
};

static const uint8_t tscc2_dc_vlc_lens[DC_VLC_COUNT] = {
     3,  3,  6,  9, 10, 10,  9, 10, 10,  8,  8,  6,  7,  9, 10, 10,  9, 10,
    10,  5,  7,  8,  8,  9, 10, 10,  9,  9,  7,  5,  5,  7,  8, 10, 10,  9,
     6,  6,  8, 10, 10,  9,  8, 10, 10,  9,  1,
};

static const uint8_t tscc2_nc_vlc_syms[NUM_VLC_SETS][16] = {
    { 0x00, 0x08, 0x0C, 0x0B, 0x0D, 0x07, 0x06, 0x04,
      0x02, 0x0E, 0x0F, 0x09, 0x0A, 0x05, 0x03, 0x01 },
    { 0x0A, 0x0C, 0x07, 0x0F, 0x0B, 0x0D, 0x04, 0x02,
      0x06, 0x0E, 0x05, 0x09, 0x08, 0x03, 0x01, 0x00 },
    { 0x08, 0x0D, 0x04, 0x0C, 0x07, 0x0A, 0x0E, 0x02,
      0x0B, 0x06, 0x05, 0x0F, 0x09, 0x03, 0x01, 0x00 },
    { 0x04, 0x0E, 0x0C, 0x09, 0x08, 0x07, 0x0A, 0x02,
      0x06, 0x0B, 0x05, 0x0F, 0x0D, 0x03, 0x01, 0x00 },
    { 0x0D, 0x0C, 0x04, 0x09, 0x08, 0x0B, 0x07, 0x02,
      0x06, 0x0A, 0x0F, 0x0E, 0x05, 0x03, 0x01, 0x00 },
    { 0x01, 0x0A, 0x06, 0x07, 0x05, 0x03, 0x0D, 0x0C,
      0x04, 0x0F, 0x0E, 0x0B, 0x09, 0x08, 0x02, 0x00 },
    { 0x01, 0x08, 0x06, 0x07, 0x0D, 0x0C, 0x05, 0x04,
      0x0F, 0x0E, 0x0B, 0x09, 0x0A, 0x03, 0x02, 0x00 },
    { 0x01, 0x0D, 0x0C, 0x08, 0x06, 0x07, 0x05, 0x0F,
      0x0E, 0x0B, 0x04, 0x0A, 0x09, 0x03, 0x02, 0x00 },
    { 0x01, 0x0D, 0x0F, 0x0E, 0x08, 0x07, 0x06, 0x0C,
      0x0B, 0x05, 0x04, 0x0A, 0x09, 0x03, 0x02, 0x00 },
    { 0x03, 0x02, 0x09, 0x08, 0x0C, 0x0B, 0x07, 0x06,
      0x05, 0x04, 0x0D, 0x0F, 0x0E, 0x0A, 0x01, 0x00 },
    { 0x03, 0x02, 0x09, 0x0C, 0x0B, 0x08, 0x07, 0x06,
      0x0D, 0x0F, 0x0E, 0x0A, 0x05, 0x04, 0x01, 0x00 },
    { 0x03, 0x02, 0x09, 0x0C, 0x0B, 0x08, 0x07, 0x06,
      0x0D, 0x0F, 0x0E, 0x0A, 0x04, 0x05, 0x01, 0x00 },
    { 0x03, 0x02, 0x09, 0x0C, 0x0B, 0x08, 0x07, 0x0D,
      0x0F, 0x0E, 0x0A, 0x06, 0x05, 0x04, 0x01, 0x00 },
};

static const uint8_t tscc2_nc_vlc_lens[NUM_VLC_SETS][16] = {
    { 1, 6, 6, 6, 6, 6, 6, 5, 3, 6, 6, 7, 7, 6, 4, 3 },
    { 6, 6, 6, 6, 6, 6, 5, 3, 6, 6, 6, 7, 7, 4, 3, 1 },
    { 6, 6, 5, 6, 6, 6, 6, 3, 6, 6, 6, 7, 7, 4, 3, 1 },
    { 5, 6, 6, 6, 6, 6, 6, 3, 6, 6, 6, 7, 7, 4, 3, 1 },
    { 6, 6, 5, 6, 6, 6, 6, 3, 6, 6, 7, 7, 6, 4, 3, 1 },
    { 2, 6, 6, 6, 6, 4, 7, 7, 6, 8, 8, 7, 7, 7, 4, 1 },
    { 2, 6, 6, 6, 7, 7, 6, 6, 8, 8, 7, 7, 7, 4, 4, 1 },
    { 2, 7, 7, 6, 6, 6, 6, 8, 8, 7, 6, 7, 7, 4, 4, 1 },
    { 2, 7, 8, 8, 6, 6, 6, 7, 7, 6, 6, 7, 7, 4, 4, 1 },
    { 3, 3, 6, 6, 7, 7, 6, 6, 6, 6, 8, 9, 9, 7, 3, 1 },
    { 3, 3, 6, 7, 7, 6, 6, 6, 8, 9, 9, 7, 6, 6, 3, 1 },
    { 3, 3, 6, 7, 7, 6, 6, 6, 8, 9, 9, 7, 6, 6, 3, 1 },
    { 3, 3, 6, 7, 7, 6, 6, 8, 9, 9, 7, 6, 6, 6, 3, 1 },
};

static const uint16_t ac_vlc_desc0_syms[172] = {
    0x0FC0, 0x0040, 0x0FF1, 0x0011, 0x0FA0, 0x0FE5, 0x0140, 0x0280, 0x0D60,
    0x0210, 0x0FC6, 0x0FD6, 0x0200, 0x0F95, 0x0101, 0x0190, 0x0FF5, 0x0FF2,
    0x0060, 0x0FE1, 0x0021, 0x0F41, 0x0270, 0x0D80, 0x0055, 0x0FB2, 0x00F1,
    0x0120, 0x0F30, 0x0015, 0x0090, 0x0FE3, 0x0FA1, 0x0E00, 0x01F0, 0x0F81,
    0x0EE0, 0x0FD1, 0x0F70, 0x0FA3, 0x0121, 0x0FC5, 0x0E70, 0x0023, 0x0180,
    0x00C1, 0x0F51, 0x0FE2, 0x0031, 0x0012, 0x0061, 0x00A5, 0x0ED1, 0x0D90,
    0x0091, 0x0E10, 0x0FE4, 0x0043, 0x0024, 0x0E80, 0x01E0, 0x0DA0, 0x0FA5,
    0x00D0, 0x0022, 0x0110, 0x0FE0, 0x0020, 0x0EF0, 0x0F45, 0x0260, 0x0045,
    0x0081, 0x0F40, 0x0F80, 0x0080, 0x00C0, 0x0E20, 0x0250, 0x0052, 0x0063,
    0x0018, 0x0FC3, 0x0170, 0x0111, 0x0F73, 0x0240, 0x0DC0, 0x0FB0, 0x0F00,
    0x0100, 0x0FC1, 0x0160, 0x0DB0, 0x00B1, 0x0026, 0x0F31, 0x0FF8, 0x0EA0,
    0x0150, 0x0065, 0x0FE6, 0x0E90, 0x0E30, 0x01C0, 0x0FD0, 0x0030, 0x0FF0,
    0x0010, 0x0050, 0x00B0, 0x0FF4, 0x0FF3, 0x0046, 0x0053, 0x0230, 0x0FB3,
    0x0EB0, 0x0035, 0x0FB5, 0x00E1, 0x0CD1, 0x0ED5, 0x0F50, 0x0041, 0x0F10,
    0x01B0, 0x01D0, 0x0F91, 0x0F90, 0x0070, 0x00F0, 0x0FB1, 0x0E40, 0x0DD0,
    0x0075, 0x0E50, 0x0FC2, 0x0F83, 0x0FD2, 0x00A1, 0x0EC0, 0x0013, 0x0014,
    0x0F61, 0x01A1, 0x0220, 0x0FD5, 0x0DE0, 0x0F21, 0x0016, 0x0F60, 0x0032,
    0x01A0, 0x0036, 0x00D1, 0x0FD3, 0x0025, 0x0130, 0x1000, 0x0051, 0x0FF6,
    0x0ED0, 0x0E60, 0x0042, 0x0DF0, 0x0F20, 0x00E0, 0x0033, 0x0F71, 0x0071,
    0x00A0,
};

static const uint16_t ac_vlc_desc1_syms[169] = {
    0x00D0, 0x0E50, 0x00D1, 0x0091, 0x0160, 0x0F31, 0x0EE1, 0x0250, 0x0F70,
    0x0051, 0x0F41, 0x0063, 0x0150, 0x0EF0, 0x01A0, 0x0F51, 0x0FD5, 0x0F93,
    0x0DC0, 0x0240, 0x0095, 0x0FD2, 0x00C0, 0x0FC0, 0x0040, 0x0090, 0x0016,
    0x0F40, 0x0FA0, 0x0026, 0x0EB0, 0x0EF1, 0x0DF0, 0x0053, 0x0F01, 0x01F0,
    0x0FC2, 0x0FF6, 0x0FF5, 0x0060, 0x0015, 0x0F85, 0x0DE1, 0x0032, 0x0100,
    0x0046, 0x0DB0, 0x0FB5, 0x0F81, 0x0FA1, 0x0012, 0x0011, 0x0FF1, 0x0FF2,
    0x0F00, 0x0E00, 0x0F21, 0x0F45, 0x0FD3, 0x0E60, 0x00C1, 0x0E80, 0x0035,
    0x0045, 0x0140, 0x0042, 0x0FB2, 0x0EB6, 0x0033, 0x0FC5, 0x0190, 0x0FE6,
    0x0024, 0x0F61, 0x0085, 0x0E33, 0x0E70, 0x0EC0, 0x00B0, 0x0F50, 0x0F80,
    0x0080, 0x0023, 0x0FE4, 0x01E0, 0x0F11, 0x0081, 0x0FC1, 0x0FD1, 0x0052,
    0x0FA5, 0x0F95, 0x0EC6, 0x00B1, 0x0111, 0x0061, 0x00F0, 0x0FE3, 0x0FE1,
    0x0021, 0x0041, 0x0FE2, 0x0055, 0x0FC6, 0x0E10, 0x0180, 0x0E20, 0x0DE0,
    0x0022, 0x0025, 0x0FB3, 0x0FA3, 0x0036, 0x0FC3, 0x00E0, 0x0FE0, 0x0020,
    0x0050, 0x0FB0, 0x1000, 0x0031, 0x01D0, 0x0018, 0x00A1, 0x0FB6, 0x00C6,
    0x0043, 0x0F10, 0x0F20, 0x0101, 0x0E30, 0x0FA6, 0x00F1, 0x0ED0, 0x0FD0,
    0x00A0, 0x0FD6, 0x0DA0, 0x0E31, 0x0120, 0x0130, 0x0170, 0x01C0, 0x00E1,
    0x0F60, 0x0014, 0x0070, 0x0F90, 0x0030, 0x0FB1, 0x0075, 0x0E90, 0x0F91,
    0x0FF4, 0x0013, 0x0FF3, 0x0EE0, 0x0FF8, 0x0065, 0x0E40, 0x01B0, 0x0F30,
    0x0071, 0x0FE5, 0x0EA0, 0x0F71, 0x0110, 0x0FF0, 0x0010,
};

static const uint16_t ac_vlc_desc2_syms[165] = {
    0x0051, 0x0E61, 0x0E81, 0x0E80, 0x0FF7, 0x0E00, 0x0E30, 0x0F71, 0x0130,
    0x00F0, 0x0170, 0x0E70, 0x0F81, 0x0F40, 0x0FF3, 0x0040, 0x0013, 0x0FA0,
    0x0FC0, 0x0060, 0x0065, 0x0E40, 0x0ED0, 0x0043, 0x0086, 0x0F93, 0x0036,
    0x0035, 0x0F10, 0x0EA1, 0x01B3, 0x0F36, 0x0FD5, 0x0FA1, 0x0F41, 0x0096,
    0x0EB3, 0x0F26, 0x0F16, 0x0FB6, 0x0FB5, 0x0160, 0x0FD6, 0x0F80, 0x0080,
    0x1000, 0x00B0, 0x0FE5, 0x0091, 0x0E90, 0x0241, 0x0231, 0x0FF5, 0x0015,
    0x0081, 0x0120, 0x0EA0, 0x0053, 0x0F51, 0x0FC2, 0x0F50, 0x0FF6, 0x0061,
    0x0EB1, 0x0018, 0x0FF8, 0x0240, 0x0042, 0x0085, 0x0EF1, 0x0FD1, 0x0FF2,
    0x0012, 0x0016, 0x0FD2, 0x0FC6, 0x0063, 0x00A5, 0x0F20, 0x0055, 0x0052,
    0x0E10, 0x0150, 0x00C1, 0x01D0, 0x0F95, 0x0032, 0x00E0, 0x01A0, 0x0EE1,
    0x0024, 0x0EF0, 0x0FC1, 0x0F60, 0x0031, 0x0190, 0x0F11, 0x0FB2, 0x0F21,
    0x0110, 0x0FC3, 0x0FE4, 0x0F01, 0x0095, 0x0FD3, 0x0FB3, 0x0E71, 0x0F61,
    0x0EB0, 0x0026, 0x00A0, 0x00D0, 0x0045, 0x00A1, 0x00B1, 0x0180, 0x01C0,
    0x0FF1, 0x0011, 0x0FB0, 0x0050, 0x0F90, 0x0041, 0x0033, 0x0F91, 0x0F30,
    0x0FE1, 0x0FE0, 0x0020, 0x0FD0, 0x0070, 0x0FE2, 0x0E50, 0x0FA3, 0x0F75,
    0x0EA3, 0x01B0, 0x0140, 0x0023, 0x0FE3, 0x0021, 0x0030, 0x0100, 0x0071,
    0x0FC5, 0x0EC0, 0x0F00, 0x0090, 0x0022, 0x0F06, 0x0F31, 0x0FA5, 0x00D1,
    0x0E60, 0x0075, 0x0F70, 0x0014, 0x0FF4, 0x0025, 0x0FB1, 0x0FE6, 0x0EE0,
    0x00C0, 0x0FF0, 0x0010,
};

static const uint16_t ac_vlc_desc3_syms[162] = {
    0x0FC5, 0x0EC1, 0x0033, 0x0FE3, 0x0FD6, 0x0110, 0x00E0, 0x0071, 0x0F36,
    0x0095, 0x01A1, 0x0F71, 0x0060, 0x0FA0, 0x0FE2, 0x0F20, 0x0F21, 0x0085,
    0x0150, 0x0053, 0x0023, 0x0022, 0x0FF4, 0x0043, 0x0E70, 0x0034, 0x0017,
    0x0140, 0x0170, 0x0EF0, 0x0F50, 0x0F80, 0x00B0, 0x0F26, 0x00D1, 0x0065,
    0x0113, 0x0FF7, 0x0073, 0x01E1, 0x0EF3, 0x0F31, 0x0FB5, 0x0FC0, 0x0040,
    0x0080, 0x0FA1, 0x0FD3, 0x0075, 0x0F41, 0x0FD4, 0x0F83, 0x0EB0, 0x0061,
    0x0013, 0x0FF3, 0x0F10, 0x0F30, 0x0100, 0x0EB1, 0x0F93, 0x0130, 0x00D0,
    0x00A0, 0x00C1, 0x0F95, 0x0FB3, 0x0FC3, 0x0FE5, 0x0FF5, 0x0F81, 0x0F00,
    0x0091, 0x0F66, 0x01B1, 0x0F51, 0x0F60, 0x0FD1, 0x0180, 0x0FF8, 0x0076,
    0x0FB2, 0x0063, 0x0042, 0x0081, 0x0052, 0x0EE1, 0x0FC2, 0x0160, 0x0036,
    0x01D0, 0x0FD2, 0x0EA0, 0x0018, 0x0E80, 0x0FD5, 0x0070, 0x0F90, 0x0FB0,
    0x0015, 0x0032, 0x0123, 0x0F01, 0x0EE3, 0x0ED1, 0x00C0, 0x0FC1, 0x0FF2,
    0x0012, 0x0050, 0x00A1, 0x00F0, 0x0ED0, 0x0FC6, 0x0ED3, 0x01D1, 0x0120,
    0x0016, 0x0031, 0x0FF6, 0x0F40, 0x0EC0, 0x0E90, 0x0F91, 0x0041, 0x0EF1,
    0x0F61, 0x0035, 0x0FB1, 0x0FF1, 0x0011, 0x0FE0, 0x0020, 0x0FE1, 0x0090,
    0x00B1, 0x0163, 0x0055, 0x0024, 0x0F56, 0x0FA3, 0x0FE4, 0x0F46, 0x0FE6,
    0x0025, 0x0026, 0x0EE0, 0x0FA5, 0x01C1, 0x0F70, 0x0FD0, 0x0030, 0x1000,
    0x0045, 0x0F11, 0x0066, 0x0F85, 0x0051, 0x0014, 0x0021, 0x0FF0, 0x0010,
};

static const uint16_t ac_vlc_desc4_syms[131] = {
    0x0010, 0x0FB0, 0x0050, 0x0070, 0x0FF5, 0x0FC1, 0x0090, 0x0FD1, 0x00E0,
    0x0161, 0x0085, 0x0F41, 0x0F20, 0x0FD6, 0x0F70, 0x0FD3, 0x0032, 0x0FD2,
    0x0055, 0x0191, 0x0063, 0x0091, 0x0081, 0x0F91, 0x0015, 0x0031, 0x0065,
    0x0036, 0x00B1, 0x0130, 0x0018, 0x0F93, 0x0F50, 0x0041, 0x0FD5, 0x0100,
    0x0F51, 0x00B0, 0x0FE6, 0x0FC5, 0x0F40, 0x0FF2, 0x0FE0, 0x0012, 0x0FE1,
    0x0043, 0x0F61, 0x0FA3, 0x0140, 0x0120, 0x0FB1, 0x0051, 0x0EE0, 0x0F21,
    0x0066, 0x0F03, 0x0F01, 0x0060, 0x0016, 0x0FF6, 0x0FA0, 0x0020, 0x0FF1,
    0x0FD0, 0x0030, 0x0011, 0x0021, 0x0071, 0x00D0, 0x0FE4, 0x0024, 0x0F30,
    0x0080, 0x0123, 0x00A1, 0x0F71, 0x0F00, 0x0180, 0x0EC0, 0x00F3, 0x0F31,
    0x0EF0, 0x0033, 0x0014, 0x0FC0, 0x0F80, 0x0FE3, 0x0FE2, 0x0025, 0x0FC3,
    0x00F0, 0x0053, 0x0075, 0x0F66, 0x0FF4, 0x0040, 0x0F60, 0x0022, 0x00A0,
    0x0ED0, 0x0F13, 0x0181, 0x0F76, 0x0F23, 0x0045, 0x1000, 0x0023, 0x00C0,
    0x0F56, 0x0171, 0x0F10, 0x0FA1, 0x0EB0, 0x0056, 0x0026, 0x0035, 0x0FB5,
    0x0FB3, 0x0EF1, 0x0FF3, 0x0013, 0x0F90, 0x0FA5, 0x0FC2, 0x0F11, 0x0F81,
    0x0110, 0x0042, 0x0061, 0x0FE5, 0x0FF0,
};

static const uint16_t ac_vlc_desc5_syms[132] = {
    0x0010, 0x0F81, 0x0FC5, 0x0F20, 0x0F76, 0x0061, 0x0F41, 0x00D3, 0x0FB3,
    0x0023, 0x0F31, 0x0EC0, 0x00B1, 0x0045, 0x0F50, 0x0FF5, 0x0042, 0x00C1,
    0x0EC1, 0x00D0, 0x0F33, 0x0F93, 0x0FF8, 0x0EF0, 0x0140, 0x0035, 0x0071,
    0x0FD1, 0x0FE3, 0x0FC1, 0x0FF3, 0x0013, 0x0052, 0x0F85, 0x0F51, 0x0046,
    0x0065, 0x0F21, 0x0F30, 0x0041, 0x0031, 0x0034, 0x0FD4, 0x0F43, 0x0032,
    0x0FB5, 0x0FD2, 0x0FE5, 0x0EE0, 0x0120, 0x0F23, 0x0F00, 0x0015, 0x0FE1,
    0x0FE0, 0x0020, 0x1000, 0x0081, 0x0018, 0x0075, 0x0043, 0x00C3, 0x0121,
    0x00A0, 0x0080, 0x0FA0, 0x0060, 0x0FD0, 0x0030, 0x0FF2, 0x0012, 0x0FF1,
    0x0F80, 0x0F91, 0x0141, 0x00A1, 0x0F96, 0x0FB1, 0x00C0, 0x0111, 0x0F66,
    0x00F0, 0x0F40, 0x0FE6, 0x0016, 0x0021, 0x0FC0, 0x0051, 0x00E0, 0x0F86,
    0x0033, 0x0FF6, 0x0F75, 0x0F11, 0x0055, 0x0F61, 0x0FA3, 0x0131, 0x0FD5,
    0x0FA1, 0x0FC3, 0x0024, 0x0056, 0x0FD6, 0x0F60, 0x0011, 0x0040, 0x0025,
    0x0FE4, 0x0103, 0x0091, 0x0026, 0x0F10, 0x0014, 0x0FE2, 0x0022, 0x0070,
    0x0090, 0x0100, 0x0FC2, 0x0151, 0x0FD3, 0x0FF4, 0x0050, 0x0F70, 0x0053,
    0x0110, 0x0F71, 0x00B0, 0x0F90, 0x0FB0, 0x0FF0,
};

static const uint16_t ac_vlc_desc6_syms[130] = {
    0x0010, 0x0FF4, 0x0F96, 0x0F71, 0x00D1, 0x0FF7, 0x00E3, 0x0045, 0x0FC1,
    0x1000, 0x00C0, 0x0042, 0x0120, 0x00A0, 0x0F80, 0x0FD1, 0x0F43, 0x0F63,
    0x0EF0, 0x0F86, 0x0F60, 0x0023, 0x0080, 0x00F0, 0x0FB3, 0x00E0, 0x0063,
    0x0110, 0x0F41, 0x0F93, 0x0FF8, 0x0081, 0x0FF5, 0x0041, 0x0FD2, 0x0F30,
    0x0F81, 0x00B1, 0x00E1, 0x0F00, 0x0031, 0x0060, 0x0035, 0x0F51, 0x0FB5,
    0x0FE3, 0x0FF3, 0x0013, 0x0FE0, 0x0032, 0x0075, 0x0FD4, 0x0046, 0x0F40,
    0x0F91, 0x0FE5, 0x00B3, 0x00C3, 0x0EE1, 0x0F31, 0x0FA0, 0x0FE1, 0x0FD0,
    0x0020, 0x0030, 0x0F10, 0x00A1, 0x0FA3, 0x0033, 0x0111, 0x0FA6, 0x0100,
    0x0F61, 0x0026, 0x0FB1, 0x0061, 0x0025, 0x0F95, 0x0FD6, 0x0036, 0x0034,
    0x0F20, 0x00B0, 0x0121, 0x0018, 0x0131, 0x0051, 0x0FF2, 0x0040, 0x0021,
    0x0FC0, 0x0015, 0x0090, 0x0043, 0x0FC5, 0x0056, 0x0055, 0x0016, 0x0FF1,
    0x0011, 0x0012, 0x0FF6, 0x0F50, 0x0FC2, 0x0053, 0x0F76, 0x0F85, 0x0FD3,
    0x0091, 0x0101, 0x0071, 0x0070, 0x0F90, 0x0FB0, 0x0FC3, 0x0065, 0x00F1,
    0x0F53, 0x00D0, 0x0FE2, 0x0FA1, 0x0024, 0x0FE4, 0x0022, 0x0050, 0x0FE6,
    0x0FD5, 0x0F70, 0x0014, 0x0FF0,
};

static const uint16_t ac_vlc_desc7_syms[125] = {
    0x0010, 0x0022, 0x0FD5, 0x0F71, 0x0F63, 0x0052, 0x0F31, 0x0042, 0x0024,
    0x0FE4, 0x0F86, 0x0F93, 0x00C1, 0x0025, 0x0FD1, 0x0FE6, 0x0F95, 0x00D1,
    0x0FA6, 0x0FD2, 0x00E1, 0x0017, 0x0FF7, 0x0081, 0x0FB3, 0x0046, 0x0014,
    0x0FF4, 0x0FC1, 0x0023, 0x0031, 0x0060, 0x0FA0, 0x0061, 0x00B0, 0x00C3,
    0x0F00, 0x0121, 0x0F80, 0x0FF5, 0x0041, 0x0FF8, 0x0100, 0x0032, 0x0090,
    0x0F81, 0x0F30, 0x0045, 0x0F61, 0x00C0, 0x0063, 0x0FD4, 0x0055, 0x0F70,
    0x0FF3, 0x0FD0, 0x0030, 0x0FE0, 0x0020, 0x0013, 0x0FE1, 0x0FE3, 0x0FB1,
    0x0093, 0x00B1, 0x0026, 0x0F10, 0x00F0, 0x0FA5, 0x0FB5, 0x0070, 0x0F90,
    0x0FC0, 0x0040, 0x0033, 0x0F40, 0x0FE5, 0x00A1, 0x0034, 0x0036, 0x0F96,
    0x0F91, 0x0043, 0x0F01, 0x0053, 0x0FC5, 0x0035, 0x0F51, 0x00A3, 0x0FC2,
    0x0FA3, 0x0F50, 0x00F1, 0x0071, 0x0051, 0x0021, 0x0FF2, 0x0FF1, 0x0012,
    0x0015, 0x0016, 0x00A0, 0x0FD3, 0x0065, 0x0111, 0x0FC3, 0x0091, 0x0018,
    0x0F20, 0x0050, 0x1000, 0x0FF6, 0x0FB0, 0x0FA1, 0x0101, 0x0F53, 0x00E0,
    0x0080, 0x0F60, 0x00D0, 0x0F41, 0x0F73, 0x0FE2, 0x0011, 0x0FF0,
};

static const uint16_t ac_vlc_desc8_syms[121] = {
    0x0010, 0x0F60, 0x0093, 0x00A3, 0x0F95, 0x0018, 0x0FE2, 0x0FA6, 0x0FA1,
    0x0022, 0x0090, 0x0042, 0x0F86, 0x0F93, 0x0036, 0x0FE6, 0x0F50, 0x0FD1,
    0x0060, 0x0FA0, 0x0025, 0x0FD2, 0x0046, 0x0F70, 0x0031, 0x0045, 0x0F40,
    0x0F81, 0x0FB3, 0x0111, 0x0073, 0x0023, 0x0FC1, 0x0FE4, 0x0024, 0x0081,
    0x0FA5, 0x0032, 0x0014, 0x0FF4, 0x0FD0, 0x0030, 0x0041, 0x0070, 0x0FF5,
    0x00E1, 0x0061, 0x0F73, 0x0063, 0x0F41, 0x00B3, 0x0FD5, 0x00F1, 0x0017,
    0x0FF7, 0x00A0, 0x0055, 0x00C1, 0x0F30, 0x0043, 0x0FD4, 0x0065, 0x0FF8,
    0x0FB1, 0x1000, 0x0020, 0x0FE0, 0x0040, 0x0FC0, 0x0FE1, 0x0FF3, 0x0013,
    0x0FE3, 0x0FA3, 0x0083, 0x0F96, 0x00D1, 0x0026, 0x0033, 0x0101, 0x00B1,
    0x0FB6, 0x0F90, 0x0080, 0x00E0, 0x0071, 0x0034, 0x0FC2, 0x0F20, 0x00A1,
    0x0021, 0x0050, 0x00B0, 0x0F71, 0x0FC5, 0x0F91, 0x0F80, 0x0035, 0x0F63,
    0x0053, 0x00C0, 0x0FF1, 0x0FF2, 0x0FB0, 0x0016, 0x0FB5, 0x0F51, 0x0091,
    0x0F21, 0x0FD3, 0x0FC3, 0x00D0, 0x0F83, 0x0F61, 0x0012, 0x0015, 0x0051,
    0x0FE5, 0x0FF6, 0x0011, 0x0FF0,
};

static const uint16_t ac_vlc_desc9_syms[114] = {
    0x0010, 0x0015, 0x0042, 0x0091, 0x0FD2, 0x0036, 0x0FE2, 0x0022, 0x00C0,
    0x0121, 0x0065, 0x0F31, 0x0018, 0x0F60, 0x0FF6, 0x0070, 0x00B0, 0x0045,
    0x0F71, 0x0FD1, 0x0FC1, 0x0FA1, 0x0055, 0x0FB5, 0x0FB2, 0x0F93, 0x0FC5,
    0x0023, 0x0F70, 0x0083, 0x0061, 0x0031, 0x0025, 0x0FA5, 0x0FB3, 0x0032,
    0x0FD5, 0x0081, 0x0F61, 0x0FE4, 0x0F21, 0x0073, 0x0F73, 0x0024, 0x0041,
    0x0030, 0x0FD0, 0x0014, 0x0FF4, 0x0040, 0x0FE0, 0x0FC0, 0x0080, 0x0043,
    0x00E1, 0x00D1, 0x0FE3, 0x1000, 0x0F90, 0x0FE1, 0x0FB1, 0x0026, 0x0FD4,
    0x0063, 0x0034, 0x0FA3, 0x00A3, 0x0F80, 0x0F40, 0x0017, 0x0FF7, 0x0F83,
    0x0FF5, 0x0020, 0x0050, 0x0FB0, 0x0021, 0x0013, 0x0FF3, 0x0FF1, 0x0F51,
    0x0093, 0x0FF8, 0x0F91, 0x0F50, 0x0071, 0x00B1, 0x0051, 0x0033, 0x0090,
    0x00D0, 0x00F1, 0x0FC2, 0x0FE6, 0x0FA6, 0x0FB6, 0x0FA0, 0x0FF2, 0x0060,
    0x0FD3, 0x0F30, 0x00A1, 0x0F96, 0x0053, 0x0035, 0x00A0, 0x0016, 0x00C1,
    0x0FC3, 0x0F81, 0x0FE5, 0x0012, 0x0011, 0x0FF0,
};

static const uint16_t ac_vlc_descA_syms[110] = {
    0x0010, 0x0F60, 0x0051, 0x0F90, 0x0FE2, 0x0044, 0x0FA5, 0x0053, 0x00A1,
    0x0035, 0x0022, 0x0026, 0x0073, 0x0080, 0x0FD1, 0x0015, 0x0FE5, 0x0090,
    0x0091, 0x0055, 0x0F73, 0x0F51, 0x00D1, 0x0023, 0x0FA1, 0x0061, 0x0FB3,
    0x0FC5, 0x0031, 0x0FF6, 0x1000, 0x0FD5, 0x0F91, 0x0FC1, 0x0032, 0x0F41,
    0x00B0, 0x00B1, 0x0081, 0x0FB2, 0x0F96, 0x0FD0, 0x0030, 0x0040, 0x0025,
    0x0F81, 0x0F70, 0x0FE3, 0x0FB6, 0x00A0, 0x0018, 0x0FA3, 0x0F31, 0x0FE0,
    0x0FC0, 0x0FB0, 0x0050, 0x0FE1, 0x0014, 0x0F80, 0x0FE6, 0x0FE4, 0x0043,
    0x0083, 0x0024, 0x0FB1, 0x0020, 0x0FF4, 0x0041, 0x0F50, 0x0FF8, 0x0F93,
    0x00C1, 0x0033, 0x0021, 0x0FF5, 0x0060, 0x0063, 0x0034, 0x0FD4, 0x0FC2,
    0x0071, 0x0FC6, 0x0093, 0x0045, 0x0FA6, 0x00C0, 0x0013, 0x0FF1, 0x0FF3,
    0x0F71, 0x00E1, 0x0F40, 0x0FC3, 0x0FB5, 0x0070, 0x0042, 0x0F61, 0x0F83,
    0x0FF7, 0x0017, 0x0FD2, 0x0036, 0x0FD3, 0x0016, 0x0FA0, 0x0FF2, 0x0012,
    0x0011, 0x0FF0,
};

static const uint16_t ac_vlc_descB_syms[101] = {
    0x0010, 0x0012, 0x0023, 0x0091, 0x0061, 0x0FA1, 0x0FD1, 0x0015, 0x0030,
    0x0FD0, 0x0FB3, 0x0F71, 0x0F60, 0x0FA6, 0x0063, 0x0032, 0x0FC1, 0x0031,
    0x0040, 0x0080, 0x0FD5, 0x0FE3, 0x0050, 0x0FC0, 0x0FB0, 0x0FF6, 0x0F81,
    0x0FB6, 0x0F70, 0x0F91, 0x0025, 0x1000, 0x0FE1, 0x00A1, 0x0FA3, 0x00F1,
    0x0F61, 0x0F51, 0x0081, 0x00C1, 0x0018, 0x0060, 0x0041, 0x0073, 0x0FE4,
    0x0F80, 0x0FE0, 0x0020, 0x0021, 0x0FC5, 0x0055, 0x0042, 0x0026, 0x0070,
    0x0024, 0x0043, 0x00A0, 0x0033, 0x0FF8, 0x0071, 0x0014, 0x0FF4, 0x0FB1,
    0x0FB5, 0x0034, 0x0F41, 0x0036, 0x0F90, 0x0FC6, 0x0090, 0x0FF5, 0x0FA0,
    0x0FD4, 0x0F83, 0x0083, 0x0051, 0x00B1, 0x0FD3, 0x0FF1, 0x0013, 0x0FF3,
    0x0FF2, 0x0035, 0x0045, 0x0FC2, 0x00D1, 0x0FE2, 0x0016, 0x0FC3, 0x0FD2,
    0x00B0, 0x0FE6, 0x0F93, 0x0F50, 0x0FF7, 0x0017, 0x0053, 0x0022, 0x0FE5,
    0x0011, 0x0FF0,
};

static const uint16_t ac_vlc_descC_syms[96] = {
    0x0010, 0x0012, 0x0FC3, 0x00B1, 0x00A1, 0x0022, 0x0FE5, 0x0F93, 0x0090,
    0x0061, 0x0055, 0x0042, 0x0FE6, 0x0040, 0x0030, 0x0FD1, 0x0050, 0x0015,
    0x0FD0, 0x0FC0, 0x0023, 0x0FC1, 0x0017, 0x00C1, 0x0032, 0x0FB5, 0x0FF7,
    0x00A0, 0x0060, 0x0031, 0x0041, 0x0FE3, 0x0FD5, 0x0091, 0x0053, 0x0FF8,
    0x0FA0, 0x0FF6, 0x0FB0, 0x0070, 0x0080, 0x1000, 0x0FE1, 0x0FE0, 0x0020,
    0x0021, 0x0063, 0x0033, 0x0FA1, 0x0F60, 0x0F61, 0x0043, 0x0073, 0x0FC6,
    0x0FE4, 0x00E1, 0x0034, 0x0018, 0x0F91, 0x0F80, 0x0024, 0x0026, 0x0014,
    0x0FF4, 0x0FB1, 0x0FB6, 0x0071, 0x0FA6, 0x0FD4, 0x0035, 0x0F70, 0x0036,
    0x0051, 0x0FF5, 0x0FF1, 0x0FD3, 0x0045, 0x0F81, 0x0F90, 0x0083, 0x0081,
    0x0FA3, 0x0FE2, 0x0FC5, 0x0F51, 0x0F71, 0x0FD2, 0x0FB3, 0x0FC2, 0x0025,
    0x0016, 0x0013, 0x0FF3, 0x0FF2, 0x0011, 0x0FF0,
};

static const uint8_t ac_vlc_desc0_bits[172] = {
     5,  5,  4,  4,  6,  9,  9, 12, 12, 11, 11, 11, 11, 12, 12, 10,  7,  6,
     6,  6,  6, 11, 12, 12, 11, 12, 12,  9,  8,  7,  7,  9,  9, 11, 11, 10,
     9,  7,  7, 12, 12, 11, 10,  9, 10, 11, 11,  9,  7,  6,  9, 11, 12, 12,
    10, 11, 11, 11, 11, 10, 11, 12, 12,  8,  9,  9,  4,  4,  9, 12, 12, 11,
    10,  8,  7,  7,  8, 11, 12, 12, 12, 12, 11, 10, 12, 12, 12, 12,  6,  9,
     9,  8, 10, 11, 11, 11, 12, 12, 10, 10, 11, 11, 10, 11, 11,  5,  5,  3,
     3,  6,  8,  8,  7, 12, 12, 12, 12, 10, 10, 12, 12, 12, 12,  8,  8,  9,
    11, 11, 10,  7,  7,  9,  9, 11, 12, 12, 11, 12, 12, 11, 11, 10,  7,  8,
    11, 12, 12, 11, 12, 12,  9,  8, 11, 11, 12, 12, 11, 10, 10,  6,  9,  9,
    10, 11, 12, 12,  9,  9, 11, 11, 10,  8,
};

static const uint8_t ac_vlc_desc1_bits[169] = {
     8, 11, 11, 10, 10, 11, 12, 12,  7,  8, 11, 11, 10,  9, 11, 11, 10, 12,
    12, 12, 12, 10,  8,  5,  5,  7,  8,  8,  6, 10, 10, 12, 12, 11, 12, 12,
    11,  8,  7,  6,  7, 11, 11, 10,  9, 12, 12, 11, 10,  9,  6,  4,  4,  6,
     9, 12, 12, 11, 10, 11, 11, 10, 10, 10, 10, 11, 12, 12, 10, 11, 11, 10,
    11, 11, 12, 12, 11, 10,  8,  8,  7,  7,  9, 11, 12, 12, 10,  8,  7, 12,
    12, 12, 12, 11, 11,  9,  9,  9,  6,  6,  8,  9, 11, 12, 12, 11, 12, 12,
     9,  9, 11, 11, 11, 11,  9,  4,  4,  6,  6,  6,  7, 12, 12, 11, 12, 12,
    11,  9,  9, 12, 12, 12, 12, 10,  5,  8, 11, 12, 12, 10, 10, 11, 12, 12,
     8,  8,  7,  7,  5,  9, 11, 11, 10,  8,  7,  7, 10, 12, 12, 12, 12,  9,
    10, 10, 11, 11, 10,  3,  3,
};

static const uint8_t ac_vlc_desc2_bits[165] = {
     8, 12, 12, 11, 12, 12, 11, 10, 10,  9, 11, 11, 10,  8,  6,  5,  6,  6,
     5,  6, 11, 11, 10, 10, 12, 12, 11,  9,  9, 11, 12, 12, 10,  9, 11, 11,
    12, 12, 11, 11, 11, 11, 11,  7,  7,  6,  8,  9, 10, 11, 12, 12,  7,  7,
    10, 10, 11, 11, 11, 11,  8,  8,  9, 12, 12, 12, 12, 11, 12, 12,  7,  6,
     6,  8, 10, 11, 12, 12,  9, 10, 12, 12, 11, 11, 12, 12, 10,  9, 12, 12,
    11, 10,  8,  8,  7, 12, 12, 12, 12, 10, 10, 11, 12, 12, 10, 11, 11, 11,
    11, 10,  8,  9, 11, 11, 11, 12, 12,  4,  4,  6,  6,  7,  8, 10, 10,  9,
     6,  4,  4,  5,  7,  9, 12, 12, 11, 12, 12, 11,  9,  9,  6,  5, 10, 10,
    11, 11, 10,  8,  9, 12, 12, 11, 11, 12, 12,  8,  8,  8,  9,  9, 10, 10,
     9,  3,  3,
};

static const uint8_t ac_vlc_desc3_bits[162] = {
    10, 10,  9,  8, 10, 10,  9,  9, 11, 12, 12, 10,  6,  6,  8,  9, 11, 11,
    11, 11,  8,  8,  7, 10, 11, 12, 12, 11, 11, 10,  8,  7,  8, 12, 12, 11,
    12, 12, 12, 12, 11, 11, 10,  5,  5,  7,  9,  9, 11, 11, 12, 12, 11,  9,
     6,  6,  9,  9, 10, 12, 12, 11,  9,  8, 12, 12, 11, 10,  9,  7, 10, 10,
    10, 12, 12, 11,  8,  7, 12, 12, 11, 12, 12, 11, 10, 12, 12, 11, 11, 12,
    12, 10, 11, 12, 12, 10,  7,  7,  6,  7, 10, 12, 12, 12, 12,  9,  8,  6,
     6,  6, 10, 10, 11, 11, 12, 12, 11,  8,  7,  8,  9, 11, 11, 10,  8, 11,
    11, 10,  9,  4,  4,  4,  4,  6,  8, 12, 12, 11, 11, 12, 12, 11, 11, 10,
     9, 10, 11, 12, 12,  8,  5,  5,  7, 10, 12, 12, 11,  9,  8,  6,  3,  3,
};

static const uint8_t ac_vlc_desc4_bits[131] = {
     2,  6,  6,  7,  7,  8,  8,  7, 10, 12, 12, 11, 10, 10,  8,  9, 10, 10,
    11, 12, 12, 10, 10, 10,  7,  7, 11, 12, 12, 11, 12, 12,  9,  8, 10, 11,
    11,  9, 10, 10,  9,  6,  4,  6,  6, 11, 11, 12, 12, 11,  9,  9, 11, 11,
    11, 12, 12,  7,  8,  8,  7,  4,  4,  5,  5,  4,  6, 10, 10, 11, 11, 10,
     8, 12, 12, 11, 11, 12, 12, 12, 12, 11, 10,  8,  6,  8,  9,  9,  9, 11,
    11, 12, 12, 11,  8,  6,  9,  9,  9, 11, 12, 12, 12, 12, 11,  7,  9, 10,
    12, 12, 11, 10, 12, 12, 11, 10, 12, 12, 11,  7,  7,  8, 11, 12, 12, 11,
    12, 12, 10, 10,  3,
};

static const uint8_t ac_vlc_desc5_bits[132] = {
     2, 10, 10, 10, 10,  9, 10, 11, 11,  8, 11, 12, 12, 10,  9,  7, 11, 12,
    12, 10, 11, 12, 12, 11, 11,  9,  9,  7,  8,  8,  6,  6, 12, 12, 11, 11,
    11, 10, 10,  8,  7, 12, 12, 11, 10, 10, 10,  9, 11, 12, 12, 10,  7,  6,
     4,  4,  7, 11, 12, 12, 11, 12, 12,  9,  8,  7,  7,  5,  5,  6,  6,  4,
     8, 10, 11, 12, 12,  9, 10, 12, 12, 11, 10, 10,  8,  6,  6,  9, 11, 11,
    10,  8, 12, 12, 11, 11, 12, 12, 10, 10, 11, 11, 11, 11,  9,  4,  6,  9,
    11, 12, 12, 11, 11,  8,  9,  9,  8,  9, 11, 12, 12, 10,  8,  7,  9, 12,
    12, 11, 10,  8,  7,  3,
};

static const uint8_t ac_vlc_desc6_bits[130] = {
     2,  7, 10, 10, 12, 12, 11, 10,  8,  7, 10, 11, 11,  9,  8,  7, 11, 12,
    12, 10,  9,  8,  8, 11, 11, 10, 12, 12, 11, 12, 12, 11,  7,  8, 10, 10,
    10, 12, 12, 11,  7,  7,  9, 10, 10,  8,  6,  6,  4, 10, 12, 12, 11, 10,
    10,  9, 11, 12, 12, 10,  7,  6,  5,  4,  5, 11, 12, 12, 10, 11, 12, 12,
    11, 11,  9,  9,  9, 12, 12, 12, 12, 10, 10, 11, 12, 12,  9,  6,  6,  6,
     6,  7,  9, 11, 11, 11, 11,  8,  4,  4,  6,  8, 10, 12, 12, 12, 12, 10,
    12, 12, 11,  8,  8,  7, 11, 12, 12, 11, 11,  9, 10, 11, 11,  9,  7, 10,
    10,  9,  8,  3,
};

static const uint8_t ac_vlc_desc7_bits[125] = {
     2,  8,  9, 10, 10, 12, 12, 11, 10, 10, 11, 12, 12,  8,  7,  9, 11, 11,
    10, 10, 11, 12, 12, 11, 11, 10,  7,  7,  8,  8,  7,  7,  7,  9, 10, 11,
    12, 12,  8,  7,  8, 11, 11, 10,  9, 10, 10, 10, 10, 10, 12, 12, 11,  9,
     6,  5,  5,  4,  4,  6,  6,  8,  9, 12, 12, 11, 12, 12, 12, 12,  8,  8,
     6,  6, 10, 10,  9, 12, 12, 11, 10, 10, 11, 12, 12, 10, 10, 10, 11, 12,
    12, 10, 11, 11,  9,  6,  6,  4,  6,  7,  8, 10, 10, 12, 12, 11, 12, 12,
    11,  7,  8,  8,  7, 10, 11, 12, 12,  9, 10, 11, 12, 12,  9,  4,  3,
};

static const uint8_t ac_vlc_desc8_bits[121] = {
     2,  9, 10, 12, 12, 11,  8,  9,  9,  8,  9, 11, 12, 12, 10,  9,  9,  7,
     7,  7,  8, 10, 10,  9,  7, 10, 10, 10, 11, 12, 12,  8,  8, 10, 10, 11,
    11, 10,  7,  7,  5,  5,  8,  8,  7, 10, 10, 10, 12, 12, 11, 10, 11, 12,
    12, 10, 11, 11, 11, 11, 12, 12, 11,  9,  8,  4,  4,  6,  6,  6,  6,  6,
     8, 12, 12, 11, 11, 11, 10, 12, 12, 11,  8,  9, 11, 11, 12, 12, 12, 12,
     6,  7, 10, 10, 10, 10,  9, 10, 12, 12, 11,  4,  6,  7,  8, 12, 12, 12,
    12, 10, 11, 12, 12, 10,  6,  7,  9,  9,  8,  4,  3,
};

static const uint8_t ac_vlc_desc9_bits[114] = {
     2,  6, 11, 11, 10,  9,  8,  8, 11, 12, 12, 11, 11,  9,  7,  8, 10, 10,
     9,  7,  8,  9, 11, 11, 12, 12, 11,  8,  9, 10, 10,  7,  8, 11, 11, 10,
     9, 11, 11, 10, 12, 12, 11, 10,  8,  5,  5,  7,  7,  6,  4,  6,  9, 11,
    11, 10,  8,  8,  8,  6,  9, 11, 12, 12, 12, 12, 11,  9, 11, 12, 12, 10,
     7,  4,  7,  7,  6,  6,  6,  4, 12, 12, 11, 10, 10, 11, 11,  9, 10, 10,
    11, 12, 12, 10, 10, 10,  8,  6,  8, 10, 12, 12, 12, 12, 10, 10,  8, 11,
    11, 10,  9,  6,  4,  3,
};

static const uint8_t ac_vlc_descA_bits[110] = {
     2,  9,  9,  8,  8, 12, 12, 11, 10,  9,  8, 10, 10,  9,  7,  6,  8,  9,
    11, 11, 12, 12, 11,  8,  9, 10, 11, 11,  7,  7,  8,  9,  9,  8, 10, 11,
    11, 10, 11, 12, 12,  5,  5,  6,  8,  9,  9,  8,  9, 10, 11, 12, 12,  4,
     6,  7,  7,  6,  7,  9, 10, 10, 11, 11, 10,  9,  4,  7,  8, 11, 11, 10,
    10, 10,  6,  7,  8, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11,  6,  4,  6,
    11, 12, 12, 11, 11,  9, 12, 12, 11, 12, 12, 11, 10, 10,  8,  8,  6,  6,
     4,  3,
};

static const uint8_t ac_vlc_descB_bits[101] = {
     2,  5,  8, 10, 10,  9,  7,  6,  5,  5, 11, 11, 10, 11, 11, 10,  8,  7,
     6,  9,  9,  8,  7,  6,  7,  7,  9,  9,  9,  9,  8,  8,  6, 10, 11, 12,
    12, 11, 11, 11, 11,  8,  8, 10, 10,  9,  4,  4,  6, 11, 12, 12, 10,  9,
    10, 11, 11, 10, 11, 11,  7,  7,  9, 11, 12, 12, 10,  9, 10, 10,  7,  8,
    12, 12, 11, 10, 10, 10,  4,  6,  6,  6, 10, 11, 12, 12,  9,  8, 11, 11,
    11, 11, 10, 12, 12, 12, 12,  9,  9,  4,  3,
};

static const uint8_t ac_vlc_descC_bits[96] = {
     2,  5, 10, 10,  9,  8,  8, 10, 10, 10, 12, 12, 11,  6,  5,  7,  7,  6,
     5,  6,  8,  8, 11, 11, 10, 11, 11, 10,  8,  7,  8,  8,  9, 10, 11, 11,
     8,  7,  7,  9,  9,  8,  6,  4,  4,  6, 10, 10,  9, 11, 11, 11, 11,  9,
    10, 12, 12, 11,  9,  9, 10, 10,  7,  7,  9, 10, 11, 12, 12, 10, 10, 10,
    10,  7,  4, 10, 11, 11,  9, 11, 11, 10,  9, 11, 12, 12, 11, 12, 12,  9,
     8,  6,  6,  6,  4,  3,
};

static const int tscc2_ac_vlc_sizes[NUM_VLC_SETS] = {
    172, 169, 165, 162, 131, 132, 130, 125, 121, 114, 110, 101, 96
};

static const uint16_t * const tscc2_ac_vlc_syms[NUM_VLC_SETS] = {
    ac_vlc_desc0_syms, ac_vlc_desc1_syms, ac_vlc_desc2_syms, ac_vlc_desc3_syms,
    ac_vlc_desc4_syms, ac_vlc_desc5_syms, ac_vlc_desc6_syms, ac_vlc_desc7_syms,
    ac_vlc_desc8_syms, ac_vlc_desc9_syms, ac_vlc_descA_syms, ac_vlc_descB_syms,
    ac_vlc_descC_syms,
};

static const uint8_t * const tscc2_ac_vlc_bits[NUM_VLC_SETS] = {
    ac_vlc_desc0_bits, ac_vlc_desc1_bits, ac_vlc_desc2_bits, ac_vlc_desc3_bits,
    ac_vlc_desc4_bits, ac_vlc_desc5_bits, ac_vlc_desc6_bits, ac_vlc_desc7_bits,
    ac_vlc_desc8_bits, ac_vlc_desc9_bits, ac_vlc_descA_bits, ac_vlc_descB_bits,
    ac_vlc_descC_bits,
};

#endif /* AVCODEC_TSCC2DATA_H */
